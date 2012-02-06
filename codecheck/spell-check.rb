#!/usr/bin/env ruby

ASPELL_DIR   = File.expand_path(File.dirname(__FILE__))
IRONBEE_DIR  = File.dirname(ASPELL_DIR)
EXTENSIONS   = ['.h','.c','.hh','.cc','.hpp','.cpp']

def spell_check(path)
  system(
    'aspell',
    '--add-filter',
    'cccpp',
    '--home-dir',
    ASPELL_DIR,
    '-c',
    path
  )
end

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
  EXTENSIONS.each do |e|
    globs << "#{dir}/**/*#{e}"
  end
end
EXTENSIONS.each do |e|
  globs << "tests/*#{e}"
end

globs.each do |glob|
  Dir.chdir(IRONBEE_DIR)
  Dir.glob(glob).each do |path|
    spell_check(path)
  end
end