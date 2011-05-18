#!/usr/bin/env ruby

# Copyright 2009-2010 Open Information Security Foundation
# Copyright 2010-2011 Qualys, Inc.
#
# Licensed to You under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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


