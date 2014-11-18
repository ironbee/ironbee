class TestAuditLogs < CLIPPTest::TestCase
  include CLIPPTest

  def test_tags
    ib_index_log = File.join(BUILDDIR, "ironbee-index.log")
    File.unlink(ib_index_log) if File.exists?(ib_index_log)

    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :config => [
        "AuditLogBaseDir " + BUILDDIR,
      ].join("\n"),
      :default_site_config => <<-EOS
        Rule REQUEST_METHOD @match "GET HEAD" id:1 phase:REQUEST_HEADER clipp_announce:A tag:tag1 tag:tag2 log event
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: A/
    event_file = File.join(
      BUILDDIR,
      File.open(ib_index_log).
        read.split("\n")[-1].
        split(/\s+/)[-1]
    )
    assert(event_file, "Could not open event file")

    event = File.open(event_file).read
    assert(event)
    assert(event =~ /"tags": \[\n\s*"tag1",\n\s*"tag2"\n\s*\],/m)
  end

  def test_fields
    ib_index_log = File.join(BUILDDIR, "ironbee-index.log")
    File.unlink(ib_index_log) if File.exists?(ib_index_log)

    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :config => [
        "AuditLogBaseDir " + BUILDDIR,
      ].join("\n"),
      :default_site_config => <<-EOS
        Rule REQUEST_METHOD @match "GET HEAD" id:1 phase:REQUEST_HEADER clipp_announce:A event
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: A/
    event_file = File.join(
      BUILDDIR,
      File.open(ib_index_log).
        read.split("\n")[-1].
        split(/\s+/)[-1]
    )
    assert(event_file, "Could not open event file")

    event = File.open(event_file).read
    assert(event)
    assert(event !~ /"fields": \[""\],/m, "Empty fields entry in event record detected.")
    assert(event =~ /"fields": \[\n\s*"request_method"\n\s*\],/m)
  end

  def test_log_auditlogs
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :config => [
        "AuditLogBaseDir " + BUILDDIR,
        "LogLevel info",
        "RuleEngineLogData event audit",
      ].join("\n"),
      :default_site_config => <<-EOS
        Rule REQUEST_METHOD @match "GET HEAD" id:1 phase:REQUEST_HEADER clipp_announce:A event
      EOS
    )
    assert_no_issues
    assert_log_match(/ AUDIT /m)
  end

  def test_auditlog_raw_parts
    ib_index_log = File.join(BUILDDIR, "ironbee-index.log")
    File.unlink(ib_index_log) if File.exists?(ib_index_log)

    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :config => [
        "AuditLogBaseDir " + BUILDDIR,
        "AuditLogParts all",
        "AuditLogSubDirFormat \"\"",
        "InspectionEngineOptions all",
      ].join("\n"),
      :default_site_config => <<-EOS
        Rule REQUEST_METHOD @match "GET HEAD" id:1 phase:REQUEST_HEADER event:alert "msg:TEST EVENT"
      EOS
    )
    assert_no_issues
    event_file = File.join(
      BUILDDIR,
      File.open(ib_index_log).
        read.split("\n")[-1].
        split(/\s+/)[-1]
    )
    assert(event_file, "Could not open event file")

    event = File.open(event_file).read
    assert(event)
    assert(event =~ /audit-log-part; name="http-request-header"/m, "Auditlog contains a request header.")
    assert(event =~ /audit-log-part; name="http-request-body"/m, "Auditlog contains a request body.")
    assert(event =~ /audit-log-part; name="http-response-header"/m, "Auditlog contains a response header.")
    assert(event =~ /audit-log-part; name="http-response-body"/m, "Auditlog contains a response body.")
  end

  def test_supressing_auditlog_raw_parts
    ib_index_log = File.join(BUILDDIR, "ironbee-index.log")
    File.unlink(ib_index_log) if File.exists?(ib_index_log)

    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :config => [
        "AuditLogBaseDir " + BUILDDIR,
        "AuditLogParts all",
        "AuditLogSubDirFormat \"\"",
        "InspectionEngineOptions all -requestHeader -requestBody -responseHeader -responseBody",
      ].join("\n"),
      :default_site_config => <<-EOS
        Rule REQUEST_METHOD @match "GET HEAD" id:1 phase:REQUEST_HEADER event:alert "msg:TEST EVENT"
      EOS
    )
    assert_no_issues
    event_file = File.join(
      BUILDDIR,
      File.open(ib_index_log).
        read.split("\n")[-1].
        split(/\s+/)[-1]
    )
    assert(event_file, "Could not open event file")

    event = File.open(event_file).read
    assert(event)
    assert(event !~ /audit-log-part; name="http-request-header"/m, "Auditlog contains a request header.")
    assert(event !~ /audit-log-part; name="http-request-body"/m, "Auditlog contains a request body.")
    assert(event !~ /audit-log-part; name="http-response-header"/m, "Auditlog contains a response header.")
    assert(event !~ /audit-log-part; name="http-response-body"/m, "Auditlog contains a response body.")
  end

end
