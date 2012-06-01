#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))

MANUAL_DIR   = File.expand_path(File.dirname(__FILE__))
# Use IronBee dictionaries.
ASPELL_DIR   = File.join(MANUAL_DIR, '..', '..', '..', 'codecheck')

def all_xml
    Dir.glob('*.xml').each do |path|
        yield path
    end
end

def spell_check(path)
  system(
    'aspell',
    '--dont-backup',
    '--add-filter',
    'sgml',
    '--mode',
    'sgml',
    '--home-dir',
    ASPELL_DIR,
    '-c',
    path
  )
end


all_xml do |path|
  if ! spell_check(path)
    puts path
    break
  end
end
