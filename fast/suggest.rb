#!/usr/bin/env ruby

# Suggest fast modifiers for rules.
#
# Very beta.
#
# This script should be given an IronBee rules file on standard in.  It will
# search for @rx or @dfa operators in those rules and analyize the regular
# expressions for appropriate fast patterns.  The rule file, plus annotation
# comments, will be emitted to standard out.
#
# This code is in a very early stage and has only partial support.  If it runs
# into part of a regular expression it doesn't understand, an exception will
# be thrown.  Please submit such exceptions, along with the rule, to the
# author.
#
# Requires `gem install regexp_parser`.
#
# Author: Christopher Alfeld <calfeld@qualys.com>

require 'rubygems'
require 'regexp_parser'
require 'pp'
require 'set'

FastString = Struct.new(:string, :case_insensitive)

# Map of certain regexp [] expressions to AC pattern equivalents.
# This may eventually go away if direct support for [] expressions is added
# to AC patterns.
KNOWN_SETS = {
  ['\r', '\n'].to_set => '\e'
}

# If true, outputs all found suggestions.  If false, chooses a best
# suggestion for each regexp.
SUGGEST_ALL = true

# A pattern is a set of FastStrings to be treated as an OR.  E.g.,
# ["foo", "bar"] means execute if "foo" or if "bar" is present.

# Evaluate if a rule is a candidate for an fast modifier.
def potential_rule(line)
  line =~ /\s@(rx|dfa) "/ &&   # Has a regexp
    line !~ /\bfast:/       &&   # Does not already have an fast
    line !~ /\st:/        &&   # Does not have a transformation
    true                       # Future expansion
end

# Format a FastString for output in a rule file.
def format_faststring(fs)
  s = fs.string
  s.gsub!(/"/, '\"')
  if fs.case_insensitive
    # This would be easier with look-behinds.
    last_char = nil
    s = s.split('').collect do |c|
      r = c
      if last_char != '\\' && c =~ /[A-Za-z]/
        r = "\\i#{c}"
      end
      last_char = c
      r
    end.join
  end
  s
end

# Format a suggestion (a set of FastStrings).
def format_suggestion(pattern)
  "# Suggest: " + pattern.collect {|x| "\"fast:#{format_faststring(x)}\""}.join(' ')
end

# Score a pattern.
#
# Minimum length of any component FastString.
def pattern_score(pattern)
  pattern.collect {|x| x.string.length}.min
end

# Select the best pattern.
#
# The one with the best score.
def select_best(patterns)
  patterns.max_by {|x| pattern_score(x)}
end

# Extract regular expressions from a Rule.
def extract_regexps(line)
  line.grep(/\s@(rx|dfa) "(.+?)"(\s|$)/) {$2}
end

# Evaluate if a suggestion is worth doing.
def valid_suggestion(pattern)
  pattern_score(pattern) >= 3
end

# Extract all possible patterns from a regular expression string.
#
# Result is array of arrays of FastStrings.
def extract_patterns(regexp)
  case_insensitive = false
  root = Regexp::Parser.parse(regexp)
  return [] if root.expressions.empty?
  first = root.expressions.first
  if first.token == :options
    if first.options[:x] || first.options[:m]
      STDERR.puts "SKIP -- Can't handle options."
      return []
    end
    if first.options[:i]
      case_insensitive = true
    end
    root.expressions.shift
  end

  extract_patterns_from_parse(root, case_insensitive)
end

# Extract all possible patterns from a parsed regular expression.
#
# case_insensitive determines the case insensitivity of all generated
# FastString's.
#
# This function is recursive.  See extract_patterns() for the top.
def extract_patterns_from_parse(node, case_insensitive)
  # Can't extract anything from quantified expressions.
  return [] if ! node.quantifier.nil?

  case node.type
  when :expression, :group
    if node.token == :options
      STDERR.puts "ERROR -- Can't handle options outside of beginning."
      return []
    end
    result = []
    current = ""
    emit = lambda do
      result << [FastString.new(current, case_insensitive)] if current != ""
      current = ""
    end
    todo = []
    node.each {|x| todo << x}
    while ! todo.empty?
      sub = todo.shift
      if ! sub.quantifier.nil?
        emit[]
        next
      end

      case sub.type
      when :group
        # We know the group is non-quantified, so just prepend it.
        toadd = []
        sub.each {|x| toadd << x}
        toadd.reverse_each {|x| todo.unshift(x)}
      when :literal
        current += sub.text
      when :escape
        current += sub.text[1..1]
      when :anchor
        emit[]
      when :meta
        case sub.token
        when :alternation
          emit[]
          result.concat(extract_patterns_from_parse(sub, case_insensitive))
        when :dot
          current += '\.'
        else
          raise "Unhandled meta token in expression: #{meta.token}"
        end
      when :set
        as_set = sub.members.to_set
        if KNOWN_SETS[as_set]
          current += KNOWN_SETS[as_set]
        else
          emit[]
        end
      when :assertion
        case sub.token
        when :nlookahead
          emit[]
        else
          raise "Unhandled assertion token in expression: #{sub.token}"
        end
      else
        raise "Unhandled node type in expression: #{sub.type}"
      end
    end
    emit[]
    return result
  when :meta
    case node.token
    when :alternation
      subpatterns = node.expressions.collect {|x| extract_patterns_from_parse(x, case_insensitive)}.select {|x| !x.empty?}
      # If too many, give up
      return [] if subpatterns.size > 3 || subpatterns.empty?
      # If any subpattern has multiple patterns, give up.
      return [] if subpatterns.collect(&:size).max > 1
      # Merge subpatterns.
      r = subpatterns.shift[0]
      while ! subpatterns.empty?
        r.concat(subpatterns.shift[0])
      end
      return [r]
    else
      raise "Unhandle meta token: #{node.token}"
    end
  when :anchor, :set, :assertion
    return []
  else
    raise "Unhandled node type: #{node.type}"
  end
  raise "Reached code it should be impossible to reach."
end

# Main loop.
STDIN.each do |line|
  if potential_rule(line)
    res = extract_regexps(line)
    res.each do |re|
      patterns = extract_patterns(re).select {|x| valid_suggestion(x)}
      next if patterns.empty?
      if SUGGEST_ALL
        patterns.each do |pattern|
          puts format_suggestion(pattern)
        end
      else
        puts format_suggestion(select_best(patterns))
      end
    end
  end
  puts line
end
