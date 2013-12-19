#!/usr/bin/env ruby

# Generate CLIPP PB input from IronBee audit logs.

# Generates one connection per transaction with connection data events.
# Must use @parse modifier.

# Ruby currently lacks a good multipart parser.  As such, this script works
# off of intimidate knowledge of how IronBee generates audit logs rather than
# a proper parsing of the multipart format.

require 'json'

$:.unshift(File.dirname(__FILE__))

require 'clippscript'
require 'hash_to_pb'

JSON_START = /^{$/
JSON_END = /^}$/
MESSAGE_BOUNDARY = /^MIME-Version: 1.0$/

def split_message(message)
  message.shift while message.first !~ %r{^Content-Type: multipart/mixed; boundary=([-\h]+)$}
  id = $1
  boundary = "--#{id}"

  current = nil
  result = {}
  while ! message.empty?
    line = message.shift
    if line =~ /^Content-Disposition: audit-log-part; name="(.+)"$/
      current = result[$1] = [line]
    elsif line != boundary && current
      current << line
    end
  end

  [id, result]
end

def parse_json(input)
  json = ""
  injson = false
  while ! input.empty?
    line = input.shift
    if ! injson && line =~ JSON_START
      injson = true
      json += line + "\n"
    elsif injson
      json += line + "\n"
      if line =~ JSON_END
        injson = false
        break
      end
    end
  end

  if injson
    throw "Unfinished json block: #{json}"
  end

  JSON.parse(json)
end

def parse_raw(input)
  input.shift while ! input.empty? && input.first != ""
  input.shift

  input.join("\n") + "\n"
end

class ClippEmitter
  # TODO: Make use of connection id to determine connection boundaries.
  # For now, always do a connection boundary.

  def initialize(to)
    @to = to
    @conn_id = nil
    @proxy = nil
  end

  def transaction(id, parsed_message)
    incoming_conn_id = parsed_message["header"]["conn-id"]
    if incoming_conn_id != @conn_id
      flush
      @conn_id = incoming_conn_id
      @proxy = ClippScript::proxy(id: id)

      @proxy.connection_opened(
        local_ip:    parsed_message["http-request-metadata"]["local-addr"],
        local_port:  parsed_message["http-request-metadata"]["local-port"],
        remote_ip:   parsed_message["http-request-metadata"]["remote-addr"],
        remote_port: parsed_message["http-request-metadata"]["remote-port"],
      )
    end
    @proxy.transaction do |t|
      if (parsed_message["http-request-header"])
        t.connection_data_in(data:
          parsed_message["http-request-header"] +
          (parsed_message["http-request-body"] || "")
        )
      end
      if (parsed_message["http-response-header"])
        t.connection_data_out(data:
          parsed_message["http-response-header"] +
          (parsed_message["http-response-body"] || "")
        )
      end
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

def process_message(message, emitter)
  id, parts = split_message(message)

  parsed = {}
  parse_json_section = lambda do |section|
    return nil if ! parts[section]
    parsed[section] = parse_json(parts[section])
  end
  parse_raw_section = lambda do |section|
    return nil if ! parts[section]
    parsed[section] = parse_raw(parts[section])
  end

  parse_json_section["header"]
  parse_json_section["events"]
  parse_json_section["http-request-metadata"]
  parse_json_section["http-response-metadata"]
  parse_raw_section["http-request-header"]
  parse_raw_section["http-request-body"]
  parse_raw_section["http-response-header"]
  parse_raw_section["http-response-body"]

  if ! parsed["header"] || ! parsed["http-request-metadata"]
    STDERR.puts "#{id}: Too little information to generate traffic."
    return
  end

  emitter.transaction(id, parsed)
end

if ! ARGV.empty?
  STDERR.puts "Usage: #{$0}"
  STDERR.puts "Example: cat auditlog-* | #{$0} | clipp pb:- @parse ironbee:ironbee.conf"
  exit 1
end

input = STDIN
emitter = ClippEmitter.new(STDOUT)
message = nil
while ! input.eof?
  line = input.gets.chomp
  if line =~ MESSAGE_BOUNDARY
    process_message(message, emitter) if message
    message = [line]
  elsif message
    message << line
  end
end
process_message(message, emitter) if message
emitter.flush
