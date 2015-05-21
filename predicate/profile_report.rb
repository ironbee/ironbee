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

# Where the ruby files are.
$: << File.join(ENV['HOME'], 'git/ironbee/predicate')

require 's_expr' # IronBee S Expression Parser
require 'ppa'    # Predicate Profile Analyzer

# Set the analyzer's logger level.
$LOG       = PredicateProfileAnalyzer.logger
$LOG.level = Logger::INFO

# Set the data directory we'll be looking at to a reasonable default.
$data_dir = "/var/tmp/#{ENV['USER']}"
$report   = nil
$limit    = 20

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

  opts.on('-dDATADIR', '--datadir=DATADIR', "Data directory.") do |v|
    $data_dir = v
  end

  opts.on("--limit=LIMIT", "Limit of records to report.") do |v|
    $limit = v.to_i
  end

  opts.on('-rREPORT', '--report=REPORT', "What report to run?\n"+
    <<-EOS
        top_times
        top_self_times
        top_avg_times
        top_avg_self_times
        top_times_subtree
        top_self_times_subtree
        top_avg_times_subtree
        top_avg_self_times_subtree
        calls <regexp> - Return how often an expression is called.
    EOS
  ) do |v|
    $report = v
  end
end.parse!

if ARGV.length > 0
  $report = ARGV.shift
end

class ProfileReporter

  # The report this writes to.
  # This must implement write() and close().
  attr_accessor :report

  # Directory to examine for `.bin` data files.
  attr_accessor :data_dir

  attr_accessor :predicate_profile

  # Build a profile reporter and load the description file.
  def initialize(options={})
    @predicate_profile = PredicateProfileAnalyzer::PredicateProfile.new
    @report            = STDOUT
    @data_dir          = options[:data_dir] || $data_dir

    # Load naitive libraries for parser.
    begin
      SExprNative.load
      SExprNative.attach
      @predicate_profile.parser = SExprNative
    rescue LoadError => e
      $LOG.warn { "Failed to load native FFI libraries. Using regex. #{e.message}" }
    end

    load_descr
  end

  def load_descr
    # Load the description file.
    @predicate_profile.load_description_file File.join(@data_dir, 'main', 'profile_graph.descr')
  end

  def load_files limit=nil
    # Grab first N data files that end in `.bin`.
    files = Dir.new(@data_dir).grep(/\.bin$/)
    files = files.first(limit) if limit
    files.each do |f|
      @predicate_profile.load_file File.join(@data_dir, f)
    end
    nil
  end

  # Do a depth-first call of block `f` on `node` and its subtree.
  def dfcall(node, depth=0, &f)

    f.call node, depth

    depth = depth + 1

    node.children.each do |c|
      dfcall(c, depth, &f)
    end
  end

  def top_times_report report_name, limit = 20
    # Get the top 3 and write out their origin information.
    @predicate_profile.__send__(report_name).first(20).map do |node|
        nd = node.data
        @report.puts "=========================="
        @report.puts "NODE:         #{nd.index}"
        @report.puts "TIME AVG/TOT: %.2fus / %dus"%[nd.time_avg, nd.time_total ]
        @report.puts "SELF AVG/TOT: %.2fus / %dus"%[nd.self_time_avg, nd.self_time_total ]
        @report.puts "CALLS:        #{nd.count}"
        (@predicate_profile.origins(node) or []).each_with_index do |o, i|
          @report.puts "ORIGIN #{i}: #{o}"
        end
        @report.puts "--------------------------"
    end
  end

  def top_times_report_subtree report_name
    n = @predicate_profile.__send__(report_name.sub(/_subtree$/, '')).first(1)[0]

    # Use the handy printer to show the self and total times for each child expression.
    # NOTE: Children may have higher total and average times than their parents because
    # they may have been called by other parent nodes.
    dfcall(n) do |n, depth|
      node_data = n.data
      self_avg = node_data.self_time_total / node_data.count.to_f
      tot_avg =  node_data.time_total / node_data.count.to_f
      @report.print "%.2f / %.2f (micro seconds)\t"%[ self_avg, tot_avg ]
      @report.puts "%s%s"%["  "*depth, n.data.index]
    end
  end

  def top_times limit = 20
    top_time_impl :top_times, limit
  end

  def top_self_times limit = 20
    top_time_impl :top_self_times, limit
  end

  def top_avg_times limit = 20
    top_time_impl :top_avg_times, limit
  end

  def top_avg_self_times limit = 20
    top_time_impl :top_avg_self_times, limit
  end

end # class ProfileReporter

##########################################
# Main
##########################################

$LOG.debug { "Starting." }

p = ProfileReporter.new data_dir: $data_dir
p.load_files 100

case ($report)
when "top_times"
  p.top_times_report $report, $limit

when "top_self_times"
  p.top_times_report $report, $limit

when "top_avg_times"
  p.top_times_report $report, $limit

when "top_avg_self_times"
  p.top_times_report $report, $limit

when "top_times_subtree"
  p.top_times_report_subtree $report

when "top_self_times_subtree"
  p.top_times_report_subtree $report

when "top_avg_times_subtree"
  p.top_times_report_subtree $report

when "top_avg_self_times_subtree"
  p.top_times_report_subtree $report

when "calls"
  i = p.
    predicate_profile.
    grep(Regexp.new(ARGV[0])).
    sort { |n1, n2| n2.data.count <=> n1.data.count }

  if $limit
    i.first($limit)
  else
    i
  end.each do |n|
    puts n.id, n.data.count
  end
else
  puts "Unknown report requested: #{$report}."
end

