#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))

require 'all-code'

all_ironbee_code do |path|
    IO.foreach(path) do |line|
        if line =~ %r{\([a-z_ ]*[a-z_]\*\)($|[^;])}
            puts "#{path}:#{$.} #{line.chomp}"
        end
    end
end
