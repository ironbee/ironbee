#!/usr/bin/env ruby

# Common routines used by various HTML renderers in this directory.
#
# Author: Christopher Alfeld <calfeld@qualys.com>

# Convert dot text to SVG.
def render_dot_as_svg(dot)
  svg = ""
  Open3.popen2(DOT, "-Tsvg") do |i, o|
    o.set_encoding("UTF-8")
    i.write(dot)
    i.close
    o.each_line do |line|
      if line =~ /^<\?xml/ || line =~ /^<!DOCTYPE/ || line =~ /^\s*"http/ || line =~ /^<!--/ || line =~ /^\s*-->$/
        next
      end
      svg += line
    end
  end

  svg
end

# Finds dot graphs in text and yields to block.
#
# For every digraph in text, yields with :dot and the dot.
# For every other line, yields with :line and the line.
def extract_dot(text)
  throw "Block required" if ! block_given?

  ingraph = false
  dot = ""

  text.split("\n").each do |line|
    if ingraph
      dot += line + "\n"
      if line =~ /^}/
        # Done with dot.  Display header.
        yield :dot, dot
        ingraph = false
      end
    else
      if line =~ /^(di)?graph (.*)\{$/
        ingraph = true
        dot = line
      else
        yield :line, line
      end
    end
  end
end

# Expects DATA (part after __END__) to be one or more sections labelled
# with lines <<<NAME>>> where NAME is anything.  Returns a hash mapping NAME
# to the next of those sections.
def split_data
  result = {}
  current = nil
  current_key = nil
  DATA.each_line do |line|
    line.chomp!
    if line =~ /^<<<(.+)>>>$/
      result[current_key] = current
      current = ""
      current_key = $1
    elsif current_key
      current += line + "\n"
    else
      throw "Data before section."
    end
  end

  result
end
