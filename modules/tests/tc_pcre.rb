class TestPcre < Test::Unit::TestCase
  include CLIPPTest

  def test_dfa_streaming
    clipp(
      :consumer => 'ironbee:IRONBEE_CONFIG @view:summary @splitdata:1',
      :input_hashes => [simple_hash("GET / HTTP/1.1\nHost: foo.bar\n\n", "HTTP/1.1 200 OK\n\nthisthis_is_a_patternthisthisthis\n\n") ],
      :modules => %w(pcre),
      :config => '''
        ResponseBuffering On
        InspectionEngineOptions all
        InitVar MATCH broken
      ''',
      :default_site_config => <<-EOS
        StreamInspect RESPONSE_BODY_STREAM @dfa "this" id:this rev:1 capture
        Rule "CAPTURE" @clipp_print "MATCH" id:2 rev:1 phase:POSTPROCESS
      EOS
    )

    assert_no_issues
    assert_log_match /(?:.*\[MATCH\]: this){5}/m
    assert_log_no_match /(?:.*\[MATCH\]: this){6}/m
  end

  def test_dfa_reset_non_streaming
    clipp(
      modules: ['pcre'],
      modhtp: true,
      default_site_config: <<-EOS
        Rule ARGS @dfa "abc" id:1 phase:REQUEST clipp_announce:YES
      EOS
    ) do
      transaction do |t|
        t.request(raw:"GET /foo?1=---ab&2=c--- HTTP/1.0")
      end
    end

    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE/
  end

  def test_dfa_multiple_non_streaming2
    clipp(
      modules: ['pcre'],
      modhtp: true,
      default_site_config: <<-EOS
        Rule ARGS @dfa "abc" id:1 phase:REQUEST clipp_announce:YES
      EOS
    ) do
      transaction do |t|
        t.request(raw:"GET /foo?1=---abc---&2=foobar HTTP/1.0")
      end
    end

    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE/
  end

  def test_dfa_multiple_non_streaming
    clipp(
      modules: ['pcre'],
      modhtp: true,
      default_site_config: <<-EOS
        Rule ARGS @dfa "abc" id:1 phase:REQUEST clipp_announce:YES
      EOS
    ) do
      transaction do |t|
        t.request(raw:"GET /foo?1=foobar&2=---abc--- HTTP/1.0")
      end
    end

    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE/
  end
end
