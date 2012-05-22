#!/usr/bin/env ruby

def all_ironbee_code
  ironbee_dir = File.dirname(File.expand_path(File.dirname(__FILE__)))
  extensions  = ['.h','.c','.hh','.cc','.hpp','.cpp']
  
  raise "Missing block." if ! block_given?
  
  globs = []

  [
    'cpp',
    'cli',
    'engine',
    'include',
    'modules',
    'plugins',
    'util'
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
      next if File.basename(path) =~ %r{^config-parser\.[ch]$}
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
