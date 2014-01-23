require File.join(File.dirname(__FILE__), '..', '..', 'clipp', 'clipp_test')

class TestTesting < Test::Unit::TestCase
  include CLIPPTest

  TEST_DIR = File.dirname(__FILE__)

  def test_logargs
    clipp(
      :modhtp => true,
      :input_hashes => [
        simple_hash(
          [
            "GET /foo HTTP/1.1",
            "User-Agent: RandomAgent"
          ].join("\r\n"),
          "HTTP/1.1 200 OK"
        )
      ],
      #:consumer => 'view',
      :log_level => 'DEBUG',
      :config => [
        'RuleEngineLogLevel INFO',
        'RuleEngineLogData all',
      ].join("\n"),
      :modules => ['pcre'],
      :default_site_config => <<-EOS
        Rule request_uri @rx "f\\x00?oo" id:1 rev:1 phase:REQUEST_HEADER "setRequestHeader:X-Foo=bar"
      EOS
    )
    assert_log_match 'OP rx("f\x00?oo") TRUE'
    assert_log_match 'ACTION setRequestHeader("X-Foo=bar")'
  end

  def test_logargs_waggle01
    clipp(
      :modhtp => true,
      :input_hashes => [
        simple_hash(
          [
            "GET /foo HTTP/1.1",
            "User-Agent: RandomAgent"
          ].join("\r\n"),
          "HTTP/1.1 200 OK"
        )
      ],
      :log_level => 'DEBUG',
      :config => [
        'RuleEngineLogLevel INFO',
        'RuleEngineLogData all',
        'LoadModule "ibmod_lua.so"',
        'LuaInclude %s' % [File.join(TEST_DIR, "waggle01.lua")],
      ].join("\n"),
      :modules => ['pcre'],
      :default_site_config => <<-EOS
        RuleEnable all
      EOS
    )
    assert_log_match 'OP rx("f\x00?oo") TRUE'
    assert_log_match 'ACTION setRequestHeader("X-Foo=bar")'
  end
end
