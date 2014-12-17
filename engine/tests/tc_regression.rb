require 'fileutils'

class TestRegression < CLIPPTest::TestCase

  parallelize_me!

  include CLIPPTest

  def test_trivial
    clipp(input: "echo:foo")
    assert_no_issues
  end

  def test_body_before_header
    clipp(input: "htp:body_before_header.t")
    assert_no_issues
  end

  def test_empty_header
    clipp(input: "raw:empty_header.req,empty_header.resp")
    assert_no_issues
  end

  def test_http09
    clipp(input: "raw:http09.req,http09.resp")
    assert_no_issues
  end

  def test_manyblank
    clipp(input: "raw:manyblank.req,manyblank.resp")
    assert_no_issues
  end

  def test_basic_rule
    clipp(
      input: "echo:\"GET /foo\"",
      modules: ['pcre'],
      default_site_config: <<-EOS
        Rule REQUEST_METHOD @rx GET id:1 phase:REQUEST_HEADER clipp_announce:basic_rule
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic_rule/
  end

  def test_chained_rule
    clipp(
      default_site_config: <<-EOS
        Rule REQUEST_METHOD @streq NOT_GET id:1 phase:REQUEST_HEADER chain
        Rule REQUEST_METHOD @clipp_print "REQ" clipp_announce:failed
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\r\nHost: foo\r\n\r\n")
      end
    end

    assert_no_issues
    assert_log_no_match /clipp_announce:failed/
  end

  def test_negative_content_length
    request = <<-EOS
      GET / HTTP/1.1
      Content-Length: -1

    EOS
    request.gsub!(/^ +/, "")
    clipp(
      input_hashes: [simple_hash(request)],
      input: "pb:INPUT_PATH @parse"
    )
    assert_no_issues
  end

  def test_negative_content_length2
    request = <<-EOS
      GET / HTTP/1.1
      Content-Length: -1

      Body text!
    EOS
    request.gsub!(/^ +/, "")
    clipp(
      input_hashes: [simple_hash(request)],
      input: "pb:INPUT_PATH @parse"
    )
    assert_no_issues
  end

  def test_rule_engine_log_with_empty_header
    request = <<-EOS
      GET / HTTP/1.1
      Accept-Encoding:

    EOS
    request.gsub!(/^ +/, "")
    clipp(
      input_hashes: [simple_hash(request)],
      input: "pb:INPUT_PATH @parse @fillbody",
      config:  "
        RuleEngineLogData +all
        RuleEngineLogLevel Debug
      ",
      default_site_config: <<-EOS
        Rule REQUEST_HEADERS @nop "" id:1029 rev:1 phase:REQUEST_HEADER tag:HTTP/RepeatedHeader setvar:REQUEST_HEADERS_COUNTS:%{FIELD_NAME}=+1
      EOS
    )
    assert_no_issues
  end

  def test_parse_http09
    request = <<-EOS
      POST /
      This is the body.
    EOS
    request.gsub!(/^ +/, "")
    response = <<-EOS
      This is the body.
    EOS
    response.gsub!(/^ +/, "")
    clipp(
      input_hashes: [simple_hash(request, response)],
      input: "pb:INPUT_PATH @parse",
      consumer: "view"
    )
    assert_no_issues
  end

  def test_ipmatch_09
    clipp(
      input: "echo:\"GET /foo\" @set_remote_ip:6.6.6.6",
      default_site_config: <<-EOS
        Rule REMOTE_ADDR @ipmatch "6.6.6.6" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch_09
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: ipmatch_09/
  end

  def test_ipmatch_11
    request = <<-EOS
GET / HTTP/1.1
Content-Length: 1234

    EOS
    input = [simple_hash(request)]
    clipp(
      input_hashes: input,
      input: "pb:INPUT_PATH @parse @set_remote_ip:6.6.6.6",
      default_site_config: <<-EOS
        Rule REMOTE_ADDR @ipmatch "10.11.12.13 6.6.6.6 1.2.3.4" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch_11a
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: ipmatch_11a/

    clipp(
      input_hashes: input,
      input: "pb:INPUT_PATH @parse @set_remote_ip:6.6.6.6",
      default_site_config: <<-EOS
        Rule REMOTE_ADDR @ipmatch "10.11.12.13 6.6.6.0/24 1.2.3.4" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch_11b
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: ipmatch_11b/

    clipp(
      input_hashes: input,
      input: "pb:INPUT_PATH @parse @set_remote_ip:6.6.6.6",
      default_site_config: <<-EOS
        Rule REMOTE_ADDR @ipmatch "10.11.12.13 6.6.5.0/24 1.2.3.4" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch_11c
      EOS
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: ipmatch_11c/
  end

  def test_ipmatch6_11
    request = <<-EOS
GET / HTTP/1.1
Content-Length: 1234

    EOS
    input = [simple_hash(request)]
    clipp(
      input_hashes: input,
      input: "pb:INPUT_PATH @parse @set_remote_ip:6::6:6",
      default_site_config: <<-EOS
        Rule REMOTE_ADDR @ipmatch6 "1::12:13 6::6:6 1::2:3" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch6_11a
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: ipmatch6_11a/

    clipp(
      input_hashes: input,
      input: "pb:INPUT_PATH @parse @set_remote_ip:6::6:6",
      default_site_config: <<-EOS
        Rule REMOTE_ADDR @ipmatch6 "1::12:13 6::6:0/112 1::2:3" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch6_11b
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: ipmatch6_11b/

    clipp(
      input_hashes: input,
      input: "pb:INPUT_PATH @parse @set_remote_ip:6::6:6",
      default_site_config: <<-EOS
        Rule REMOTE_ADDR @ipmatch6 "1::12:13 6::5:0/112 1::2:3" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch6_11c
      EOS
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: ipmatch6_11c/
  end

  def test_request_body_rule
    s = "POST /a HTTP/1.1\nContent-Type: application/x-www-form-urlencoded\nContent-Length: 19\n\nfoo=bar&hello=world\n"
    clipp(
      input_hashes: [simple_hash(s)],
      modules: ['pcre'],
      default_site_config: "Rule ARGS @rx hello id:8 phase:REQUEST clipp_announce:body"
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: body/
  end

  def test_response_no_response_line
    clipp(
      input_hashes: [simple_hash("GET /\n\n", "\n")]
    )
    assert_no_issues
  end

  def test_no_parsed
    clipp(
      default_site_config: <<-EOS
      Rule REQUEST_METHOD @match "GET HEAD" id:1 phase:REQUEST_HEADER clipp_announce:A
      EOS
    ) do
      transaction do |t|
        t.event(ClippScript::REQUEST_STARTED, raw: 'GET /foo/bar HTTP/1.1')
        t.request_finished
      end
    end
    assert_no_issues
  end

  def test_no_raw
    clipp(
      default_site_config: <<-EOS
      Rule REQUEST_METHOD @match "GET HEAD" id:1 phase:REQUEST_HEADER clipp_announce:A
      EOS
    ) do
      transaction do |t|
        t.event(ClippScript::REQUEST_STARTED,
          method: 'GET',
          uri: '/foo/bar',
          protocol: 'HTTP/1.1'
        )
        t.request_finished
      end
    end
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: A/
  end

  def test_no_tx_finished_event_okay
    clipp(
      log_level: 'debug3'
    ) do
      transaction do |t|
        t.request(
          raw: "GET /"
        )
        t.response(
          raw: "HTTP/1.1 200 OK"
        )
      end
    end
    assert_no_issues
    assert_log_match /TX EVENT: tx_finished_state/
  end

  def test_no_tx_finished_event_fail
    clipp(
      log_level: 'debug3'
    ) do
      transaction do |t|
        t.request(
          raw: "GET /"
        )
      end
    end
    assert_no_issues
    assert_log_match /TX EVENT: tx_finished_state/
  end

  def test_main_action
    clipp(
      config: 'Action id:1 phase:REQUEST clipp_announce:bad',
      default_site_config: 'Action id:2 phase:REQUEST clipp_announce:good'
    ) do
      transaction {|t| t.request(raw: 'GET /')}
    end
    assert_no_issues
    assert_log_no_match(/CLIPP ANNOUNCE: bad/)
    assert_log_match(/CLIPP ANNOUNCE: good/)
  end

  def test_main_rule
    clipp(
      modhtp: true,
      config: 'Rule NULL @nop "" id:1 phase:REQUEST clipp_announce:bad',
      default_site_config: 'Rule NULL @nop "" id:2 phase:REQUEST clipp_announce:good'
    ) do
      transaction {|t| t.request(raw: 'GET /')}
    end
    assert_no_issues
    assert_log_no_match(/CLIPP ANNOUNCE: bad/)
    assert_log_match(/CLIPP ANNOUNCE: good/)
  end

  @@LUA_MODULE_COUNT_STREAM_LEN = <<-EOS
        local m = ...

        m:request_body_data_state(function(ib, data, len)
          local v = ib:get("REQ_LEN") or 0
          v = v + ib.data.ib_request_data_len
          ib:set("REQ_LEN", v)
          ib:logInfo("Request length is %d", v)
          return 0
        end)

        m:response_body_data_state(function(ib, data, len)
          local v = ib:get("RES_LEN") or 0
          v = v + ib.data.ib_response_data_len
          ib:set("RES_LEN", v)
          ib:logInfo("Response length is %d", v)
          return 0
        end)

        m:tx_finished_state(function(tx)
          tx:logInfo("Transaction finished.")
          return 0
        end)

        return 0
  EOS

  def test_core_request_body_log_limit

    eventdir = File.join(BUILDDIR, 'test_core_request_body_log_limit')
    FileUtils.rm_rf(eventdir)
    FileUtils.mkdir_p(eventdir)

    clipp(
      modhtp: true,
      config: """
        RequestBodyLogLimit 1
        AuditLogBaseDir #{eventdir}
        AuditLogIndex index.log
        AuditLogParts requestBody
      """,
      default_site_config: '''
        Action id:1 phase:REQUEST event:alert msg:Boom
      '''
    ) do
      transaction {|t|
        t.request(
          raw: 'PUT / HTTP/1.1',
          headers: {'Host' => "www.myhost.com", 'Content-Length' => 10},
          body: "Some text."
        )
      }
    end

    assert_no_issues
    auditlog = File.open(File.join(eventdir, 'index.log'), 'r').read.split(/ +/)[-1]
    auditlog = File.open(File.join(eventdir, auditlog)).read
    assert_match /^S\r$/m, auditlog
  end

  def test_core_reponse_body_log_limit

    eventdir = File.join(BUILDDIR, 'test_core_response_body_log_limit')
    FileUtils.rm_rf(eventdir)
    FileUtils.mkdir_p(eventdir)

    clipp(
      modhtp: true,
      config: """
        ResponseBodyLogLimit 1
        AuditLogBaseDir #{eventdir}
        AuditLogIndex index.log
        AuditLogParts responseBody
      """,
      default_site_config: '''
        Action id:1 phase:RESPONSE event:alert msg:Boom
      '''
    ) do
      transaction {|t|
        t.request(raw: 'GET / HTTP/1.1', headers: { 'Host' => 'www.example.com' })
        t.response(
          raw: 'OK 200',
          headers: {'Content-Length' => 10},
          body: "Some text."
        )
      }
    end

    assert_no_issues
    auditlog = File.open(File.join(eventdir, 'index.log'), 'r').read.split(/ +/)[-1]
    auditlog = File.open(File.join(eventdir, auditlog)).read
    assert_match /^S\r$/m, auditlog
  end

  def test_core_logwrite_dir
    clipp(
      input: 'echo:',
      config: <<-EOS
        LogWrite DEBUG "Debug message."
        LogWrite INFO "Info message."
        LogWrite ERROR "Error message."
      EOS
    )

    assert_log_no_match /DEBUG\s/
    assert_log_match /INFO\s+-\s+CONFIG Info message\./
    assert_log_match /ERROR\s+-\s+CONFIG Error message\./
  end
end
