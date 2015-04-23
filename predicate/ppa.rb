#!/usr/bin/env ruby

# Predicate Profile Analyzer.
#
# A collection of tools to analyzer predicate profile files with.
#
# Author: Sam Baskinger <sbaskinger@qualys.com>
#

require 'logger'
require 's_expr'

module PredicateProfileAnalyzer

# Encapsulate logging into a module that can be included by all classes in this module.
module Logging
  @@log = Logger.new(STDERR)
  @@log.level= Logger::WARN
  def self.log= l
    @@log = l
  end
  def self.log
    @@log
  end
end

# Set the package logger.
def self.logger= l
  Logging::log = l
end

# Get the package logger.
def self.logger
  Logging::log
end

class NodeMetaData
  include Logging
  attr_accessor :index
  attr_accessor :times
  attr_accessor :self_times

  # Constructor.
  #
  # If no index is given, then this data describes a shadow node.
  #
  # That is, we have not encountered the node's description
  # which maps an index to an expression, so we cannot record
  # timing data for it yet. This is a normal occurence when reading
  # a graph description file.
  #
  # If the graph description file is not in-error, then all nodes
  # will eventually be described, and all index-to-expression
  # mappings will be defined.
  def initialize index = nil
    @index      = index
    @times      = []
    @self_times = []
  end

  def add_timing time, self_time
    @@log.debug { "Adding time #{time}, self-time #{self_time}" }
    @times      << time
    @self_times << self_time
  end

  def time_total
    @times.reduce(0) { |x,y| x + y }
  end

  def self_time_total
    @self_times.reduce(0) { |x,y| x + y }
  end

  def time_avg
    @times.reduce(0) { |x,y| x + y } / count.to_f
  end

  def self_time_avg
    @self_times.reduce(0) { |x,y| x + y } / count.to_f
  end

  def count
    @times.length
  end

  # Merge +that+ into +self+ and return +self+.
  def merge! that
    @times      = @times + that.times
    @self_times = @self_times + that.self_times
    self
  end
end # class NodeMetaData

# This class represents a predicate profiling run.
class PredicateProfile
  include Logging

  # Get or set the current parser
  #
  # A parser is a module that respons to parse(string)
  # and returns an SExpr.
  #
  # See SExpr or SExprNative.
  #
  attr_accessor :parser

  def initialize
    @nodedb   = {} # hash of all nodes.

    # List of nodes by node index.
    # Node description builds this.
    @nodelist = []
    @origindb = {} # hash of all node origins.
    @parser   = SExpr
  end

  def node id
    id = @parser.parse id if id.is_a? String
    @nodedb[id]
  end

  # A shadow node is any node that does not yet have an index.
  #
  # This is how they are added to the graph.
  # Shadow nodes are only valid when reading a graph description file
  # as we encounter nodes before we encounter their index.
  def add_shadow_node node
    node.data = NodeMetaData.new # Give this node meta data.
    @nodedb[node] = node         # Add to the has database.
  end

  #
  # Replace any descendents of +node+ with existing nodes.
  #
  # If a descendent of +node+ is not found, it is
  # added with no timing information.
  #
  def merge_graph node
    node.children.each_with_index do |child, idx|

      # If the node exists, replace it and we are done.
      if @nodedb.key? child
        c = @nodedb[child]

        # Replace our child node with the exist child node.
        node.children[idx] = c

        # Tell the child node we are also a parent to it.
        c.parents << node

      # Otherwise, add this node and recursively keep checking.
      else
        add_shadow_node child
        merge_graph     child # Keep merging.
      end
    end
  end

  # Add a node with the given meta data to this graph.
  #
  # When a node is added, it has all the data it needs. Specifically,
  # the index from the description file. Subsequent adds
  # have timing information.
  def add_node node, idx, timing = nil, self_timing = nil
    # Fetch by index.
    n = @nodelist[idx]

    # If that fails, it may be a shadow node.
    unless n
      n = @nodedb[node]

      # If we have a node listed by expression but not by index,
      # fix that. This situtation arises when a graph contains
      # references to nodes for which there is partial information.
      if n
        @@log.debug { "Fixing shadow node's index: #{idx}." }
        n.data.index = idx
        @nodelist[idx] = n
      end
    end

    if n
      # We do not merge because we don't have
      # timing information of the sub-nodes. That is
      # recorded in separate records.
      if timing and self_timing
        n.data.add_timing timing, self_timing
      end

    # Completely new node!
    else
      metadata = NodeMetaData.new idx

      if timing and self_timing
        metadata.add_timing timing, self_timing
      end

      @@log.debug { "Recording node index #{idx}." }
      node.data      = metadata
      @nodelist[idx] = node
      @nodedb[node]  = node

      merge_graph node
    end
  end

  def top_times
    @nodedb.values.sort do |node1, node2|
      node2.data.time_total <=> node1.data.time_total
    end
  end

  def top_self_times
    @nodedb.values.sort do |node1, node2|
      node2.data.self_time_total <=> node1.data.self_time_total
    end
  end

  def top_avg_times
    @nodedb.values.sort do |node1, node2|
      value1 = node1.data.time_avg
      value2 = node2.data.time_avg

      if value1.nan?
        if value2.nan?
          0
        else
          1
        end
      else
        if value2.nan?
          -1
        else
          value2 <=> value1
        end
      end
    end
  end

  def top_avg_self_times
    @nodedb.values.sort do |node1, node2|
      value1 = node1.data.self_time_avg
      value2 = node2.data.self_time_avg

      if value1.nan?
        if value2.nan?
          0
        else
          1
        end
      else
        if value2.nan?
          -1
        else
          value2 <=> value1
        end
      end
    end
  end

  def top_calls
    @nodedb.values.sort do |node1, node2|
      node2.data.count <=> node1.data.count
    end
  end

  def load_file file
    @@log.info { "Opening data #{file}." }

    File.open file, 'rb' do |io|
      while data = io.read(12) do

        # Read duration data
        duration, self_duration, node_idx = data.unpack("LLL")

        node = @nodelist[node_idx]

        raise RuntimeError.new(
          "No known node id #{node_idx}. Did you load the description file first?"
        ) unless node

        @@log.debug("analysis") { "Recording node: #{node_idx}" }
        add_node(node, node_idx, duration, self_duration)
      end
    end

    @@log.debug { "Closing data #{file}." }
  end

  # The description file is extra data that Predicate can output such as the location of rule definitions.
  def load_description_file file
    @@log.info { "Open descripton #{file}." }

    line_no = 1
    File.open(file, 'rb').each_line do |line|

      @@log.info { "Line #{line_no} in #{file}." } if line_no % 100 == 0

      line_no = line_no + 1

      # Split the string on tab to get the expression and any origins associated with it.
      idx, expr, *origins = line.split "\t"

      idx = idx.to_i

      # It is not valid to see a node twice, but protect against it.
      next if @nodelist[idx]

      node = @parser.parse expr

      raise RuntimeError.new "Node is null." unless node

      @origindb[expr] = origins

      # Add the node, but with no timing information.
      add_node node, idx
    end
    @@log.debug { "Closing descripton #{file}." }

  end

  def data node
    @nodedb[node].data
  end

  def origins node
    @origindb[node]
  end
end # class PredicateProfile
end