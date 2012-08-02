#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))

require 'all-code'

def last_chars(path, n)
  File.open(path, 'r') do |fp|
    fp.seek(-n, IO::SEEK_END)
    fp.read
  end
end

all_ironbee_code do |path|
  finals = last_chars(path, 2)
  if finals =~ /^[ \n]+/
    puts "#{path} -- Extra space."
  end
  if finals[1] != 10
    puts "#{path} -- Missing final new line."
  end
end
