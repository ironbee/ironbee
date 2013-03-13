#!/usr/bin/env ruby

# Copyright (c) 2009-2010 Open Information Security Foundation
# Copyright (c) 2010-2013 Qualys, Inc.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
# - Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.

# - Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.

# - Neither the name of the Qualys, Inc. nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

$:.unshift( File.dirname( __FILE__ ) )

require 'htp'

# parse_uri example.
uri = HTP::parse_uri( "http://host.com/hello/world" )
puts uri

puts "----"

# Config and Connp example.
cfg = HTP::Cfg.new

cfg.server_personality = :apache
cfg.register_urlencoded_parser
# Comment out this line and notice that cookies vanish from output.
cfg.parse_request_cookies = 1

cfg.register_request do |connp|
  tx = connp.in_tx
  
  puts "Parsed URI: "
  puts "  " + tx.parsed_uri
  
  if tx.request_headers
    puts "Request Headers: "
    tx.request_headers.each {|h| puts "  " + h}
  end
  
  if tx.request_cookies
    puts "Request Cookies: "
    tx.request_cookies.each {|k,v| puts "  #{k} = #{v}"}
  end
  
  if tx.request_params_query
    puts "Request Params Query: "
    tx.request_params_query.each {|k,v| puts "  #{k} = #{v}"}
  end

  if tx.request_params_body
    puts "Request Body Query: "
    tx.request_params_body.each {|k,v| puts "  #{k} = #{v}"}
  end
  
  0
end

cfg.register_request_body_data do |tx,data|
  puts "Body Data: #{data}"
  
  0
end

cfg.register_request_file_data do |tx,fileinfo,data|
  puts "File Data for #{fileinfo}: #{data}"
  
  0
end

connp = HTP::Connp.new( cfg )
input = DATA.read

connp.req_data( Time.now, input )

# Non-Callback Interface.
puts "----"

connp.conn.transactions.each do |tx|
  # Might be an empty transaction.
  next if ! tx.request_line
  puts tx
end

__END__
POST http://user@password:host/%61/b/c?foo=bar#hi HTTP/1.1
User-Agent: Mozilla
Cookie: foo=bar
Content-Type: text/plain
Content-Length: 9

Body Text


