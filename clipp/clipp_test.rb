#!/usr/bin/env ruby

#
# Defines the CLIPPTestCase class which can be used in place of
# Test::Unit::TestCase to do clipp based testing.
#
# CLIPPTestCase adds an clipp command to run IronBee in various ways and
# then a number of assertions related to the most recent run.
#
# This file requires that either ENV['abs_builddir] or $abs_builddir be set
# to the build location of clipp.
#

require 'rubygems'
require 'test/unit'
require 'tmpdir'
require 'erb'

$:.unshift(File.dirname(__FILE__))
require 'hash_to_pb'

# Assertion module.
#
# Mixed in to CLIPPTest.
module CLIPPTestAssertions
  # Assert that _re_ appears in the log.
  def assert_log_match(re)
    assert_match(re, log, "#{re.inspect} was not found in log.")
  end

  # Assert that _re_ does not appear in the log.
  def assert_log_no_match(re)
    assert_no_match(re, log, "#{re.inspect} was found in log.")
  end

  # Assert that nothing higher than notice appears
  def assert_no_issues
    assert_log_no_match(/ (EMERGENCY|CRITICAL|ALERT|ERROR|WARNING) /)
  end
end

# Log helper module.
#
# Mixed into CLIPPTest.
module CLIPPTestLogHelper
  # Number of times re appears in the log.
  def log_count(re)
    n = 0
    log.scan(re) {n += 1}
    n
  end
end

# Base class for CLIPP related test cases.
#
# This class should be used in place of Test::Unit::TestCase.  It adds
# the clipp command which invokes clipp to run IronBee and a number of
# assertions about the most recent run (see CLIPPTestAssertions)
#
module CLIPPTest
  include CLIPPTestAssertions
  include CLIPPTestLogHelper

  # Access log of most recent clipp run.
  attr_reader :log
  # Alias for above.
  attr_reader :clipp_log

  # Helper routine.  Display message and exit.
  def self.fatal(message)
    STDERR.puts message
    exit 1
  end

  # Source test directory.
  TESTDIR          = File.join(File.expand_path(File.dirname(__FILE__)), 'tests')
  # Build test directory.
  BUILDDIR         = \
    ($abs_builddir || ENV['abs_builddir'] || fatal("abs_builddir not set."))

  # CLIPP executable.
  CLIPP            = File.join(BUILDDIR, 'clipp')
  # Default IronBee configuration template.
  DEFAULT_TEMPLATE = 'TESTDIR/ironbee.config.erb'
  # Default consumer.
  DEFAULT_CONSUMER = 'ironbee:IRONBEE_CONFIG'

  # Execute cmd and return [output, exit status]
  # stdin is closed immediately.
  def run_command(cmd, *args)
    r,w = IO.pipe

    pid = fork do
      r.close
      STDIN.close
      STDOUT.reopen(w)
      STDERR.reopen(w)

      exec(cmd, *args)
    end

    w.close
    output = r.read
    pid, status = Process::wait2(pid)

    [output, status]
  end

  # Execute clipp using the clipp config at config_path.  Output is displayed
  # to standard out and returned.  If exit status is non-zero then nil is
  # returned.
  def run_clipp(config_path)
    output, status = run_command(
      CLIPP, '-c', config_path
    )

    puts "#{CLIPP} -c #{config_path}"
    puts "== CLIPP Configuration =="
    puts IO.read(config_path)
    puts "== OUTPUT =="
    puts output
    puts
    puts "Exit status: #{status.exitstatus}"

    if status.exitstatus != 0
      nil
    else
      output
    end
  end

  # Replace TESTDIR and BUILDDIR in path with the appropriate values (see
  # above).
  def expand_path(path)
    File.expand_path(
      path.gsub(
        'TESTDIR',  TESTDIR
      ).gsub(
        'BUILDDIR', BUILDDIR
      )
    )
  end

