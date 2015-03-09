#!/usr/bin/env ruby

# Generate a report from predicate profiling information.
# See documentation for the configuration directives:
# - PredicateProfile
# - PredicateProfileDir
#
# Author: Sam Baskinger <sbaskinger@qualys.com>
#


# Include our directory in the library search path.
$: << File.dirname($0)

# Now do the requires.
require 'logger'
require 's_expr'
require 'optparse'

################
# Setup Logging
################
# Logging device.
class MyLogDev
  def write(s)
    puts s
  end

  def close
  end
end

$MY_LOG_DEVICE = MyLogDev.new

$LOG = Logger.new($MY_LOG_DEVICE)
$LOG.level = Logger::WARN

#########################
# Parse program options.
#########################

OptionParser.new do |opts|
  opts.banner = "Usage: #{$0} [options] [profile data files]"

  opts.on("-lLOGLEVEL", "--loglevel=LOGLEVEL", "Log level.") do |l|
    $LOG.level = Logger::DEBUG if "DEBUG".casecmp(l) == 0
    $LOG.level = Logger::ERROR if "ERROR".casecmp(l) == 0
    $LOG.level = Logger::FATAL if "FATAL".casecmp(l) == 0
    $LOG.level = Logger::INFO if "INFO".casecmp(l) == 0
    $LOG.level = Logger::WARN if "WARN".casecmp(l) == 0
  end
end.parse!

####################
# Class definitions
####################

class NodeMetaData
  attr_accessor :times

  def initialize
    @times = []
  end

  def <<(time)
    @times << time
  end

  def time_total
    @times.reduce(0) { |x,y| x + y }
  end

  def count
    @times.length
  end

  # Merge +that+ into +self+ and return +self+.
  def merge!(that)
    @times = @times + that.times
    self
  end
end

# This class represents a predicate profiling run.
class PredicateProfile
  def initialize
    @nodedb = {} # hash of all nodes.
  end

  # Add a node with the given meta data to this graph.
  def add(node, timing)
    if @nodedb.key? node
      @nodedb[node].data << timing
    else
      metadata = NodeMetaData.new
      metadata << timing
      node.data = metadata
      @nodedb[node] = node
    end
  end

  def top_times
    @nodedb.values.sort do |node1, node2|
      node2.data.time_total <=> node1.data.time_total
    end
  end

  def top_calls
    @nodedb.values.sort do |node1, node2|
      node2.data.count <=> node1.data.count
    end
  end
end

$LOG.debug { "Starting." }

predicate_profile = PredicateProfile.new

# For each file, build a predicate_profile object.
ARGV.each do |file_name|
  $LOG.info { "Opening #{file_name}." }

  File.open file_name do |io|
    while data = io.read(4) do

      # Read data
      t    = data.unpack("L")[0]
      name = io.readline("\0")

      $LOG.debug("analysis") { "Parsing expression: #{name}"}
      node = SExpr.parse(name)
      $LOG.debug("analysis") { "Recording node:     #{node}" }
      predicate_profile.add(node, t)
    end
  end

  $LOG.debug { "Closing #{file_name}." }
end

$LOG.info("analysis") { "Building top times." }

predicate_profile.top_times.each_with_index do |node, idx|
  puts "Time: #{node.data.time_total}: #{node}"
end

$LOG.info("analysis") { "Building top calls." }

predicate_profile.top_calls.each_with_index do |node, idx|
  puts "Calls: #{node.data.count}: #{node}"
end