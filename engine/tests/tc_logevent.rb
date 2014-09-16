require 'fileutils'
require 'json'

class TestCore < Test::Unit::TestCase
  include CLIPPTest

  def test_logevent_generation
    eventdir = File.join(BUILDDIR, 'logevents')
    eventidx = File.join(eventdir, 'idx')
    FileUtils.rm_rf(eventdir)
    FileUtils.mkdir_p(eventdir)

    puts(eventdir)
    clipp(
      config: """
        LogLevel debug
        AuditLogBaseDir #{eventdir}
        AuditLogParts all
        AuditLogIndex #{eventidx}
      """,
      default_site_config: <<-EOS
        Rule REQUEST_LINE @contains "foo" id:1 phase:REQUEST_HEADER block event "msg:Oh no."
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET /foobar/a\nHost: foo.com\n\n")
        t.response(raw: "HTTP/1.1 200 OK\n\n")
      end
    end

    # Before looking at the generated logs, ensure that we're ok.
    assert_clean_exit

    logfile_name   = File.open(eventidx).read.split("\n")[-1].split(" ")[-1]
    logfile        = File.open(File.join(eventdir, logfile_name)).read
    event_section  = ''
    header_section = ''
    separator = /^Content-Type:.* boundary=(\S+)(:?\s|$)/m.match(logfile)[1]
    logfile.split('--'+separator).each do |section|
      section_name = nil
      if section =~ /^Content-Disposition: audit-log-part; name="([^"]+)"/m
        section_name = $~[1]
      end

      if section_name == 'events'
        event_section = section
      elsif section_name == 'header'
        header_section = section
      end
    end

    assert_not_nil(event_section)
    assert_not_nil(header_section)

    puts event_section
    event_json = JSON::parse(event_section.sub(/^[^{]+/m, ''))

    assert_equal(1, event_json['events'].length)
    assert_not_nil(1, event_json['events'][0])
    assert_equal(0, event_json['events'][0]['confidence'])
    assert_equal(0, event_json['events'][0]['severity'])
    assert_equal("NoAction", event_json['events'][0]['rec-action'])
    assert_equal('Oh no.', event_json['events'][0]['msg'])

    puts header_section
    header_json = JSON::parse(header_section.sub(/^[^{]+/m, ''))
    assert_equal(1, header_json['tx-num'])


  end
end
