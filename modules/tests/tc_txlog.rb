# Integration testing.
class TestTxLog < Test::Unit::TestCase
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
        TxLogIronBeeLog

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
      modules: [
        'htp', 'txlog', 'pcre', 'persistence_framework', 'init_collection',
        'libinjection', 'trusted_proxy', 'xrules'
      ],
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
      modules: %w{ txlog },
      config: <<-EOS
        TxLogEnabled enable
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\r\nHost: foo\r\n\r\n")
      end
    end

    assert_no_issues
  end
end