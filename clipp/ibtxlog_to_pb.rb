#!/usr/bin/env ruby

# Generate CLIPP PB input from IronBee transaction logs.
# Expects exactly one transaction per line.

require 'json'

$:.unshift(File.dirname(__FILE__))

require 'clippscript'
require 'hash_to_pb'

module ClippScript
  class Environment
    def from_ibtxlog(io)
      conn_id = nil
      input = nil
      flush = lambda do
        if input
          input.connection_closed
          emit(input.result)
          input = nil
        end
      end
      convert_headers = lambda do |json|
        result = {}
        json.each do |header|
          result[header['name']] = header['value']
        end
        result
      end
      
      io.each_line do |line|
        json = JSON::parse(line)        
        incoming_conn_id = json['connection']['id']
        if incoming_conn_id != conn_id
          flush[]
          conn_id = incoming_conn_id
          input = proxy(id: conn_id)
        
          input.connection_opened(
            local_ip:    json['connection']['clientIp'],
            local_port:  json['connection']['clientPort'],
            remote_ip:   json['connection']['serverIp'],
            remote_port: json['connection']['serverPort'],
          )
        end
        input.transaction do |t|
          req = json['request']
          t.request(
            method:   req['method'],
            uri:      req['uri'],
            protocol: req['protocol'],
            headers:  convert_headers[req['headers']]
          )
          resp = json['response']
          t.response(
            protocol: resp['protocol'],
            status:   resp['status'],
            message:  resp['message'],
            headers:  convert_headers[resp['headers']]
          )
        end
      end
      
      flush[]
    end
  end
end

if __FILE__ == $0
  if ! ARGV.empty?
    STDERR.puts "Usage: #{$0}"
    STDERR.puts "Example: cat txlog-* | #{$0} | clipp pb:- @parse ironbee:ironbee.conf"
    exit 1
  end
  
  input = STDIN
  ClippScript.eval_io(STDOUT) do
    from_ibtxlog(STDIN)
  end
end
