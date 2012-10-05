#!/usr/bin/env ruby

#
# This file can be included as a library or run as a script.  In the latter
# case, it takes a list of newline separated words on stdin and outputs the
# automata to stdout.
#

$:.unshift(File.dirname(File.dirname(__FILE__)))
require 'intermediate'
require 'set'

module IronAutomata

# Generate an automata implementing Aho-Corasick.
#
# Aho-Corasick is an algorithm for searching for substrings in an input.  It
# takes a list of words.  The resulting automata looks for those words in the
# input and outputs whenever one is found.
#
# The particular output is the length of the word.  As Aho-Corasick only finds
# exact matches, the matching substring can be found by looking backwards in
# the input at the point of output.
#
# This implementation closely follows the original paper with the exception of
# an added pass at the end which looks for situations where multiple failure
# edges are guaranteed to be followed and collapses those into a single
# failure edge.
#
# For an easier interface, see aho_corasick()
class AhoCorasickGenerator
  # ID of start node.
  START = 1

  # Build automata from words, an array of words.
  def initialize(words)
    @next_node_id = START + 1
    @goto = {}
    @failure = {}
    @output = {}

    words.each {|w| add_to_tree(w)}
    process_failures
    collapse_fails
  end

  # Generate one or more protobuf chunks and yield them.
  #
  # If chunk_size is 0, a single chunk will be generated containing the entire
  # automata.  Otherwise, each chunk will hold at most chunk_size nodes and
  # outputs.
  def each_pb(chunk_size = 0)
    raise "Block needed." if ! block_given?

    current_size = 0
    pb_chunk = IronAutomata::Intermediate::PB::Chunk.new
    advance_chunk = Proc.new do
      current_size += 1
      if chunk_size > 0 && current_size >= chunk_size
        yield pb_chunk
        current_size = 0
        pb_chunk = IronAutomata::Intermediate::PB::Chunk.new
      end
    end

    pb_graph = IronAutomata::Intermediate::PB::Graph.new
    pb_graph.no_advance_no_output = true
    pb_chunk.graph = pb_graph
    advance_chunk[]

    state = START
    while state < @next_node_id
      pb_node = IronAutomata::Intermediate::PB::Node.new
      pb_node.id = state
      if @output[state]
        pb_node.first_output = state
      end
      if state == START
        pb_node.default_target = START
        pb_node.advance_on_default = true
      else
        pb_node.default_target = failure(state)
        pb_node.advance_on_default = false
      end

      (@goto[state] || []).each do |a, s|
        pb_edge = IronAutomata::Intermediate::PB::Edge.new
        pb_edge.target = s
        pb_edge.values = [a].pack("c")

        pb_node.edges << pb_edge
      end

      pb_chunk.nodes << pb_node
      advance_chunk[]

      state += 1
    end

    n = 0
    @output.each do |state, (content, next_output)|
      next if ! content
      pb_output = IronAutomata::Intermediate::PB::Output.new
      pb_output.id = state
      pb_output.content = [content].pack("L")
      pb_output.next = next_output if next_output != 0

      pb_chunk.outputs << pb_output
      advance_chunk[]
    end

    yield pb_chunk if current_size > 0

    nil
  end

  # Create and return a single Protobuf chunk containing automata.
  def to_pb
    result = nil
    each_pb {|c| result = c}
    result
  end

  # Return a string representing the automata in intermediate format.
  #
  # If chunk_size is 0, a single chunk will be used, otherwise, chunks of up
  # to chunk_size nodes and outputs will be used.
  def to_intermediate(chunk_size = 0)
    out = StringIO.new
    n = 1
    each_pb(chunk_size) do |pb_chunk|
      IronAutomata::Intermediate::write_chunk(out, pb_chunk)
    end
    out.string
  end

  # Convert automata to GraphViz dot format and return as string.
  def to_dot
    out = StringIO.new

    out.puts "digraph G {"
    state = START
    while state < @next_node_id
      out.print "  #{state}"

      label = "#{state}\\n"
      label += output(state).join("\\n")

      out.puts " [label=\"#{label}\"];"

      (@goto[state] || []).each do |a, s|
        out.puts "  #{state} -> #{s} [label=\"%c\", weight=1000];" % [a]
      end

      if state == START
        out.puts "  #{state} -> #{state} [label=\"*\", weight=1000];"
      elsif failure(state)
        out.puts "  #{state} -> #{failure(state)} [style=\"dashed\"]";
      end

      state += 1
    end
    out.puts "}"

    out.string
  end

  private

  # @goto[node_id] is a list of [character, target_id]
  def goto(node_id, c)
    entry = (@goto[node_id] || []).find {|v,t| v == c}
    if entry
      entry[1]
    else
      nil
    end
  end

  def failure(node_id)
    @failure[node_id]
  end

  # @output[node_id] is a pair of [output, next_output_node]
  def output(node_id)
    result = []
    while o = @output[node_id]
      result << o[0]
      node_id = o[1]
    end
    result
  end

  def add_to_tree(word)
    state = START
    j = 0
    while goto(state, word[j])
      state = goto(state, word[j])
      j += 1
    end
    while j < word.length
      new_state = @next_node_id
      @next_node_id += 1
      @goto[state] ||= []
      @goto[state] << [word[j], new_state]
      j += 1
      state = new_state
    end
    @output[state] = [word.length, 0]
  end

  def process_failures
    queue = []
    @goto[START].each do |a, s|
      queue << s
      @failure[s] = START
    end
    n = 0
    while ! queue.empty?
      r = queue.shift
      (@goto[r] || []).each do |a, s|
        queue << s
        state = failure(r)
        while state != START && ! goto(state, a)
          state = failure(state)
        end
        if state == START
          @failure[s] = goto(state, a) || START
        else
          @failure[s] = goto(state, a)
        end
        if ! @output[s] && @output[failure(s)]
          @output[s] = @output[failure(s)]
        elsif @output[failure(s)]
          @output[s][1] = failure(s)
        end
      end
    end
  end

  def out_chars(node)
    Set.new((@goto[node] || []).collect {|a,s| a})
  end

  # Not part of normal AC.
  def collapse_fails
    queue = []
    @goto[START].each do |a, s|
      queue << s
    end
    n = 0
    while ! queue.empty?
      r = queue.shift
      out_r = out_chars(r)
      state = failure(r)
      while out_r == out_chars(state)
        state = failure(state)
      end
      if state != failure(r)
        @failure[r] = state
      end
      (@goto[r] || []).each do |a, s|
        queue << s
      end
    end
  end

end

# Simple interface to AhoCorasickGenerator.
#
# Takes array of words and returns automata in intermediate format.
def self.aho_corasick(words)
  AhoCorasickGenerator.new(words).to_intermediate
end

end

if __FILE__ == $0

  if ARGV.length != 0
    puts "Usage: #{$0}"
    exit 1
  end

  # Read input.
  words = STDIN.read.split

  ac = IronAutomata::AhoCorasickGenerator.new(words)

  print ac.to_intermediate

end
