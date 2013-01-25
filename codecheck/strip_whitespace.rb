#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))

require 'all-code'
require 'set'

def has_trailing_whitespace?(path)
    ! IO.foreach(path).find {|l| l =~ / +$/}.nil?
end

all_ironbee_code do |path|
  if has_trailing_whitespace?(path)
    puts path
    
    lines = IO.foreach(path).to_a
    lines.each do |line|
        line.gsub!(/ +$/, '')
    end
    
    File.open(path,'w') do |fp|
        lines.each {|line| fp.print line}
    end
  end
end
