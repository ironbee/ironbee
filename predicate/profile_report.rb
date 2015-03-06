#!/usr/bin/env ruby

# Generate a report from predicate profiling information.
# See documentation for the configuration directives:
# - PredicateProfile
# - PredicateProfileDir
#
# Author: Sam Baskinger <sbaskinger@qualys.com>
#

require 'logger'


class MyLogDev
  def write(s)
    puts s
  end

  def close
  end
end

# Helper class to parse S-Expressions.
class SExpr
  # The raw s_exp given t the constructor.
  attr_reader :raw

  # The operator represented by this.
  # In the case of leaf nodes, this equals @raw.
  attr_reader :op

  # Child s-expressions.
  attr_reader :children

  def initialize(s_exp)
    parse(s_exp)
  end

  def parse_child(s_exp, offset = 0)
  end

  # Parse the given string +s_exp+ into this SExp.
  def parse(s_exp, offset = 0)

    # Find start-and-stop of s-expression.
    i = s_exp.index('(')
    j = i + 1
    depth = 1

    @raw = ''
    @op = ''
    @children = []

    # If we match a (, this is an s-expression. Recursively parse bits.
    if (/\s*\(/.match(s_exp, offset))
      offset = offset + $~[0].length
      unless /\s*(\w+)\s*/.match(s_exp, offset)
        raise RuntimeError.new("Cannot parse op for "+s_exp)
      end

      # Record the captured op and move the offset.
      @op = s_exp[offst, $~[1].length]
      offset = offset + $~[0].length

      while s_exp[offset] != nil && s_exp[offset] != ')' do
        child_s_expr, offset = parse_child(s_exp, offset)
        @children << child_s_expr
      end
    end

    while j < s_exp.length && depth > 0
      # Eat quoted strings and associated white space.
      if /\s*
          (?:
              '(?: [^']|\\\'|\\\\)+' | # Match strings.
              "(?: [^"]|\\\"|\\\\)+"   # Match strings.
          )\s*
         /x.match(s_exp, j)

         # If we find quoted stuff, we don't care. Next!
         j = j + $~[0].length

      # If we find a ( not in a quoted string, add to depth.
      elsif /\s+\(/.match(s_exp, j)
        depth = depth + 1
      # If we find a ) not in a quoted string, remove from depth.
      elsif /\s+\)/.match(s_exp, j)
        depth = depth - 1
      end
    end

    # We've found the start of an s-expression.
    if (depth == 0 && i && j)
      @raw = s_exp # store the raw
      @children = []

      # Strip off the () and split the string.
      @op, args = @raw[(i+1)...j].strip.split(/\s+/, 2)

      # Parse each child out and parse it as an SExp.
      # Args has no leading white space.
      while args && args.length > 0 do
        $LOG.debug { "Checking child string #{args}."}
      end

    # This is an atom / leaf.
    else
      @raw = s_exp # Store the raw.
      @op  = s_exp # Val = raw, in this case.
      @children = [] # No kids. Done.
    end
  end
end

$LOG = Logger.new(MyLogDev.new)
$LOG.debug { "Starting." }

ARGV.each do |file_name|
  $LOG.debug { "Opening #{file_name}." }

  File.open file_name do |io|
    while data = io.read(4) do

      len = data.unpack("L")[0]
      name = io.readline("\0")
      puts "#{len} #{name}"
      SExpr.new(name)
    end
  end

  $LOG.debug { "Closing #{file_name}." }
end