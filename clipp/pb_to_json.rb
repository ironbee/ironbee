#!/usr/bin/env ruby

# Script to convert clipp protobuf files to JSON.

$:.unshift(File.dirname(__FILE__))
require 'rubygems'
require 'clipp.pb'
require 'zlib'
require 'json'

if ! ARGV.empty?
  puts "Usage: #{$0} < <pb file>"
  exit 1
end

inputs = []

while ! STDIN.eof?
  size = STDIN.read(4).unpack("N")[0]
  data = STDIN.read(size)
  gzip = Zlib::GzipReader.new(StringIO.new(data))

  input = IronBee::CLIPP::PB::Input.new.parse_from_string(gzip.read)
  gzip.close
  inputs << input.to_hash
end

puts JSON.pretty_generate(inputs)

