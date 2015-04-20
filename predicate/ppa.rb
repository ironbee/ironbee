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
  attr_accessor :times
  attr_accessor :self_times

  def initialize
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
  def initialize
    @nodedb = {} # hash of all nodes.
    @origindb = {} # hash of all node origins.
  end

  def node id
    id = SExpr.parse id if id.is_a? String
    @nodedb[id]
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
        child.data = NodeMetaData.new
        @nodedb[child] = child
        merge_graph child
      end
    end
  end

  # Add a node with the given meta data to this graph.
  def add node, timing, self_timing
    if @nodedb.key? node
      # We do not merge because we don't have
      # timing information of the sub-nodes. That is
      # recorded in separate records.
      @nodedb[node].data.add_timing timing, self_timing
    else
      metadata      = NodeMetaData.new
      metadata.add_timing timing, self_timing
      node.data     = metadata
      @nodedb[node] = node

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
    @@log.info { "Opening #{file}." }

    File.open file, 'rb' do |io|
      while data = io.read(4) do

        # Read duration data
        duration = data.unpack("L")[0]

        # Read self-duration.
        data          = io.read(4)
        self_duration = data.unpack("L")[0]

        # Read the name.
        name = io.readline("\0")

        @@log.debug("analysis") { "Parsing expression: #{name}"}
        node = SExpr.parse(name)

        @@log.debug("analysis") { "Recording node:     #{node}" }
        add(node, duration, self_duration)
      end
    end

    @@log.debug { "Closing #{file}." }
  end

  # The description file is extra data that Predicate can output such as the location of rule definitions.
  def load_description_file file
    @@log.info { "Open #{file}." }

    File.open(file, 'rb').each_line do |line|

      # For now, we do not do anything with sub-nodes, denoted by a leading tab.
      next if line.start_with? "\t"

      # Split the string on tab to get the expression and any origins associated with it.
      expr, *origins = line.split "\t"

      @origindb[expr] = origins
    end

    @@log.debug { "Closing #{file}." }
  end

  def data node
    @nodedb[node].data
  end

  def origins node
    @origindb[node]
  end
end # class PredicateProfile
end