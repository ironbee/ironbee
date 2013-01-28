#!/usr/bin/env ruby

# This script looks at stdin for IronBee rules with fast: modifiers and pulls
# the arguments and ids out into a manifest for building the fast automata.

# This script is currently heuristic.  It should be replaced with a backend to
# an actual rule language parser.

STDIN.each do |line|
  if line =~ /("|\s)id:(.+?)\1/
    id = $2
    line.gsub(/("|\s)fast:(.+?)\1/).each do
      pattern = $2
      pattern.gsub!(/\s/, '\s')
      puts [pattern, id].join(' ')
    end
  end
end
