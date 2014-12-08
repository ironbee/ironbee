#!/usr/bin/env ruby

# Clipp is not required other than the
# Test::Unit::TestCase / Minitest::TestCase
# abstraction as CLIPPTest::TestCase.
require '../../clipp/clipp_test'

$:.unshift(File.dirname(__FILE__))

require 'tc_predicate'
require 'tc_predicate_constant'
require 'tc_predicate_errors'
