# Also see test_module_ee_oper.cpp
# TODO: move test_module_ee_oper tests into this file.

class TestEE < Test::Unit::TestCase
  include CLIPPTest

  # echo foo | trie_generator > ee_non_streaming.a
  # ec ee_non_streaming.a
  def test_non_streaming
    clipp(
      modules: ['htp', 'ee'],
      default_site_config: <<-EOS
        LoadEudoxus "test" "ee_non_streaming.e"
        Rule ARGS @ee_match_any test id:1 phase:REQUEST_HEADER "clipp_announce:MATCH=%{FIELD_NAME}"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET /?a=foobaz&b=foobar")
      end
    end
    assert_no_issues
    # Important that both a and b match.
    assert_log_match 'CLIPP ANNOUNCE: MATCH=a'
    assert_log_match 'CLIPP ANNOUNCE: MATCH=b'
  end
end
