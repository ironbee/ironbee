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

# Implementation module.  Do not use directly.
module IBAuditToPB
  JSON_START = /^{$/
  JSON_END = /^}$/
  MESSAGE_BOUNDARY = /^MIME-Version: 1.0$/

  def self.split_message(message)
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

  def self.parse_json(input)
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

  def self.parse_raw(input)
    input.shift while ! input.empty? && input.first != ""
    input.shift

    input.join("\n") + "\n"
  end

  def self.process_message(message)
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

    yield id, parsed
  end
end

module ClippScript
  class Environment
    def from_ibaudit(io)
      conn_id = nil
      input = nil
      message = nil
      flush = lambda do
        if input
          input.connection_closed
          emit input.result
          input = nil
        end
      end
      
      process_transaction = lambda do |id, parsed_message|
        incoming_conn_id = parsed_message["header"]["conn-id"]
        if incoming_conn_id != conn_id
          flush
          conn_id = incoming_conn_id
          input = proxy(id: id)

          input.connection_opened(
            local_ip:    parsed_message["http-request-metadata"]["local-addr"],
            local_port:  parsed_message["http-request-metadata"]["local-port"],
            remote_ip:   parsed_message["http-request-metadata"]["remote-addr"],
            remote_port: parsed_message["http-request-metadata"]["remote-port"],
          )
        end
        input.transaction do |t|
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

      io.each_line do |line|
        line.chomp!
        if line =~ IBAuditToPB::MESSAGE_BOUNDARY
          if message
            IBAuditToPB::process_message(message, &process_transaction)
          end
          message = [line]
        elsif message
          message << line
        end
      end
      if message
        IBAuditToPB::process_message(message, &process_transaction)
      end
      flush[]        
    end
  end
end

if __FILE__ == $0
  if ! ARGV.empty?
    STDERR.puts "Usage: #{$0}"
    STDERR.puts "Example: cat auditlog-* | #{$0} | clipp pb:- @parse ironbee:ironbee.conf"
    exit 1
  end

  ClippScript.eval_io(STDOUT) do
    from_ibaudit(STDIN)
  end
end
