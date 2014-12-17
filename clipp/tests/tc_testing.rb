require File.join(File.dirname(__FILE__), '..', 'ibtxlog_to_pb.rb')

class TestTesting < CLIPPTest::TestCase
  include CLIPPTest

  def test_clippscript
    clipp(consumer: 'view') do
      transaction do |t|
        t.request(
          raw: "GET /foo HTTP/1.1",
          headers: {
            'Host'           => 'foo.com',
            'Content-Length' => 0
          }
        )
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end
    assert_log_match %r{GET /foo HTTP/1.1}
    assert_log_match %r{Host: foo.com}
    assert_log_match %r{Content-Length: 0}
    assert_log_match %r{HTTP/1.1 200 OK}
  end

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

  def test_log_count
    clipp(
      :input_hashes => [simple_hash("GET /foo HTTP/1.1", "HTTP/1.1 200 OK")],
      :consumer     => 'view'
    )
    assert log_count(%r{GET /foo HTTP/1.1}) > 0
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
      :input => "echo:\"GET /foo\"",
      :default_site_config => <<-EOS
        Action id:1 phase:REQUEST_HEADER setRequestHeader:X-Foo=bar
      EOS
    )
    assert_log_match /clipp_header/
  end

  def test_input_asserts
    clipp(
      :default_site_config => <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:A
      EOS
    ) do
      transaction(id: 'id1') do |t|
        t.request(raw: "GET /1")
      end
      transaction(id: 'id2') do |t|
        t.request(raw: "GET /2")
      end
      transaction(id: 'id3') do |t|
        t.request(raw: "GET /3")
      end
    end
    assert_log_every_input_match /CLIPP ANNOUNCE: A/
  end

  def test_add
    clipp(consumer: 'view @add:Foo:Bar') do
      transaction do |t|
        t.request(
          raw: "GET / HTTP/1.1",
          headers: {}
        )
      end
    end
    assert_log_match(/^Foo:\s*Bar/)
  end

  def test_addmissing
    clipp(consumer: 'view @addmissing:Foo:Bar @addmissing:Baz:Buz') do
      transaction do |t|
        t.request(
          raw: "GET / HTTP/1.1",
          headers: {"Foo" => "SomethingElse"}
        )
      end
    end
    assert_log_no_match(/^Foo:\s*Bar/)
    assert_log_match(/^Baz:\s*Buz/)
  end
  def test_headerless
    clipp(
      :config => "LoadModule \"ibmod_htp.so\"",
      :default_site_config => <<-EOS
        Rule REQUEST_URI_PARAMS @match "bar" id:1 phase:REQUEST_HEADER clipp_announce:A
      EOS
    ) do
      transaction do |t|
        t.request(
          raw: "GET /foo?x=bar HTTP/1.1"
        )
      end
    end
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: A/
  end
  def test_args_simple
    clipp(
      :config => "LoadModule \"ibmod_htp.so\"",
      default_site_config: <<-EOS
        Rule ARGS @match "baz" id:1 phase:REQUEST clipp_announce:A
        Rule REQUEST_BODY_PARAMS @match "baz" id:2 phase:REQUEST clipp_announce:B
      EOS
    ) do
      transaction do |t|
        t.request(
          raw: "POST /foo HTTP/1.1",
          headers: {
            "Content-Type" => "application/x-www-form-urlencoded",
            "Content-Length" => 5
          },
          body: "x=baz"
        )
      end
    end
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: A/
    assert_log_match /CLIPP ANNOUNCE: B/
  end
  def test_site_selection
    clipp(
      :config => <<-EOS,
        LoadModule "ibmod_htp.so"
        LogLevel debug3
        <Site should_be_selected>
          SiteId bf9b5c39-c94e-4d9c-89a6-8d17cfd92911
          Service *:*
          Hostname my.testsite.tld
          Action id:1 phase:REQUEST_HEADER clipp_announce:A
        </Site>
      EOS
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:B
      EOS
    ) do
      transaction do |t|
        t.request(
          raw: "GET /foo HTTP/1.1",
          headers: {
            'Host'           => 'my.testsite.tld',
          }
        )
      end
    end
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: A/
    assert_log_no_match /CLIPP ANNOUNCE: B/
  end

  def test_request_line_parsing
    clipp(
      consumer: 'view @parse'
    ) do
      transaction do |t|
        t.connection_data_in(data: "GET foo\rbar HTTP/1.1\r\n")
      end
    end
    assert_log_match '=== REQUEST_STARTED: GET foo[0d]bar HTTP/1.1 ==='
  end

  def test_ibtxlog
    json_txlog = <<-EOS
{"timestamp":"2014-01-31T17:26:00.553-00:00","duration":24,"id":"16029352-4c91-461f-a2b6-318100ad0400","clientIp":"127.0.0.1","clientPort":60619,"sensorId":"E2D71E93-9B86-44ED-978D-8AE6FE9C0330","siteId":"9e8d34a4-1431-4a90-a79a-de9fe88fb111","connection":{"id":"984314a6-ddc0-414e-9a98-596aa8362105","clientIp":"127.0.0.1","clientPort":60619,"serverIp":"127.0.0.1","serverPort":8080},"request":{"method":"GET","uri":"/foobar","protocol":"HTTP/1.1","host":"127.0.0.1","bandwidth":0,"headers":[{"name":"User-Agent","value":"curl/7.34.0"}],"action":"Passed"},"response":{"protocol":"HTTP/1.1","status":"200","message":"OK","bandwidth":0,"headers":[{"name":"Server","value":"Apache/2.2.15 (CentOS)"},{"name":"Content-Type","value":"text/html; charset=UTF-8"}],"action":"Passed"},"security":{"auditLogRef":"","events":[]}}
    {"timestamp":"2014-01-31T17:26:00.588-00:00","duration":25,"id":"81512b4c-00a7-4ee1-a4ad-fe52aec72e5d","clientIp":"127.0.0.1","clientPort":60621,"sensorId":"E2D71E93-9B86-44ED-978D-8AE6FE9C0330","siteId":"9e8d34a4-1431-4a90-a79a-de9fe88fb111","connection":{"id":"885cd6e3-3808-408d-b1f7-c31dcee6a6e9","clientIp":"127.0.0.1","clientPort":60621,"serverIp":"127.0.0.1","serverPort":8080},"request":{"method":"GET","uri":"/helloworld","protocol":"HTTP/1.1","host":"127.0.0.1","bandwidth":0,"headers":[{"name":"User-Agent","value":"curl/7.34.0"}],"action":"Passed"},"response":{"protocol":"HTTP/1.1","status":"200","message":"OK","bandwidth":0,"headers":[{"name":"Server","value":"Apache/2.2.15 (CentOS)"},{"name":"Content-Type","value":"text/html; charset=UTF-8"}],"action":"Passed"},"security":{"auditLogRef":"c11ee260-5847-44c0-b1e8-0fd22b2de5e4","events":[{"tags":["cat/test"],"locations":["ARGS:a"],"rule":"main/test/1","message":"Matched foo","confidence":25,"severity":50,"id":"2008773884"}]}}
EOS
    clipp(
      consumer: 'view'
    ) do
      from_ibtxlog(json_txlog)
    end
    assert_log_match '/foobar'
    assert_log_match '/helloworld'
  end
end
