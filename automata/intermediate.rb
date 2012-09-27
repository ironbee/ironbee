#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))
require 'rubygems'
require 'intermediate.pb'
require 'zlib'

module IronAutomata
module Intermediate

def self.read_chunk(input)
  size = input.read(4).unpack("N")[0]
  data = input.read(size)
  gzip = Zlib::GzipReader.new(StringIO.new(data))

  PB::Chunk.new.parse_from_string(gzip.read)
end

def self.write_chunk(output, data)
  buffer = ""
  gzip = Zlib::GzipWriter.new(StringIO.new(buffer))
  data.serialize_to(gzip)
  gzip.close

  output.write [buffer.size].pack("N")
  output.write buffer
end

end
end

if $0 == __FILE__
  puts "Library.  Do not run directly."
  exit(1)
end
