class TestAuditLogs < Test::Unit::TestCase
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
    assert(event =~ /"tags": \["tag1", "tag2"\],/m)
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
        Rule REQUEST_METHOD @match "GET HEAD" id:1 phase:REQUEST_HEADER clipp_announce:A log event
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
    assert(event =~ /"fields": \["request_method"\],/m)


  end

end
