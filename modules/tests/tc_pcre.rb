require '../../clipp/clipp_test'

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

  def test_dfa_reset_non_streaming3
    clipp(
      modules: ['pcre','abort'],
      modhtp: true,
      default_site_config: <<-EOS
        Rule ARGS @dfa "abc" id:1 phase:REQUEST clipp_announce:YES capture "setvar:x=%{capture:0}"
        Rule x @clipp_print "x" id:2 phase:REQUEST
      EOS
    ) do
      transaction do |t|
        t.request(raw:"GET /foo?1=---ab&2=c---&3=-abc- HTTP/1.0")
        t.response(raw:"HTTP/1.0 200 OK")
      end
    end

    assert_no_issues
    assert_log_match /clipp_print \[x\]: abc/
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

  def test_filter_rx_name
    clipp(
      modules: ['pcre'],
      modhtp: true,
      default_site_config: <<-EOS
        Rule ARGS.filterNameRx(^[ab]) @nop "" id:1 phase:REQUEST clipp_announce:Yes=%{FIELD_NAME}
      EOS
    ) do
      transaction do |t|
        t.request(raw:"GET /foo?anvil=1&bar=2&gather=3")
      end
    end
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: Yes=anvil/
    assert_log_match /CLIPP ANNOUNCE: Yes=bar/
    assert_log_no_match /CLIPP ANNOUNCE: Yes=gather/
  end

  def test_filter_rx_value
    clipp(
      modules: ['pcre'],
      modhtp: true,
      default_site_config: <<-EOS
        Rule ARGS.filterValueRx(^[ab]) @nop "" id:1 phase:REQUEST clipp_announce:Yes=%{FIELD_NAME}
      EOS
    ) do
      transaction do |t|
        t.request(raw:"GET /foo?1=anvil&2=bar&3=gather")
      end
    end
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: Yes=1/
    assert_log_match /CLIPP ANNOUNCE: Yes=2/
    assert_log_no_match /CLIPP ANNOUNCE: Yes=3/
  end

  def test_filter_rx_invalid
    clipp(
      modules: ['pcre'],
      modhtp: true,
      default_site_config: <<-EOS
        Rule ARGS.filterValueRx(^[ab) @nop "" id:1 phase:REQUEST clipp_announce:Yes=%{FIELD_NAME}
      EOS
    ) do
      transaction do |t|
        t.request(raw:"GET /foo?1=anvil&2=bar&3=gather")
      end
    end
    assert_log_match /EINVAL/
  end
end
