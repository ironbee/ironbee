class TestStringEncoders < CLIPPTest::TestCase
  include CLIPPTest

  def stringencoders_clipp(uri, config = {})
    config[:modules] ||= []
    config[:modules] << 'stringencoders'
    config[:modules] << 'htp'
    clipp(config) do
      transaction {|t| t.request(raw: "GET #{uri}")}
    end
  end

  def test_load
    stringencoders_clipp('foo')
    assert_no_issues
  end

  def test_b64_decode
    stringencoders_clipp(
      'SGVsbG9Xb3JsZA==',
      default_site_config: <<-EOS
        Rule REQUEST_URI.b64_decode() @clipp_print "DECODED" id:1 phase:REQUEST_HEADER
      EOS
    )
    assert_no_issues
    assert_log_match /DECODED\]: HelloWorld/
  end

  def test_b64w_decode
    stringencoders_clipp(
      'SGVsbG9Xb3JsZA..',
      default_site_config: <<-EOS
        Rule REQUEST_URI.b64w_decode() @clipp_print "DECODED" id:1 phase:REQUEST_HEADER
      EOS
    )
    assert_no_issues
    assert_log_match /DECODED\]: HelloWorld/
  end

  def test_b16_decode
    stringencoders_clipp(
      '48656c6c6f576f726c64',
      default_site_config: <<-EOS
        Rule REQUEST_URI.b16_decode() @clipp_print "DECODED" id:1 phase:REQUEST_HEADER
      EOS
    )
    assert_no_issues
    assert_log_match /DECODED\]: HelloWorld/
  end

  def test_b16_decode_prefix
    stringencoders_clipp(
      '0x480x650x6c0x6c0x6f0x570x6f0x720x6c0x64',
      default_site_config: <<-EOS
        Rule REQUEST_URI.b16_decode(0x) @clipp_print "DECODED" id:1 phase:REQUEST_HEADER
      EOS
    )
    assert_no_issues
    assert_log_match /DECODED\]: HelloWorld/
  end
end
