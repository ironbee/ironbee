# Test how vars integrate with engine.
class TestVars < CLIPPTest::TestCase
  include CLIPPTest

  def test_empty_target
    clipp(
      :input_hashes => [simple_hash("GET /foobar\n", "HTTP/1.1 200 OK\n\n")],
      :default_site_config => <<-EOS
        Rule COL1 @clipp_print 'COL1' id:1 rev:1 phase:REQUEST
      EOS
    )

    assert_log_match /clipp_print \['COL1'\]: NULL/
    assert_no_issues
  end
end