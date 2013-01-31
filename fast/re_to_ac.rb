#!/usr/bin/env ruby

# Extract Aho-Corasick Patterns from regular expressions.
#
# See ReToAC::extract()
#
# This file is intended to be used as a library.  If run as a script it
# looks for 1 regexp per line on STDIN and outputs the correspond DNF.

require 'rubygems'
require 'regexp_parser'
require 'set'
require 'pp'

# See ReToAC::extract().
module ReToAC
  # Extraction

  module Impl
    # The result of extraction on a node.
    #
    # The extraction of a node can be:
    #
    # - complete: The pattern represent the entire node.
    # - prefix: The pattern represents a prefix of the node.
    # - suffix: The pattern represents a suffix of the node.
    # - internals: These patterns represent portions of the node that are not
    #   complete, prefixes, or suffixes.
    #
    # If there is a complete pattern, there are no others.
    #
    # The main logic is in concat() which takes the extraction results of two
    # adjacent nodes and combines them.
    class ExtractionResult
      # A pattern representing the entire node.
      attr_accessor :complete
      # A pattern representing a prefix of the node.
      attr_accessor :prefix
      # A pattern representing the suffix of the node.
      attr_accessor :suffix
      # Patterns contained in the node but none of the above.
      attr_accessor :internals

      # Constructor.
      def initialize
        @internals = []
      end

      # Convert to string for debugging purposes.
      def to_s
        r = []
        r << "complete: #{complete}" if complete
        r << "prefix: #{prefix}" if prefix
        r << "suffix: #{suffix}" if suffix
        internals.each {|x| r << "internal: #{x}"}
        r.join(' ')
      end

      # Concatenate with another result.
      #
      # For C = A + B.  Each group represents first valid.
      #
      # C.complete =
      # - A.complete & B.complete: A.complete + B.complete
      # - else: nil
      #
      # C.prefix =
      # - A.complete & B.prefix: A complete + B.prefix
      # - A.complete: A.complete
      # - else: A.prefix
      #
      # C.suffix =
      # - A.suffix & B.complete: A.suffix + B.complete
      # - B.complete: B.complete
      # - else: B.suffix
      #
      # C.internals = A.internals union B.internals and
      # - A.suffix & B.prefix: A.suffix + B.prefix
      # - A.suffix: A.sufix
      # - B.prefix: B.prefix
      # - else: nil
      def concat(other)
        result = ExtractionResult.new

        # Complete
        if complete && other.complete
          result.complete = complete.concat(other.complete)
        end

        # Prefix
        if complete
          if other.prefix
            result.prefix = complete.concat(other.prefix)
          elsif ! other.complete
            result.prefix = complete
          end
        else
          result.prefix = prefix
        end

        # Suffix
        if other.complete
          if suffix
            result.suffix = suffix.concat(other.complete)
          elsif ! complete
            result.suffix = other.complete
          end
        else
          result.suffix = other.suffix
        end

        # Internals
        result.internals = internals + other.internals
        if suffix && other.prefix
          result.internals << suffix.concat(other.prefix)
        elsif suffix
          result.internals << suffix
        elsif other.prefix
          result.internals << other.prefix
        end

        result
      end
    end

    # Do all the work.
    #
    # See ReToFast::extract() for details.
    def self.extract(top, max_alternations = nil, max_min = nil)
      # Eliminate quantified nodes.
      expand_quantifiers(top, max_min)

      # Build alternative map.
      amap = AlternativeMap.new(top, max_alternations)

      # Global case insensitivity.  This can be altered by (?i) and (?-i)
      # expressions.
      global_case_insensitive = false

      # The following lambda is used to recursively walk the parse tree.  It
      # takes a node and the local case insensitivity (see below) and returns
      # the ExtractionResult for that node.

      # case_insensitive:
      # nil - use global_case_insensitive
      # true - force case insensitive
      # false - force case sensitive
      walk = lambda do |node, case_insensitive|
        result = ExtractionResult.new
        node = amap[node]
        return result if ! node

        case node.type

        when :expression, :group, :assertion
          if node.token == :options
            if node.options[:x] || node.options[:m]
              raise "ERROR -- x and m options not supported."
            end
            if node.text[-1..-1] == ":"
              # Scoped
              case_insensitive = node.options[:i]
            else
              # Global
              global_case_insensitive = node.options[:i]
            end
          end

          # Don't handle negative assertions.
          if node.type == :assertion
            if node.token == :nlookahead || node.token == :nlookbehind
              return result
            elsif node.token != :lookahead && node.token != :lookbehind
              raise "Unrecognized assertion token: #{node.token}"
            end
          end

          # Special case
          if node.expressions.empty?
            result.complete = ""
          else
            todo = []
            node.each {|x| todo << x}
            # Special case first
            result = walk[todo.shift, case_insensitive]
            todo.each do |subnode|
              subresult = walk[subnode, case_insensitive]
              result = result.concat(subresult)
            end
          end
        when :literal
          actual_case_insensitive = case_insensitive
          if case_insensitive.nil?
            actual_case_insensitive = global_case_insensitive
          end
          result.complete = escape_literal(node.text, actual_case_insensitive)

        when :meta
          case node.token
          when :dot
            result.complete = '\.'
          else
            pp node
            raise "Unsupported meta node.token = #{node.token}"
          end

        when :escape
          case node.token
          when :zero_or_more, :zero_or_one, :one_or_more,
               :group_open, :group_close, :interval_open, :interval_close,
               :dot, :beginning_of_line, :end_of_line, :alternation
            result.complete = node.text[1..1]
          when :set_open, :set_close, :backslash, :escape, :form_feed,
               :newline, :carriage, :tab, :vertical_tab,
               :hex
            result.complete = node.text.dup
          when :literal
            raise "Arbitrary escaped literals not supported: #{node.text}"
          when :bell
            result.complete = '\^G'
          when :control
            c = node.text[2..2]
            if c =~ /[A-Z\\\\\[\]^_?]/
              result.complete = '\^' + node.text[2..2]
            else
              # This code currently doesn't work as Regexp::Scanner does not
              # properly handle these codes.  That will hopefully be fixed
              # eventually.
              c.upcase!
              raise "Controls above 127 not supported." if c[0] > 127
              result.complete = "\\x%02x" % [c[0] ^ 0x40]
            end
          else
            pp node
            raise "Unsupported escape node.token = #{node.token}"
          end

        when :type
          case node.token
          when :digit, :nondigit, :space, :nonspace, :word, :nonword
            result.complete = node.text.dup
          else
            pp node
            raise "Unsupported type node.token = #{node.token}"
          end

        when :anchor
          # Do nothing

        when :set
          # This doesn't always work as there may be escapes in the members
          # that is different than AC pattern escapes.  But it's a good start.
          result.complete = '[' + (node.negated? ? '^' : '') + node.members.join + ']'

        else
          pp node
          raise "Unsupported node.type = #{node.type}"
        end

        # Process min and max
        if result.complete && node.type == :expression && node.token == :repeated
          min, max = repetitions(node.actual)
          if min != max
            result.prefix = result.complete
            result.suffix = result.complete
            result.complete = nil
          end
        end

        result
      end

      # Walk every alternative and process results.
      amap.collect do
        result = walk[top, nil]
        candidates = ([result.complete] + [result.prefix] + [result.suffix] + result.internals).compact.to_set.to_a
        candidates.select do |a|
          candidates.find {|b| a != b && b.include?(a)}.nil?
        end
      end
    end

    # Deep dup of a parse node.
    def self.deep_dup(n)
      x = n.dup
      xexprs = n.expressions.collect {|y| deep_dup(y)}
      x.instance_variable_set(:@expressions, xexprs)
      x
    end

    # Escape double quotes in s and \i to each letter if case_insensitive.
    def self.escape_literal(s, case_insensitive)
      s = s.gsub(/"/, '\"')
      if case_insensitive
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

    # min and max quantification for node.
    #
    # Is [1, 1] for unquantified nodes.
    def self.repetitions(node)
      if ! node.quantifier.nil?
        [node.quantifier.min, node.quantifier.max]
      else
        [1, 1]
      end
    end

    # Format a node for debugging.
    #
    # Pass in AlternativeMap as second parameter if using.  Third parmeter
    # specifies indentation level.
    def self.fmt_node(n, amap = nil, d = 0)
      n = amap[n] if amap
      if n.nil?
        return ('  ' * d) + "void\n"
      end
      if n.type == :literal and n.token == :literal
        return (' ' * d) + n.text + "\n"
      end
      r = (' ' * d) + "#{n.type}/#{n.token}"
      r += "[#{n.text}]" if ! n.text.empty?
      r += "\n"
      if ! n.expressions.empty?
        n.expressions.each do |s|
          r += fmt_node(s, amap, d + 2)
        end
      end
      r
    end

    # True iff n is an alternation node.
    def self.is_alternation(n)
      n.type == :meta && n.token == :alternation
    end

    # Calculate and iterate through all alternatives of a parse tree.
    #
    # Does not work correctly with quantifiers.  Eliminate them first!
    #
    # To use, create for a given parse tree and then, when using tree,
    # use amap[node] instead of node.
    class AlternativeMap
      include Enumerable

      # Top node.
      attr_reader :top

      # Constructor.
      #
      # If max_alternatives is provided, any alternation node with more than
      # that many alternatives will be replaced by a "void" node, i.e., nil.
      def initialize(top_node, max_alternatives = nil)
        @top = top_node
        @map = {}
        @voids = Set.new

        countmap = {}

        walk = lambda do |n|
          if Impl::is_alternation(n)
            if max_alternatives && Impl::count_alternatives(n, countmap) > max_alternatives
              @voids << n
            else
              @map[n] = 0
            end
          end
          n.expressions.each {|sub| walk[sub]}
        end

        walk[top_node]
      end

      # Calculate actual node for a given node.
      #
      # If node is a "void" node, this returns nil.
      #
      # Otherwise, if node is an alternation node, this chooses the
      # appropriate child.
      #
      # Otherwise, returns node.
      def [](node)
        return nil if @voids.member?(node)
        if @map[node]
          node.expressions[@map[node]]
        else
          node
        end
      end

      # Advance to the next alternative; return true iff success.
      #
      # See also each.
      def advance(node = top)
        rnode = self[node]
        return false if ! rnode
        rnode.expressions.each do |sub|
          return true if advance(sub)
        end
        # Failed to do anything with children, try self.
        if Impl::is_alternation(node)
          @map[node] += 1
          if @map[node] == node.expressions.size
            @map[node] = 0
            false
          else
            true
          end
        else
          false
        end
      end

      # Call given block for every alternative.  No parameters passed.
      def each
        while true do
          yield
          break if ! advance
        end
      end
    end

    # Calculate a lower bound on the number of alternatives of node.
    #
    # Bound assumes any quantifier expression is repreated the minimum number
    # of times.
    #
    # A hash may be provided as the second parameter.  If present, it is used
    # to memoize intermediate results.  If you expect to call this for various
    # nodes in a tree, using such a cache can significantly improve
    # performance.
    def self.count_alternatives(node, cache = {})
      cache[node] ||= if node.type == :expression || node.type == :group
        if node.token == :options
          raise "Options not supported."
        end

        node.expressions.inject(1) {|m,x| m*(count_alternatives(x)**Impl::repetitions(x)[0])}
      elsif node.type == :meta && node.token == :alternation
        node.expressions.inject(0) {|m,x| m+count_alternatives(x)*Impl::repetitions(x)[0]}
      else
        1
      end
    end

    # Special parse node representing an expand quantified node.
    #
    # See expand_quantifiers.
    class RepeatedExpression < Regexp::Expression::Base
      attr_reader :actual
      attr_reader :repetitions

      def initialize(actual, repetitions)
        super(Regexp::Token.new(:expression, :repeated, ''))
        @actual = actual
        @repetitions = repetitions

        (1..repetitions).each {self << Impl::deep_dup(actual)}
      end
    end

    # Eliminate quantifiers in parse tree given under node.
    #
    # Replaces all quantified descendents (min, max) with RepeatedExpression
    # nodes with min (possibly 0) copies of the node with min=max=1
    # quantifiers.
    #
    # Any node with a min quantification greater than max_min uses max_min
    # instead.
    #
    # Any node with min=max=1 is left alone.
    #
    # The top node itself is not replaced.
    def self.expand_quantifiers(node, max_min = nil)
      newexprs = []
      node.expressions.each do |n|
        expand_quantifiers(n, max_min)
        min = nil
        max = nil
        if n.quantified?
          min, max = n.quantity
        end
        if min.nil? || (min == 1 && max == 1)
          newexprs << n
          next
        end
        min = max_min if ! max_min.nil? && max_min < min
        min = 0 if min < 0

        newexprs << RepeatedExpression.new(n, min)
      end
      # Hack
      node.instance_variable_set(:@expressions, newexprs)
    end
  end

  # Analyze regular expression for necessary DNF of AC patterns.
  #
  # The result is a boolean expression in disjunctive normal form represented
  # is an array of arrays of strings.  E.g., [['a', 'b'], ['c']] means
  # ('a' AND 'b') OR ('c'), i.e., that the regexp will only match if either
  # 'a' and 'b' are in the input or 'c' is in the input.
  #
  # The motivation for this code was to extract AC patterns to select which
  # regexps to run on an input.  For that problem, each clause is simplfied
  # to a single "best" member.  I.e., for the above example, evaluate the
  # regexp if either 'a' or 'c' appears in the input.
  #
  # AC Patterns are defined by the IronAutomata Aho-Corasick generator.  See
  # that documentation for details.  In summary, they are patterns similar to
  # regular expressions except allowing for only fixed and non-zero width
  # expressions.  In particular, they lack quantifiers or alternation.
  #
  # This code works, more or less, in three stages:
  #
  # 1. Quantifiers are eliminated by expanding the quantified expression to
  #    its minimum number of repeats.  E.g., 'a+' becomes 'a'; 'a{3,10}'
  #    becomes 'aaa', and 'a*' becomes a special "void" expression.  The
  #    max_min parameter can be used to limit this expansion.  Any quantifier
  #    that would be expanded to more than max_min copies will instead be
  #    expanded to max_min copies.
  #
  # 2. Alternations are eliminated by replacing an alternating expression with
  #    a set of possibilities.  E.g., 'a|b' becomes 'a', 'b'; '(a|b){2}'
  #    becomes 'aa', 'ab', 'ba', 'bb'.  The max_alterantions parameter can be
  #    used to limit this expansion.  Any alternation expression with more
  #    than max_alternations possibilities (including subalternations) will
  #    be replaced with a "void" expression.
  #
  # Note: You generally want to use both.  For example, '(a|b){10}' with
  # max_alternations of 2 and no max_min, will result in 1024 alternatives as
  # the alternation expression does have only two alternatives, there are
  # just many copies of it in the top (non-alternation) expression.
  #
  # 3. Each alternative is analyzed to find every fixed with pattern it
  #    contains that is convertible to an AC pattern (most are).  Each
  #    alternative forms a conjunctive clause of the final result.
  #
  # 4. The result of each alternative are combined into the single final
  #    disjunction.
  #
  # A significant portion of regular expressions is supported, but not all of
  # it.  Notable limitations are listed below.  Those marked with
  # [parser] are due to bugs or limitations in the regexp_parser gem.  Those
  # marked with [future] are potential future features.
  #
  # - \cX is not supported for X non-alpha. [parser]
  # - Only the i option is supported.  The m option is assumed.  [future]
  # - There is no unicode support, however, bytes can be specified by \xHHH.
  #   [parser]
  # - Many pcre specific features such as \Q are not supported. [parser]
  # - Back references are not supported.  [future]
  # - Stacked quantifiers, e.g., 'x{2}{3}', will result in suboptimal
  #   patterns.  Use parenthesis to work around, e.g., '(?:x{2}){3}. [parser]
  def self.extract(re, max_alternations = nil, max_min = nil)
    top = Regexp::Parser.parse(re, 'ruby/1.9')
    Impl::extract(top, max_alternations, max_min)
  end

  # Convert result of extract() to have singular clauses.
  #
  # Recall that result represents a DNF as an array of subarrays with each
  # subarray representing a conjuctive clause.  This method selects a single
  # element of each conjunctive clause and returns the result as an array of
  # strings.  It chooses elements so as to minimize the number of terms in the
  # result first, and maximize the length of elements second.
  def self.craft_suggestion(result)
    # Result is array of array of strings.
    # Need to choose a set of strings S such that for each subarray T S
    # intersect T is non-empty.  Otherwise, want S to be as small as possible.

    # Call each subarray of result a "row".

    # Convert arrays to sets.
    result = result.collect(&:to_set)

    # Choose a string and remove all rows that it appears in.  Choose string
    # by highest count with ties broken by length.
    suggestion = []
    while ! result.empty?
      # Count number of rows each string appears in.
      counts = Hash.new(0)
      result.each {|r| r.each {|s| counts[s] += 1}}

      highest_count = counts.values.max
      highest_strings = counts.select {|s, c| c == highest_count}.collect(&:first)
      highest_string = highest_strings.max {|a,b| a.length <=> b.length}

      suggestion << highest_string
      result = result.select {|r| ! r.member?(highest_string)}
    end

    suggestion
  end
end

if __FILE__ == $0
  STDIN.each do |line|
    puts line
    puts '  ' + (ReToAC::extract(line.chomp, 5, 10).collect do |alternative|
      "(#{alternative.join(' AND ')})"
    end.join(" OR\n  "))
  end
end
