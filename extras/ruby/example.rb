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

config = HTP::Config.new

# Comment out this line and notice that cookies vanish from output.
config.parse_request_cookies = 1

config.register_request do |connp|
  puts "Parsed URI: "
  puts "  " + connp.in_tx.parsed_uri.to_s
  
  # Calling request_headers rubyizes it so we cache the value to avoid
  # paying the cost multiple times.  This repeats in subsequent examples.
  request_headers = connp.in_tx.request_headers
  if request_headers
    puts "Request Headers: "
    request_headers.each {|h| puts "  " + h.to_s}
  end
  
  request_cookies = connp.in_tx.request_cookies
  if request_cookies
    puts "Request Cookies: "
    request_cookies.each {|k,v| puts "#{k} = #{v}"}
  end
end

connp = HTP::Connp.new( config )
input = DATA.read

connp.req_data( Time.now, input )

__END__
POST http://user@password:host/a/b/c?foo=bar#hi HTTP/1.1
User-Agent: Mozilla
Cookie: foo=bar



