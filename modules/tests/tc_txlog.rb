# Integration testing.
class TestTxLog < Test::Unit::TestCase
  include CLIPPTest

  def test_txlog_01
    clipp(
      :input_hashes => [
        simple_hash("GET /?d=eek HTTP/1.1\nHost: any\n\n", "HTTP/1.1 200 OK\nContent-Type: text/html\n\n")
      ],
      :config => '''
        LoadModule "ibmod_htp.so"
        LoadModule "ibmod_txlog.so"
        LoadModule "ibmod_pcre.so"
        LoadModule "ibmod_persistence_framework.so"
        LoadModule "ibmod_init_collection.so"
        LoadModule "ibmod_libinjection.so"
        LoadModule "ibmod_trusted_proxy.so"
        LoadModule "ibmod_xrules.so"

        AuditEngine RelevantOnly
        AuditLogBaseDir .
        AuditLogIndex None
        AuditLogSubDirFormat "events"
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
      ''',
      :default_site_config => '''
        RuleEnable all
      '''
    )

    assert_clean_exit
  end
end