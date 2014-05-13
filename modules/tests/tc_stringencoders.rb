require '../../clipp/clipp_test'

class TestStringEncoders < Test::Unit::TestCase
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
end
