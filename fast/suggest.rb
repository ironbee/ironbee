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

rx_mode = ARGV.member?('--rx')
lua_mode = ARGV.member?('--lua')
$comment = lua_mode ? '--' : '#'

# Evaluate if a rule is a candidate for an fast modifier.
def potential_rule(line)
  line =~ /\s@(rx|dfa) / &&   # Has a regexp
    line !~ /\bfast:/    &&   # Does not already have an fast
    line !~ /\st:/       &&   # Does not have a transformation
    line =~ FIELD_RE          # Involves a field fast knows about.
end

# Extract regular expressions from a Rule.
def extract_regexps(line)
  r = []
  line.scan(/\s@(rx|dfa) (?:"((?:\\"|[^"])+)"|([^ ]+?))(\s|$)/) {r << $2}
  r
end

# Format pattern for inclusion in rule.
def format_patterns(patterns)
  patterns.collect {|x| "\"fast:#{x}\""}.join(' ')
end
def format_patterns_lua(patterns)
  patterns.collect {|x| "action(\"fast:#{x}\")"}.join(':')
end

# Is this pattern considered valid?
def valid_pattern(s)
  s.length > 3
end

# Main loop.
lua_eligible = false
STDIN.each do |line|
  begin
    if rx_mode
      res = [line.chomp]
    elsif lua_mode
      res = []
      if line =~ /^\s*Sig|Action/
        lua_eligible = false
      elsif line =~ /\bfields?\((.+)\)/
        if $1 =~ /[A-Za-z_]+/
          if FIELDS.member?($&)
            lua_eligible = true
          end
        end
      elsif lua_eligible && line =~ /\bop\(\s*(?:'|"|\[=*\[)rx(?:'|"|\]=*\])\s*,\s*(?:'|"|\[=*\[)(.+)(?:'|"|\]=*\])/
        res = [$1]
      end
    elsif potential_rule(line)
      res = extract_regexps(line)
    else
      res = []
    end
  rescue Exception => err
    puts "#{$comment} FAST Error looking at line: #{err}"
    puts line
    res = []
  end

  res.each do |re|
    if re.nil?
      puts "#{$comment} FAST Failed to extract regexp.  Malformed rule?"
      next
    end

    # Validate with ruby.
    begin
      Regexp.new(re)
    rescue Exception => err
      puts "#{$comment} FAST Ruby Says: #{err}"
    end

    begin
      result = ReToAC::extract(re, MAX_ALTERNATIONS, MAX_REPETITIONS)
    rescue Exception => err
      puts "#{$comment} FAST Exception: #{err}"
      result = []
    end
    next if result.empty?

    # Filter out invalid patterns.
    result = result.collect {|r| r.select {|s| valid_pattern(s)}}

    # If any row is now empty, we are done.
    next if result.find {|r| r.empty?}

    suggestion = ReToAC::craft_suggestion(result)
    if suggestion
      puts "#{$comment} FAST RE: #{re}"
      if lua_mode
        puts "#{$comment} FAST Suggest: #{format_patterns_lua(suggestion)}"
      else
        puts "#{$comment} FAST Suggest: #{format_patterns(suggestion)}"
      end
      if result.size > 1 || result.first.size > 1
        puts "#{$comment} FAST Result Table: "
        puts "#{$comment} FAST " +
          (result.collect do |row|
            '( ' + row.join(' AND ') + ' )'
          end.join(" OR\n# FAST "))
      end
    end
  end
  puts line
end
