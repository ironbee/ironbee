#!/usr/bin/env ruby

if ARGV.empty?
  puts "Usage #{$0} <file>"
  exit 1
end

path = ARGV[0]
base = File.basename(path, ".txt")
terms = []
section = nil
IO::foreach(path) do |line|
  if line =~ /\[\[s.(.+)\]\]/
    section = $1
  elsif line =~ /\[\[p.(.+)\]\]/
    terms << [$1, section]
  end
end
File.open("#{base}_index.txt", "w") do |index|
  terms.sort.each do |term, section|
    index.puts "<<p.#{term},+#{term}+>> (<<s.#{section},#{section.capitalize}>>) +"
  end
end
