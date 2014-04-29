require '../../clipp/clipp_test'

class TestConstant < Test::Unit::TestCase
  include CLIPPTest

  def stringset_clipp(config = {})
    config[:modules] ||= []
    config[:modules] << 'stringset'
    config[:modules] << 'htp'
    clipp(config) do
      transaction {|t| t.request(raw: "GET /")}
    end
  end

  def test_load
    stringset_clipp
    assert_no_issues
  end

  def test_strmatch
    stringset_clipp(
      default_site_config: <<-EOS
        Rule REQUEST_METHOD @strmatch \"Foo Bar GET\" phase:REQUEST_HEADER id:1 clipp_announce:YES
        Rule REQUEST_METHOD @strmatch \"Foo Bar Baz\" phase:REQUEST_HEADER id:2 clipp_announce:NO
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: YES/
    assert_log_no_match /CLIPP ANNOUNCE: NO/
  end

  def test_strmatch_prefix
    stringset_clipp(
      default_site_config: <<-EOS
        Rule REQUEST_METHOD @strmatch_prefix \"Foo Bar G GE\" phase:REQUEST_HEADER id:1 capture clipp_announce:YES=%{CAPTURE}
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: YES=GE/
  end
end
