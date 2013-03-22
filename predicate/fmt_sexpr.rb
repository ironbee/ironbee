#!/usr/bin/env ruby

module IronBee
module Predicate

# Format sexpr in multiline format for easy human consumption.
def self.fmt_sexpr(s)
  indent = 0
  r = ''
  s = s.dup
  while ! s.empty?
    line = s.slice!(/^[^()]+/)     ||
           s.slice!(/^\([^()]+\)/) ||
           s.slice!(/^\([^ ]+ ?/)  ||
           s.slice!(/^\)/)

    if line.nil?
      throw "Insanity error on #{s}"
    end

    if line == ')'
      indent -= 2
    end
    r += (' '*indent) + line + "\n"
    if line[0..0] == '(' && line[-1..-1] != ')'
      indent += 2
    end
  end

  r
end

end
end

if __FILE__ == $0
  STDIN.each do |line|
    puts IronBee::Predicate::fmt_sexpr(line.chomp)
  end
end
