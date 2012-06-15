#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))

require 'all-code'

all_ironbee_code do |path|
  if path =~ /\.c$/ && path !~ /modules/ && path !~ /plugins/
    IO.foreach( path ) do |line|
      if line =~ /^#include/
        if line !~ /^#include "ironbee_config_auto.h"/
          puts path
        end
        break
      end
    end
  end
end
