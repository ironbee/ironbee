#!/usr/bin/env ruby

SKIP = [
    %r{^cpp/},
    %r{config-parser.c}
]

$:.unshift(File.dirname(__FILE__))

require 'all-code'

all_ironbee_code do |path|
    next if SKIP.find {|x| path =~ x}
    have_license = false
    IO.foreach(path) do |line|
        if line =~ %r{Licensed to Qualys, Inc.}
            have_license = true
            break
        elsif line =~ /^\s*$/
            break
        end
    end

    puts path if ! have_license
end
