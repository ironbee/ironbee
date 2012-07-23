$:.unshift(File.dirname(File.dirname(File.expand_path(__FILE__))))
require 'clipp_test'

class TestTesting < Test::Unit::TestCase
  include CLIPPTest

  def test_input_hashes
    clipp(
        :input_hashes => [
          {
            "id" => "input_hash",
            "connection" => {
              "pre_transaction_event" => [
                {
                  "which" => 1,
                  "connection_event" => {
                    "local_ip" => "203.194.118.50",
                    "local_port" => 9665,
                    "remote_ip" => "67.23.33.110",
                    "remote_port" => 443
                  }
                }
              ],
              "transaction" => [
                {
                  "event" => [
                    {
                      "which" => 5,
                      "request_event" => {
                        "raw" => "GET /ssldb/ HTTP/1.1",
                        "method" => "GET",
                        "uri" => "/ssldb/",
                        "protocol" => "HTTP/1.1"
                      }
                    }
                  ]
                }
              ],
              "post_transaction_event" => [
                 {
                   "which" => 4
                 }
              ]
            }
          }
        ],
        :consumer => 'view'
    )
    assert_log_match %r{GET /ssldb/ HTTP/1.1}
  end

  def test_simple_hash
    clipp(
      :input_hashes => [simple_hash("GET /foo HTTP/1.1", "HTTP/1.1 200 OK")],
      :consumer     => 'view'
    )
    assert_log_match %r{GET /foo HTTP/1.1}
  end

  def test_erb
    clipp(
      :input_hashes => [
        simple_hash(
          erb("<%= c[:method] %> /foo HTTP/1.1", :method => 'GET')
        )
      ],
      :consumer => 'view'
    )
    assert_log_match %r{GET /foo HTTP/1.1}
  end

  def test_clipp_header
    clipp(
      :log_level => "alert",
      :input => "echo:\"GET /foo\"",
      :default_site_config => <<-EOS
        Action id:1 phase:REQUEST_HEADER setRequestHeader:X-Foo=bar
      EOS
    )
    assert_log_match /clipp_header/
  end
end