private

  # Generate an IronBee configuration where context is the context to evaluate
  # the erb in and template is the path to the erb template (defaults to
  # DEFAULT_TEMPLATE).
  def generate_ironbee_configuration(context, template = nil)
    template ||= DEFAULT_TEMPLATE
    template = expand_path(template)
    fatal "Could not read #{template}" if ! File.readable?(template)

    erb = ERB.new(IO.read(template))
    config = erb.result(context)
  end

  # Write contents to name_template in a temporary location and return
  # path.  name_template will have RAND replaced with a random number.
  def write_temp_file(name_template, contents)
    to = File.join(
      Dir::tmpdir,
      name_template.gsub('RAND') {rand(10000)}
    )
    File.open(to, 'w') {|fp| fp.print contents}

    to
  end

  def make_context(c)
    binding
  end

public

  # Evaluate ERB with specific context.
  #
  # Context is a hash that is made available to the ERB as c.  E.g.,
  # {:foo => 5} allows <%= c[:foo] %> in the ERB to be replaced by 5.
  def erb(erb_text, context = {})
    ERB.new(erb_text).result(make_context(context))
  end

  # As above, but takes a path to a file.
  def erb_file(erb_path, context = {})
    erb(IO.read(erb_path))
  end

  # Create a simple input with a single connection of a single transaction
  # of a single connection data in and connection data out event.
  def simple_hash(request, response = "")
    {
      "id" => "simple_hash",
      "connection" => {
        "pre_transaction_event" => [
          {
            "which" => 1,
            "connection_event" => {
              "local_ip" => "1.2.3.4",
              "local_port" => 1000,
              "remote_ip" => "5.6.7.8",
              "remote_port" => 80
            }
          }
        ],
        "transaction" => [
          {
            "event" => [
              {
                "which" => 2,
                "data_event" => {"data" => request}
              }
            ]
          },
          {
            "event" => [
              {
                "which" => 3,
                "data_event" => {"data" => response}
              }
            ]
          },
        ],
        "post_transaction_event" => [
           {
             "which" => 4
           }
        ]
      }
    }
  end

  # Run clipp.  Determines the context which all clipp assertions run under.
  # Includes an assertion that clipp exited normally.
  #
  # All arguments are key-values.  :input or :input_hashes is required.
  #
  # Configuration Options:
  # +input+::    One or more clipp input chains.
  # +input_hashes+:: Input as array of hashes.  See hash_to_pb.rb.
  # +template+:: ERB template to use for ironbee configuration file.  The
  #              result of this template can be used via IRONBEE_CONFIG.
  # +consumer+:: Consumer chain.  IRONBEE_CONFIG will be replaced with the
  #              path to the evaluation of +template+.  Defaults to
  #              DEFAULT_CONSUMER.
  #
  # The entire configuration hash is made available to the ERB template via
  # +config+.  The following options are used by the default template:
  #
  # +config+::              Inserted before default site section.
  # +default_site_config+:: Inserted inside default site section.
  # +config_trailer+::      Inserted at end of configuration file.
  # +log_level+::           What log level to run at; default 'notice'.
  #
  # More options coming in the future.
  def clipp(config)
    config = config.dup

    if ! config[:input] && ! config[:input_hashes]
      CLIPPTestCase::fatal "Must have :input or :input_hashes."
    end

    if config[:input_hashes]
      input_content = ""
      config[:input_hashes].each do |h|
        input_content +=
          IronBee::CLIPP::HashToPB::hash_to_pb(h)
      end
      input_path = write_temp_file("clipp_test_RAND.pb", input_content)
      config[:input] ||= "pb:INPUT_PATH"
      config[:input].gsub!("INPUT_PATH", input_path)
    end

    config_path = write_temp_file(
      "clipp_test_RAND.config",
      generate_ironbee_configuration(binding, config[:template])
    )

    consumer_chain = (config[:consumer] || DEFAULT_CONSUMER).gsub(
      'IRONBEE_CONFIG', config_path
    )

    clipp_config = write_temp_file(
      "clipp_test_RAND.clipp",
      "#{config[:input]} #{consumer_chain}\n"
    )

    @log = @clipp_log = run_clipp(clipp_config)

    assert_not_nil(@log, "No output.")
  end
end
