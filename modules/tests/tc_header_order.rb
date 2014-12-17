class TestHeaderOrder < CLIPPTest::TestCase

  parallelize_me!

  include CLIPPTest

  def header_order_clipp(request_headers, response_headers, config = {})
    config[:modules] ||= []
    config[:modules] << 'header_order'
    config[:modules] << 'htp'
    clipp(config) do
      transaction do |t|
        t.request(
          raw: "GET /",
          headers: request_headers.collect {|k| [k, k]}
        )
        t.response(
          raw: "HTTP/1.0 200 OK",
          headers: response_headers.collect {|k| [k, k]}
        )
      end
    end
  end

  def test_load
    header_order_clipp([], [])
    assert_no_issues
  end

  def test_basic
    header_order_clipp(
      ['Host', 'Accept', 'Referer'],
      ['Location', 'Date'],
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST "clipp_announce:REQUEST=%{REQUEST_HEADER_ORDER}"
        Action id:2 phase:RESPONSE_HEADER "clipp_announce:RESPONSE=%{RESPONSE_HEADER_ORDER}"
      EOS
    )

    assert_log_match /REQUEST=HAR/
    assert_log_match /RESPONSE=AD/
    assert_no_issues
  end

  def test_repeated
    header_order_clipp(
      ['Host', 'Accept', 'Referer', 'Accept'],
      ['Location', 'Date', 'Location'],
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST "clipp_announce:REQUEST=%{REQUEST_HEADER_ORDER}"
        Action id:2 phase:RESPONSE_HEADER "clipp_announce:RESPONSE=%{RESPONSE_HEADER_ORDER}"
      EOS
    )

    assert_log_match /REQUEST=HARA/
    assert_log_match /RESPONSE=ADA/
    assert_no_issues
  end

  def test_off
    header_order_clipp(
      ['Host', 'Accept', 'Referer'],
      ['Location', 'Date'],
      default_site_config: <<-EOS
        HeaderOrderRequest ""
        Action id:1 phase:REQUEST "clipp_announce:REQUEST=%{REQUEST_HEADER_ORDER}"
        Action id:2 phase:RESPONSE_HEADER "clipp_announce:RESPONSE=%{RESPONSE_HEADER_ORDER}"
      EOS
    )

    assert_log_match /REQUEST=$/
    assert_log_match /RESPONSE=AD/
    assert_no_issues
  end

  def test_custom
    header_order_clipp(
      ['Foo', 'Bar', 'Baz'],
      ['Bar', 'Baz', 'Foo'],
      config: "HeaderOrderRequest \"F=Foo B=Bar\ Z=Baz\"\nHeaderOrderResponse \"O=Foo A=Bar\ W=Baz\"\n",
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST "clipp_announce:REQUEST=%{REQUEST_HEADER_ORDER}"
        Action id:2 phase:RESPONSE_HEADER "clipp_announce:RESPONSE=%{RESPONSE_HEADER_ORDER}"
      EOS
    )

    assert_log_match /REQUEST=FBZ/
    assert_log_match /RESPONSE=AWO/
    assert_no_issues
  end

  def test_custom2
    header_order_clipp(
      ['Foo', 'Bar', 'Baz'],
      ['Bar', 'Baz', 'Foo'],
      config: "HeaderOrderRequest \"F=Foo B=Bar\ Z=Baz\"\nHeaderOrderResponse \"O=Foo A=Bar\ W=Baz\"\n",
      default_site_config: <<-EOS
        HeaderOrderRequest "1=Foo 2=Bar 3=Baz"
        HeaderOrderResponse "3=Foo 2=Bar 1=Baz"
        Action id:1 phase:REQUEST "clipp_announce:REQUEST=%{REQUEST_HEADER_ORDER}"
        Action id:2 phase:RESPONSE_HEADER "clipp_announce:RESPONSE=%{RESPONSE_HEADER_ORDER}"
      EOS
    )

    assert_log_match /REQUEST=123/
    assert_log_match /RESPONSE=213/
    assert_no_issues
  end
end
