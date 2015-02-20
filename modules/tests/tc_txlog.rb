# Integration testing.
class TestTxLog < CLIPPTest::TestCase
  include CLIPPTest

  def test_txlog_01
    config_text = <<-EOS
        AuditEngine RelevantOnly
        AuditLogBaseDir #{BUILDDIR}
        AuditLogIndex None
        AuditLogSubDirFormat ""
        AuditLogDirMode 0775
        AuditLogFileMode 0664
        AuditLogParts all

        InspectionEngineOptions all
        TxLogIronBeeLog on

        ### Buffering
        RequestBuffering On
        ResponseBuffering On

        ### Rule diagnostics
        RuleEngineLogData all
        RuleEngineLogLevel info

        Rule ARGS @rx foo id:test/1 phase:REQUEST "msg:Matched foo" event
        Rule ARGS @rx bar id:test/2 phase:REQUEST "msg:Matched bar" event block
        Rule ARGS @rx boo id:test/3 phase:REQUEST "msg:Matched boo" event
        Rule ARGS @rx eek id:test/4 phase:REQUEST "msg:Matched eek" event block setvar:FLAGS:block=1
        Rule FLAGS:block.count() @gt 0 id:test/5 phase:REQUEST "msg:Blocking" event:alert block:phase
    EOS
    clipp(
      modules: %w{ htp txlog pcre persistence_framework init_collection libinjection trusted_proxy xrules },
      config: config_text,
      default_site_config: 'RuleEnable all'
    ) do
      transaction do |t|
        t.request(
          raw: "GET /?d=eek HTTP/1.1",
          headers: {
            "Host" => "any"
          }
        )
        t.response(
          raw: "HTTP/1.1 200 OK",
          headers: {
            "Content-Type" => "text/html"
          }
        )
      end
    end

    assert_clean_exit
  end

  def test_txlog_enable
    clipp(
      modules: %w{ header_order txlog },
      config: <<-EOS
        TxLogEnabled on
        TxLogIronBeeLog on
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\r\nHost: foo\r\n\r\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_match /{"timestamp":.*}/
  end

  def test_txlog_customdata
    clipp(
      modules: %w{ header_order txlog },
      config: """
        TxLogEnabled on
        TxLogIronBeeLog on
        TxLogData my_value1 1
        TxLogData request.my_value2 2
        TxLogData response.my_value3 3
        TxLogData security.my_value4 4
        TxLogData connection.my_value5 5
      """,
      default_site_config: ''
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\r\nHost: foo\r\n\r\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_match '"my_value1":"1"'
    assert_log_match '"my_value2":"2"'
    assert_log_match '"my_value3":"3"'
    assert_log_match '"my_value4":"4"'
    assert_log_match '"my_value5":"5"'
  end

  def test_txlog_customdata_expand
    clipp(
      modhtp: true,
      modules: %w{ header_order txlog },
      config: """
        TxLogEnabled on
        TxLogIronBeeLog on
        TxLogData my_value1 %{ARGS}
      """,
      default_site_config: ''
    ) do
      transaction do |t|
        t.request(raw: "GET /?a=b HTTP/1.1\r\nHost: foo\r\n\r\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_match '"my_value1":"b"'
  end


  def test_txlog_off
    clipp(
      modules: %w{ header_order txlog },
      config: """
        TxLogEnabled on
        TxLogIronBeeLog on
      """,
      default_site_config: 'TxLogIronBeeLog off'
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\r\nHost: foo\r\n\r\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_no_match /{"timestamp":.*}/
  end

  def test_txlog_no_header_order
    clipp(
      modules: %w{ txlog },
      config: <<-EOS
        TxLogEnabled on
        TxLogIronBeeLog on
      EOS
    ) do
      transaction do |t|
        t.request(
          raw: "GET / HTTP/1.1",
          headers: { "Host" => 'foo' }
        )
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_log_no_match /"headerOrder":/
  end

  def test_txlog_header_order
    clipp(
      modules: %w{ header_order txlog },
      config: <<-EOS
        TxLogEnabled on
        TxLogIronBeeLog on
      EOS
    ) do
      transaction do |t|
        t.request(
          raw: "GET / HTTP/1.1",
          headers: { "Host" => 'foo' }
        )
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_match /"headerOrder":/
  end

  def test_txlog_bandwidth
    clipp(
      modules: %w{ htp header_order txlog },
      config: <<-EOS
        TxLogEnabled on
        TxLogIronBeeLog on
      EOS
    ) do
      transaction do |t|
        t.request_started(raw: "GET / HTTP/1.1")
        t.request_header(
          headers: {"Host" => "foo", "Content-Length" => "5"}
        )
        t.request_body(data: "12345")
        t.response_started(raw: "HTTP/1.1 200 OK")
        t.response_header(
          headers: {"Content-Length" => "15"}
        )
        t.response_body(data: "54321")
        t.response_body(data: "54321")
        t.response_body(data: "54321")
      end
    end

    assert_no_issues
    # Lengths do not (currently) take into account the HTTP separators in the MIME headers.
    assert_log_match /"bandwidth":41,/
    assert_log_match /"bandwidth":46,/
  end

  def test_txlog_path
    clipp(
      modules: %w{ htp header_order txlog },
      config: <<-EOS
        TxLogEnabled on
        TxLogIronBeeLog on
      EOS
    ) do
      transaction do |t|
        t.request_started(raw: "GET /./foo%20bar/baz/../boo/eek HTTP/1.1")
        t.request_header(
          headers: {"Host" => "foo"}
        )
        t.response_started(raw: "HTTP/1.1 200 OK")
        t.response_header(
          headers: {"Content-Length" => "5"}
        )
        t.response_body(data: "54321")
      end
    end

    assert_no_issues
    # Lengths do not (currently) take into account the HTTP separators in the MIME headers.
    assert_log_match /"path":"\/foo bar\/boo\/eek",/
  end
end
