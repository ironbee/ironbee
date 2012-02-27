#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))

require 'all-code'

ASPELL_DIR   = File.expand_path(File.dirname(__FILE__))
IRONBEE_DIR  = File.dirname(ASPELL_DIR)

def spell_check(path)
  system(
    'aspell',
    '--add-filter',
    'ccpp',
    '--mode',
    'ccpp',
    '--home-dir',
    ASPELL_DIR,
    '-c',
    path
  )
end


all_ironbee_code do |path|
  if ! spell_check(path)
    puts path
  end
end
