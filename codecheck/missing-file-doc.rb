#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))

require 'all-code'

all_ironbee_code do |path|
  if path =~ /\.[ch](pp)?$/
    okay = false
    IO.foreach( path ) do |line|
      if line =~ / @file/
        okay = true
        break
      end
    end
    if ! okay
      puts path
    end
  end
end
