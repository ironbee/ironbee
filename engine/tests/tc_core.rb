# Test features of the core module which may include directives, actions, etc.
class TestCore < Test::Unit::TestCase
  include CLIPPTest

  def test_blocking_method_status_403
    clipp(
      :consumer => 'ironbee:IRONBEE_CONFIG @view',
      :input_hashes => [simple_hash("GET /foobar/a\n", "HTTP/1.1 200 OK\n\n")],
      :config => """
        BlockingMethod status=403
      """,
      :default_site_config => <<-EOS
        Action id:1 rev:1 phase:REQUEST_HEADER block:immediate
      EOS
    )
    assert_log_match /clipp_error: 403/
  end

  def test_blocking_method_status_200
    clipp(
      :consumer => 'ironbee:IRONBEE_CONFIG @view',
      :input_hashes => [simple_hash("GET /foobar/a\n", "HTTP/1.1 200 OK\n\n")],
      :config => """
        BlockingMethod status=200
      """,
      :default_site_config => <<-EOS
        Action id:1 rev:1 phase:REQUEST_HEADER block:immediate
      EOS
    )
    assert_log_match /clipp_error: 200/
  end
end
