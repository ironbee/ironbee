#!/usr/bin/env ruby

# Looks at pairs of lines, A and B, such that:
# - Neither contains a { or }
# - A does not end in (
# - A contains more ( than )
# - B is not indented to just past the last unmatched (

$:.unshift(File.dirname(__FILE__))
require 'all-code'

PATH_EXCEPTIONS = [
  %r{^cli/},
  /rule_dev.c/,
  /ahocorasick/
]
PREVIOUS_EXCEPTIONS = [
  # General.
  /ASSERT_EQ/,
  /^\s+\(/,
  %r{^\s*//},
  %r{^\s*/\*},
  %r{^\s+\*},

  # Specific.
  %r{^[^(]+\( \(},
  %r{^\s+"\(},
  /for \(;/,
  /^\s+if \( /,
  /\("/
]

def last_unmatched_lp(s)
  i = s.length
  depth = 0
  while i > 0
    last_p = s.rindex(/[()]/, i)
    return nil if ! last_p
    if s[last_p..last_p] == '('
      depth -= 1
      return last_p if depth < 0
    else # )
      depth += 1
    end
    i = last_p - 1
  end
  nil
end

all_ironbee_code do |path|
  next if PATH_EXCEPTIONS.find {|x| path =~ x}

  previous = nil
  current  = nil

  n = 0
  IO::foreach(path) do |line|
    n += 1
    previous = current
    current  = line.chomp.rstrip

    next if ! previous || PREVIOUS_EXCEPTIONS.find {|x| previous =~ x}
    next if previous =~ [{}] || current =~ /[{}]/
    next if previous =~ /\($/
    next if previous.count('(') <= previous.count(')')
    desired_indent = last_unmatched_lp(previous) + 1
    actual_indent  = current.index(/[^ ]/)
    if desired_indent != actual_indent
      # For the moment, ignore if indent of current is 4 more than indent of
      # previous
      next if previous.index(/[^ ]/) + 4 == actual_indent
      prefix = "#{path}:#{n}: "
      puts prefix + previous
      puts (" " * prefix.length) + current
    end
  end
end
