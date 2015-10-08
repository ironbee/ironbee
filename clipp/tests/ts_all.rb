#!/usr/bin/env ruby

require '../clipp_test'

require 'tc_testing'

defs =
  # Take the .h file and split all the 3-token #define lines into a hash.
  File.open(File.join(CLIPPTest::TOP_BUILDDIR, 'ironbee_config_auto_gen.h')).reduce({}) do |x,y|
    d, name, str = y.split(' ', 3)
    x[name] = 1
    x
  end

# Only run tc_burp if we have LIBXML2.
require 'tc_burp'if defs.has_key? 'HAVE_LIBXML2'
