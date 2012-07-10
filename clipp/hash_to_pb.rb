#!/usr/bin/env ruby

$:.unshift(File.dirname(__FILE__))
require 'rubygems'
require 'clipp.pb'
require 'zlib'
require 'stringio'

module IronBee
  module CLIPP
    module HashToPB
      private

      def self.write_pb(io, input)
        data = ""
        out = Zlib::GzipWriter.new(StringIO.new(data))
        input.serialize_to(out)
        out.close

        io.write [data.size].pack("N")
        io.write data
      end

      def self.copy_keys(pb, hash, *keys)
        keys.select {|k| hash.has_key?(k)}.each do |k|
          pb[k] = hash[k]
        end
      end

      def self.copy_event_list(pb_field, events)
        events.each do |event|
          pb_event = IronBee::CLIPP::PB::Event.new
          pb_field << pb_event
          copy_keys(pb_event, event, 'which', 'pre_delay', 'post_delay')
          if event.has_key?('connection_event')
            pb_event.connection_event = IronBee::CLIPP::PB::ConnectionEvent.new
            copy_keys(
              pb_event.connection_event,
              event['connection_event'],
              'local_ip', 'local_port', 'remote_ip', 'remote_port'
            )
          elsif event.has_key?('data_event')
            pb_event.data_event = IronBee::CLIPP::PB::DataEvent.new
            copy_keys(
              pb_event.data_event,
              event['data_event'],
              'data'
            )
          elsif event.has_key?('request_event')
            pb_event.request_event = IronBee::CLIPP::PB::RequestEvent.new
            copy_keys(
              pb_event.request_event,
              event['request_event'],
              'raw', 'method', 'uri', 'protocol'
            )
          elsif event.has_key?('response_event')
            pb_event.response_event = IronBee::CLIPP::PB::ResponseEvent.new
            copy_keys(
              pb_event.response_event,
              event['response_event'],
              'raw', 'protocol', 'status', 'message'
            )
          elsif event.has_key?('header_event')
            pb_event.header_event = IronBee::CLIPP::PB::HeaderEvent.new
            event['header_event']['header'].each do |header|
              pb_header = IronBee::CLIPP::PB::Header.new
              pb_event.header_event.header << pb_header
              copy_keys(pb_header, header, 'name', 'value')
            end
          end
        end
      end

      public

      def self.write_hash_to_pb(io, input_as_hash)
        pb_input = IronBee::CLIPP::PB::Input.new
        copy_keys(pb_input, input_as_hash, 'id')

        pb_input.connection = IronBee::CLIPP::PB::Connection.new

        if input_as_hash['connection'].has_key?('pre_transaction_event')
          copy_event_list(
            pb_input.connection.pre_transaction_event,
            input_as_hash['connection']['pre_transaction_event']
          )
        end

        if input_as_hash['connection'].has_key?('transaction')
          input_as_hash['connection']['transaction'].each do |tx|
            pb_tx = IronBee::CLIPP::PB::Transaction.new
            pb_input.connection.transaction << pb_tx
            copy_event_list(
              pb_tx.event,
              tx['event']
            )
          end
        end

        if input_as_hash['connection'].has_key?('post_transaction_event')
          copy_event_list(
            pb_input.connection.post_transaction_event,
            input_as_hash['connection']['post_transaction_event']
          )
        end

        write_pb(io, pb_input)
      end

      def self.hash_to_pb(input_as_hash)
        result = ""
        write_hash_to_pb(StringIO.new(result), input_as_hash)
        result
      end
    end
  end
end