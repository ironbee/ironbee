#!/usr/bin/env ruby

# Note that automata does not require CLIPP features beyond
# the TestCase/Minitest abstraction to CLIPPTest::TestCase.
# CLIPP is not used in these tests.
require '../../clipp/clipp_test'

$:.unshift(File.dirname(__FILE__))

require 'tc_basic'
require 'tc_pattern'
