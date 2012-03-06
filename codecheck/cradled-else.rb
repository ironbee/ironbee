#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))

require 'all-code'

all_ironbee_code do |path|
  system(
    'grep',
    '-H',
    '-n',
    '} *else',
    path
  )
end