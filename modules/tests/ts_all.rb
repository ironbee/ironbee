#!/usr/bin/env ruby

require '../../clipp/clipp_test'

require 'tc_persistence'
require 'tc_libinjection'
require 'tc_fast'
require 'tc_parser_suite'
require 'tc_modps'
require 'tc_rules'
require 'tc_xrules'
require 'tc_init_collection'
require 'tc_trusted_proxy'
require 'tc_txlog'
require 'tc_ee'
require 'tc_pcre'
require 'tc_abort'
require 'tc_constant'
require 'tc_write_clipp'
require 'tc_stringset'
require 'tc_header_order'
require 'tc_sqltfn'
require 'tc_block'
require 'tc_modhtp'
require 'tc_smart_stringencoders'
require 'tc_utf8'
require 'tc_txvars'

# Conditionally require those module tests that use the optional OpenSSL code.
File.open(File.join(CLIPPTest::TOP_BUILDDIR, "ironbee_config_auto_gen.h")) do |io|
  io.read.split("\n").grep(/HAVE_OPENSSL\s+1/) do
    require 'tc_authscan'
  end
end

# Conditionally require those module tests that use the optional MODP.
File.open(File.join(CLIPPTest::TOP_BUILDDIR, "ironbee_config_auto_gen.h")) do |io|
  io.read.split("\n").grep(/HAVE_MODP\s+1/) do
    require 'tc_stringencoders'
  end
end
