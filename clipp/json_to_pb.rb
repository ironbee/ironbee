#!/usr/bin/env ruby19

# Script to convert JSON to clipp protobuf.
# The inverse of pb_to_json.
#
# This script is fragile and only really works for tweaking the results of
# pb_to_json.

$:.unshift(File.dirname(__FILE__))
require 'hash_to_pb'

if ! ARGV.empty?
  puts "Usage: #{$0} < <json file>"
  exit 1
end

inputs = JSON.parse(STDIN.read)
inputs.each do |input|
  IronBee::CLIPP::HashToPB::write_hash_to_pb(STDOUT, input)
end
