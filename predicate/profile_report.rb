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
require 'optparse'

require 's_expr' # IronBee S Expression Parser
require 'ppa'    # Predicate Profile Analyzer


################
# Setup Logging
################
# Logging device.
class MyLogDev
  def write s
    STDERR.write s
  end

  def close
  end
end

$MY_LOG_DEVICE = MyLogDev.new

$LOG = Logger.new($MY_LOG_DEVICE)
$LOG.level = Logger::WARN

# Set the logger for the analyzer.
PredicateProfileAnalyzer::logger = $LOG

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
# Main Line
####################

$LOG.debug { "Starting." }

predicate_profile = PredicateProfileAnalyzer::PredicateProfile.new

# For each file, build a predicate_profile object.
ARGV.each do |file_name|
  predicate_profile.load_file file_name
end

$LOG.info("analysis") { "Building predicate_timing.txt." }

File.open 'predicate_timing.txt', 'w' do |io|
  predicate_profile.top_times.each_with_index do |node, idx|
    io.write "Time: #{node.data.time_total}: #{node}\n"
  end
end

$LOG.info("analysis") { "Building predicate_calls.txt." }

File.open 'predicate_calls.txt', 'w' do |io|
  predicate_profile.top_calls.each_with_index do |node, idx|
    io.write "Calls: #{node.data.count}: #{node}\n"
  end
end