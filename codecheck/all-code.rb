#!/usr/bin/env ruby

# Exclude paths that match any of these regexs.
EXCLUDE = [
    %r{/config-parser\.[ch]$}
]

def all_ironbee_code
  ironbee_dir = File.dirname(File.expand_path(File.dirname(__FILE__)))
  extensions  = ['.h','.c','.hpp','.cpp']

  raise "Missing block." if ! block_given?

  globs = []

  [
    'automata',
    'clipp',
    'cpp',
    'engine',
    'fast',
    'include',
    'modules',
    'plugins',
    'predicate',
    'util',
    'example_modules'
  ].each do |dir|
    extensions.each do |e|
      globs << "#{dir}/**/*#{e}"
    end
  end
  extensions.each do |e|
    globs << "tests/*#{e}"
  end

  current_dir = Dir.pwd
  Dir.chdir(ironbee_dir)
  globs.each do |glob|
    Dir.glob(glob).each do |path|
      next if EXCLUDE.find {|r| r =~ path}
      yield path
    end
  end
  Dir.chdir(current_dir)
end

if __FILE__ == $0
  all_ironbee_code do |x|
    puts x
  end
end
