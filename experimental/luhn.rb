#!/usr/bin/env ruby

# Generate automata for the Luhn credit card algorithm.

# This is just an example of how the Luhn algorithm could be implemented via
# an automata.  To use, this should be converted to generate an IronAutomata
# intermediate file.

# An obvious enhancement would be to add a table of prefixes and outputs of
# which company they belong to, e.g., AmEx.

require 'set'

DIGITS = 10
RADIX = 10

# Map of name to list of [value, destination]
$nodes = {}
$accepts = Set.new

def add_node(name, accept = false)
  $nodes[name.to_s] = []
  if accept
    $accepts << name
  end
end

def edge(from, to, value)
  add_node(from) if ! $nodes[from.to_s]
  add_node(to)   if ! $nodes[to.to_s]

  $nodes[from.to_s] << [value, to.to_s]
end

(0..RADIX-1).each do |i|
  edge("CC 1", "CC 2: #{i}", i)
end
edge("CC 1", "CC 1", "Else")
(2..DIGITS-1).each do |d|
  (0..RADIX-1).each do |i|
    (0..RADIX-1).each do |j|
      edge("CC #{d}: #{i}", "CC #{d+1}: #{(i+j)%RADIX}", j)
    end
    edge("CC #{d}: #{i}", "CC 1", "Else")
  end
end

add_node("Valid", true)
(0..RADIX-1).each do |i|
  edge("CC #{DIGITS}: #{i}", "Valid", (RADIX-i)%RADIX)
  edge("CC #{DIGITS}: #{i}", "CC 1", "Else")
end

edge("Valid", "CC 1", "Epsilon")

puts "digraph G {"
$nodes.each do |n, edges|
  print "  #{n.hash} [label=\"#{n}\""
  if $accepts.member?(n)
    print "; shape=box"
  end
  puts "];"
end
$nodes.each do |n, edges|
  edges.each do |value, dst|
    puts "  #{n.hash} -> #{dst.hash} [label=\"#{value}\"];"
  end
end
puts "}"

