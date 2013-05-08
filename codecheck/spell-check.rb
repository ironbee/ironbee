#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))

require 'all-code'
require 'open3'

ASPELL_DIR   = File.expand_path(File.dirname(__FILE__))
IRONBEE_DIR  = File.dirname(ASPELL_DIR)

def spell_check(path, list_mode = false)
  cmd = [
    'aspell',
    '--add-filter',
    'ccpp',
    '--mode',
    'ccpp',
    '--home-dir',
    ASPELL_DIR,
  ]
  if ! list_mode
    cmd.concat(['check', path])
    if ! system(*cmd)
      puts path
    end
  else
    cmd.concat(['list'])
    okay = true
    File.open(path, 'r') do |f|
      Open3::popen3(*cmd) do |i, o, e|
        i.write(f.read)
        i.close
        o.each do |line|
          puts "#{path}: #{line}"
          okay = false
        end
      end
      okay
    end
  end
end

if ARGV[0] == '-list'
  list_mode = true
  ARGV.shift
end

  okay = true
if ARGV.empty?
  all_ironbee_code do |path|
    okay &= spell_check(path, list_mode)
  end
else
  ARGV.each do |path|
    okay &= spell_check(path, list_mode)
  end
end

exit 1 if ! okay

