#!/usr/bin/env ruby19

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

require 'test/unit'
require 'tmpdir'
require 'erb'
require 'open3'

# Assertion module.
#
# Mixed in to CLIPPTestCase
module CLIPPTestAssertions
  # Assert that _re_ appears in the log.
  def assert_log_match(re)
    assert_match(re, log)
  end

  # Assert that _re_ does not appear in the log.
  def assert_log_no_match(re)
    assert_no_match(re, log)
  end
end

# Base class for CLIPP related test cases.
#
# This class should be used in place of Test::Unit::TestCase.  It adds
# the clipp command which invokes clipp to run IronBee and a number of
# assertions about the most recent run (see CLIPPTestAssertions)
#
class CLIPPTestCase < Test::Unit::TestCase
  include CLIPPTestAssertions

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
  BUILDDIR         = File.join(
    ($abs_builddir || ENV['abs_builddir'] || fatal("abs_builddir not set.")),
    'tests'
  )
  # CLIPP executable.
  CLIPP            = File.join(BUILDDIR, '..', 'clipp')
  # Default IronBee configuration template.
  DEFAULT_TEMPLATE = 'TESTDIR/ironbee.config.erb'
  # Default consumer.
  DEFAULT_CONSUMER = 'ironbee:IRONBEE_CONFIG'

  # Execute clipp using the clipp config at config_path.  Output is displayed
  # to standard out and returned.  If exit status is non-zero then nil is
  # returned.
  def run_clipp(config_path)
    stdout, stderr, status = Open3.capture3(CLIPP, '-c', config_path)

    puts "#{CLIPP} -c #{config_path}"
    puts "== CLIPP Configuration =="
    puts IO.read(config_path)
    puts "== STDOUT =="
    puts stdout
    puts "== STDERR =="
    puts stderr
    puts
    puts "Exit status: #{status.exitstatus}"

    if status.exitstatus != 0
      nil
    else
      stdout + "\n" + stderr
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

public

  # Run clipp.  Determines the context which all clipp assertions run under.
  # Includes an assertion that clipp exited normally.
  #
  # All arguments are key-values.  :input is required.
  #
  # Configuration Options:
  # +input+::    One or more clipp input chains.
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
  #
  # More options coming in the future.
  def clipp(config)
    fatal ":input required in config." if ! config[:input]

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

    assert_not_nil(@log)
  end
end
