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
        Rule ARGS @ee test id:1 phase:REQUEST_HEADER "clipp_announce:MATCH=%{FIELD_NAME}"
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

  def test_ee_match_trie
    clipp(
      modules: ['htp', 'ee'],
      default_site_config: <<-EOS
        LoadEudoxus "test" "ee_non_streaming.e"
        Rule ARGS @ee_match test id:1 phase:REQUEST_HEADER "clipp_announce:MATCH=%{FIELD_NAME}"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET /?a=foobaz&b=foobar&c=foo")
      end
    end
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=a/
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=b/
    assert_log_match /CLIPP ANNOUNCE: MATCH=c/
  end

  # echo 'foo' | ac_generator > ac_foo.a
  # ec ac_foo.a
  def test_ee_match_ac
    clipp(
      modules: ['htp', 'ee'],
      default_site_config: <<-EOS
        LoadEudoxus "test" "ac_foo.e"
        Rule ARGS @ee_match test id:1 phase:REQUEST_HEADER "clipp_announce:MATCH=%{FIELD_NAME}"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET /?a=foobaz&b=foobar&c=foo&d=barfoo&e=arf")
      end
    end
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=a/
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=b/
    assert_log_match /CLIPP ANNOUNCE: MATCH=c/
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=d/
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=e/
  end

  # echo 'foo\d\d' | ac_generator -p > ac_foo_pattern.a
  # ec ac_foo_pattern.a
  def test_ee_match_ac_pattern
    clipp(
      modules: ['htp', 'ee'],
      default_site_config: <<-EOS
        LoadEudoxus "test" "ac_foo_pattern.e"
        Rule ARGS @ee test id:1 phase:REQUEST_HEADER "clipp_announce:MATCH=%{FIELD_NAME}"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET /?a=foo17&b=fooba")
      end
    end
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: MATCH=a/
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=b/
  end

  # RNS-839
  def test_ee_module_nothing_to_do
    clipp(
      modules: ['ee']
    ) do
      transaction do |t|
        t.request(
          raw: "GET /"
        )
        t.response(
          raw: "HTTP/1.1 200 OK"
        )
      end
    end
    assert_no_issues
    assert_log_no_match /ENOENT/
  end

  # def test_streaming
  #   clipp(
  #     input: 'pb:INPUT_PATH',
  #     modules: ['htp', 'ee'],
  #     default_site_config: <<-EOS
  #       LoadEudoxus "test" "ee_non_streaming.e"
  #       StreamInspect request_body_stream @ee test capture id:1 \
  #         "clipp_announce:MATCH=%{CAPTURE:0}"
  #     EOS
  #   ) do
  #     transaction do |t|
  #       t.request_started(raw: "GET / HTTP/1.1")
  #       t.request_header(
  #         headers: {"Content-Length" => "6"}
  #       )
  #       "foobar".each_char {|c| t.request_body(data: c)}
  #     end
  #   end
  #   assert_no_issues
  #   assert_log_match 'CLIPP ANNOUNCE: MATCH=foo'
  # end
end
