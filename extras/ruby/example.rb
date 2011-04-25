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
config = HTP::Config.new

config.server_personality = :apache
config.register_urlencoded_parser
# Comment out this line and notice that cookies vanish from output.
config.parse_request_cookies = 1

config.register_request do |connp|
  tx = connp.in_tx
  
  puts "Parsed URI: "
  puts "  " + tx.parsed_uri
  
  # Calling request_headers rubyizes it so we cache the value to avoid
  # paying the cost multiple times.  This repeats in subsequent examples.
  request_headers = tx.request_headers
  if request_headers
    puts "Request Headers: "
    request_headers.each {|h| puts "  " + h}
  end
  
  request_cookies = tx.request_cookies
  if request_cookies
    puts "Request Cookies: "
    request_cookies.each {|k,v| puts "  #{k} = #{v}"}
  end
  
  request_params_query = tx.request_params_query
  if request_params_query
    puts "Request Params Query: "
    request_params_query.each {|k,v| puts "  #{k} = #{v}"}
  end

  request_params_body = tx.request_params_body
  if request_params_body
    puts "Request Body Query: "
    request_params_body.each {|k,v| puts "  #{k} = #{v}"}
  end
  
  0
end

config.register_request_body_data do |tx,data|
  puts "Body Data: #{data}"
  
  0
end

config.register_request_file_data do |tx,fileinfo,data|
  puts "File Data for #{fileinfo}: #{data}"
  
  0
end

connp = HTP::Connp.new( config )
input = DATA.read

connp.req_data( Time.now, input )

__END__
POST http://user@password:host/%61/b/c?foo=bar#hi HTTP/1.1
User-Agent: Mozilla
Cookie: foo=bar
Content-Type: text/plain
Content-Length: 9

Body Text


