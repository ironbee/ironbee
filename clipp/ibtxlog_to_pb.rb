#!/usr/bin/env ruby

# Generate CLIPP PB input from IronBee transaction logs.
# Expects exactly one transaction per line.

require 'json'

$:.unshift(File.dirname(__FILE__))

require 'clippscript'
require 'hash_to_pb'

if ! ARGV.empty?
  STDERR.puts "Usage: #{$0}"
  STDERR.puts "Example: cat txlog-* | #{$0} | clipp pb:- @parse ironbee:ironbee.conf"
  exit 1
end

class ClippEmitter
  def initialize(to)
    @to = to
    @conn_id = nil
    @proxy = nil
  end

  def convert_headers(json)
    result = {}
    json.each do |header|
      result[header['name']] = header['value']
    end
    result
  end

  def transaction(json)
    incoming_conn_id = json['connection']['id']
    if incoming_conn_id != @conn_id
      flush
      @conn_id = incoming_conn_id
      @proxy = ClippScript::proxy(id: @conn_id)

      @proxy.connection_opened(
        local_ip:    json['connection']['clientIp'],
        local_port:  json['connection']['clientPort'],
        remote_ip:   json['connection']['serverIp'],
        remote_port: json['connection']['serverPort'],
      )
    end
    @proxy.transaction do |t|
      req = json['request']
      t.request(
        method:   req['method'],
        uri:      req['uri'],
        protocol: req['protocol'],
        headers:  convert_headers(req['headers'])
      )
      resp = json['response']
      t.response(
        protocol: resp['protocol'],
        status:   resp['status'],
        message:  resp['message'],
        headers:  convert_headers(resp['headers'])
      )
    end
  end

  def flush
    if @proxy
      @proxy.connection_closed
      IronBee::CLIPP::HashToPB::write_hash_to_pb(@to, @proxy.result)
      @proxy = nil
    end
  end
end

input = STDIN

emitter = ClippEmitter.new(STDOUT)
input.each_line do |tx_json|
  tx = JSON.parse(tx_json)
  emitter.transaction(tx)
end
emitter.flush
