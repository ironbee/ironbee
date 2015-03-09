#!/usr/bin/env ruby
#
# A library to parse S-Expressions.
#
# Author: Sam Baskinger <sbaskinger@qualys.com>
#


# Helper class to parse S-Expressions.
class SExpr
  # The raw s_exp given t the constructor.
  attr_reader :raw

  # The operator represented by this.
  # In the case of leaf nodes, this equals @raw.
  attr_reader :op

  # Child s-expressions.
  attr_reader :children

  # S-expressions that we are part of.
  # Note that parsing will only generate a single parent or no parent
  # but merging may show additional parents.
  attr_reader :parents

  # User data.
  #
  # The user may annotate this node with any single
  # data object they would like.
  attr_accessor :data

  # Constructor
  #
  # This should not be used to parse nodes, but to build them.
  # The children are modified in that their parent list has self added to it.
  #
  def initialize(op, raw, children)
    @op = op
    @raw = raw
    @children = children.map { |c| c.parents << self ; c }
    @parents = []
  end

  # Add +c+ as one of this node's children and this node as one of +c's+ parent.
  def add_child(c)
    @children << c
    c.parents << self
  end

  # Add +p+ as one of this node's parents and this node as one of +p's+ child.
  def add_parent(p)
    @parents << p
    p.children << self
  end

  # Parse a quoted string into a leaf SExpr.
  #
  # Returns an array of 1 string and 1 SExpr..
  #
  # +return[0]+:: The consumed string.
  # +return[1]+:: The matched, parsed string, including quotes.
  # Returns +nil+ on no match.
  def self.parse_string(exp)
      if /^\s*                         # Start with white space.
          (?:
              '(?: [^']|\\\'|\\\\)+' | # Match strings.
              "(?: [^"]|\\\"|\\\\)+"   # Match strings.
          )\s*
         /x.match(exp)

        SExpr.new($~[1], $~[0], [])
      else
        nil
      end # if
  end

  # Parse a single word as an operator.
  def self.parse_op(exp)
    if /^\s*(\w+)\s+/.match(exp)
      [ $~[0], $~[1] ]
    else
      nil
    end
  end

  # Parse an S Expression.
  # Returns nil on failure.
  # On success, returns a 2-element array.
  # +return[0]+::The matched text.
  # +return[1]+::An SExp object.
  def self.parse_s_exp(exp)

    # If we find a starting (, try to parse this.
    if /\s*\(\s*/.match(exp)

      # Keep a copy.
      offset = $~[0].length
      children = []

      # Parse or fail
      all, op = parse_op(exp[offset..-1])
      return nil unless all

      offset = offset + all.length

      # Parse args.
      while not /^\s*\)/.match(exp[offset..-1]) do
        subexp = parse_one exp[offset..-1]
        if subexp
          offset = offset + subexp.raw.length
          children << subexp
        else
          raise RuntimeError.new("Could not parse: "+exp[offset..-1])
        end
      end

      offset = offset + $~[0].length
      raw = exp[0 ... offset]
      SExpr.new(op, raw, children)
    else
      nil
    end
  end

  # This will try to parse the given string into an SExpr.
  #
  # Parsing will start at +offset+ and stop on a match.
  #
  # Return [ match, sexpr ] or nil on failure.
  def self.parse_one(exp)

    # Try parsing a string.
    subexp = parse_string exp
    return subexp if subexp

    # Try parsing this as a sub-s-expression.
    subexp = parse_s_exp exp
    return subexp if subexp

    nil
  end

  def self.parse(exp)
    parse_one(exp)
  end

  def to_s
    if children.length == 0
      "%s"%[ @raw.strip ]
    else
      "(%s %s)"%[ @op, @children.map(&:to_s).join(" ")]
    end
  end

  alias id to_s
  def hash
    id.hash
  end
  def ==(other)
    id == other.id
  end
  alias eql? ==
end

