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

  # Add +p+ as one of this node's parents and this node as one of +p's+ child.
  def add_parent(p)
    @parents << p
    p.children << self
  end

  # Merge +that+ SExpr into this node.
  #
  # This assumes that +self.id+ and +that.id+ are equal.
  #
  # If this is indeed true, then both nodes have
  # identical subtrees and may be merged.
  #
  # When two nodes are merged, if +self.data+ responeds to +merge!+,
  # it is called with +that.data+ as its argument.
  #
  # +merge!+ is then recursively called on all children.
  def merge!(that)
    @data.merge that.data if @data.responds_to? :merge!

    @children.each_with_index do |child, i|
      child.merge! that.children[i]
    end
  end

  # Apply the given block in a depth-first way to all nodes
  # including this node.
  def depth_first(&y)
    y.call(self)
    y.children.each { |c| c.depth_first y }
  end

  # Parse a quoted string into a leaf SExpr.
  #
  # Returns an array of 1 string and 1 SExpr..
  #
  # Returns +nil+ on no match.
  def self.parse_string(exp)
      if /^\s*                         # Start with white space.
          (
              :                     | # Null value.
              -?\d+                 | # Integer literal.
              '(?: \\\\|\\.|[^'])*' | # Match strings.
              "(?: \\\\|\\.|[^"])*"   # Match strings.
          )\s*
         /x.match(exp)
        SExpr.new($~[1], $~[0], [])
      else
        nil
      end # if
  end

  # Parse a single word as an operator.
  def self.parse_op(exp)
    if /^\s*(\w+)\s*/.match(exp)
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
  def self.parse_one(exp)
    s = parse_string exp

    s = parse_s_exp exp unless s

    s
  end

  # Build a tree from the given s-expression.
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

  # Allow use as a hash key.
  def hash
    id.hash
  end

  # Allow use as a hash key.
  def ==(other)
    if other.is_a? String
      id == other
    else
      id == other.id
    end
  end
  alias eql?  ==
end

# An S-Expression parser that uses native C code.
module SExprNative
  @@loaded = false

  def self.load
    return if @@loaded
    @@loaded = true
    self.class_eval do
      require 'ffi'
      extend FFI::Library
      ffi_lib_flags :now, :global

      ffi_lib "libibutil.so"
      ffi_lib "libironbee.so"
      ffi_lib "libibpp.so"
      ffi_lib "libpredicate_c_api.so"
    end
  end

  def self.attach
    FFI::typedef :void, :ib_predicate_node_t

    attach_function :parse_call, :ib_predicate_parse, [:pointer, :pointer ], :uint8
    attach_function :destroy, :ib_predicate_node_destroy, [ :pointer ], :void
    attach_function :str, :ib_predicate_node_to_s, [ :pointer ], :string
    attach_function :child_count, :ib_predicate_node_child_count, [ :pointer ], :size_t
    attach_function :name, :ib_predicate_node_name, [ :pointer ], :string
    attach_function :children, :ib_predicate_node_children, [ :pointer, :pointer ], :void
    attach_function :is_literal, :ib_predicate_node_is_literal, [ :pointer ], :bool
  end

  # Parse an s expression into a call tree.
  #
  # If no block is given then the FFI object is returned
  # and the user must call SExprNative::destroy on it.
  #
  # Otherwise, the block is run, being passed the FFI object, and that
  # object is automatically cleaned up.
  def self.for_node expr
    n = FFI::MemoryPointer.new :pointer

    r = self.parse_call(n, expr)

    raise RuntimeError.new("Could not parse expression.") unless r == 0

    # Unwrap the pointer-pointer.
    n = n.read_pointer

    if block_given?
      begin
        yield n
      ensure
        self::destroy n
        nil
      end
    else
      n
    end
  end

  def self.build_node n
    c = []

    c_children_count = child_count(n)

    if c_children_count > 0

      c_children = FFI::MemoryPointer.new :pointer, c_children_count
      children n, c_children

      c_children_count.times do |i|
        c << build_node(c_children[i])
      end
    end

    if is_literal n
      SExpr.new str(n), str(n), c
    else
      SExpr.new name(n), str(n), c
    end
  end

  # Parse an expression and return an SExpr.
  def self.parse expr
    for_node expr do |n|
      build_node n
    end
  end
end