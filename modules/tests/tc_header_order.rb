require '../../clipp/clipp_test'

class TestConstant < Test::Unit::TestCase
  include CLIPPTest

  def header_order_clipp(request_headers, response_headers, config = {})
    config[:modules] ||= []
    config[:modules] << 'header_order'
    config[:modules] << 'htp'
    request_header_hash = {}
    request_headers.each {|k| request_header_hash[k] = k}
    response_header_hash = {}
    response_headers.each {|k| response_header_hash[k] = k}
    clipp(config) do
      transaction do |t|
        t.request(
          raw: "GET /",
          headers: request_header_hash
        )
        t.response(
          raw: "HTTP/1.0 200 OK",
          headers: response_header_hash
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
