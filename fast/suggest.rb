#!/usr/bin/env ruby

# Suggest fast modifiers for rules.
#
# This script should be given an IronBee rules file on standard in.  It will
# search for @rx or @dfa operators in those rules and analyize the regular
# expressions for appropriate fast patterns.  The rule file, plus annotation
# comments, will be emitted to standard out.
#
# Requires `gem install regexp_parser`.
#
# Author: Christopher Alfeld <calfeld@qualys.com>

$:.unshift(File.dirname(__FILE__))
require 're_to_ac'

# Don't try to handle any alternation expression (a|b|c) with more than this
# many alternatives, including subexpressions.
MAX_ALTERNATIONS = 5

# Any expression that repreats more than this many types; repeat only this
# many times.  E.g., x{20} will be treated as x{10}.
MAX_REPETITIONS  = 10

# Fields that are covered by fast.
FIELDS = [
  'REQUEST_METHOD',
  'REQUEST_URI',
  'REQUEST_PROTOCOL',
  'REQUEST_HEADERS',
  'REQUEST_URI_PARAMS',
  'RESPONSE_PROTOCOL',
  'RESPONSE_STATUS',
  'RESPONSE_MESSAGE',
  'RESPONSE_HEADERS',
  'ARGS'
]
FIELD_RE = Regexp.new('\b(' + FIELDS.join('|') + ')[^A-Za-z]')

rx_mode = (ARGV[0] == '--rx')

# Evaluate if a rule is a candidate for an fast modifier.
def potential_rule(line)
  line =~ /\s@(rx|dfa) / &&   # Has a regexp
    line !~ /\bfast:/    &&   # Does not already have an fast
    line !~ /\st:/       &&   # Does not have a transformation
    line =~ FIELD_RE          # Involves a field fast knows about.
end

# Extract regular expressions from a Rule.
def extract_regexps(line)
  line.grep(/\s@(rx|dfa) ("[^"]+?"|[^ ]+?)(\s|$)/) {$2}
end

# Format pattern for inclusion in rule.
def format_patterns(patterns)
  patterns.collect {|x| "\"fast:#{x}\""}.join(' ')
end

# Is this pattern considered valid?
def valid_pattern(s)
  s.length > 3
end

# Main loop.
STDIN.each do |line|
  if rx_mode
    res = [line.chomp]
  elsif potential_rule(line)
    res = extract_regexps(line)
  else
    res = []
  end

  res.each do |re|
    begin
      result = ReToAC::extract(re, MAX_ALTERNATIONS, MAX_REPETITIONS)
    rescue Exception => err
      puts "# FAST Exception: #{err}"
      result = []
    end
    next if result.empty?

    # Filter out invalid patterns.
    result = result.collect {|r| r.select {|s| valid_pattern(s)}}

    # If any row is now empty, we are done.
    next if result.find {|r| r.empty?}

    suggestion = ReToAC::craft_suggestion(result)
    if suggestion
      puts "# FAST RE: #{re}"
      puts "# FAST Suggest: #{format_patterns(suggestion)}"
      if result.size > 1 || result.first.size > 1
        puts "# FAST Result Table: "
        puts "# FAST " +
          (result.collect do |row|
            '( ' + row.join(' AND ') + ' )'
          end.join(" OR\n# FAST "))
      end
    end
  end
  puts line
end
