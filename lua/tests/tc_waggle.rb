class TestWaggle < Test::Unit::TestCase
  include CLIPPTest

  def test_logargs
    clipp(
      modhtp: true,
      #consumer: 'view',
      :config => '''
        RuleEngineLogLevel INFO
        RuleEngineLogData all
      ''',
      modules: %w{ pcre },
      default_site_config: '''
        Rule request_uri @rx "f\\x00?oo" id:1 rev:1 phase:REQUEST_HEADER "setRequestHeader:X-Foo=bar"
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1", headers: [ "User-Agent: RandomAgent"] )
      end
    end
    assert_log_match 'OP rx("f\x00?oo") TRUE'
    assert_log_match 'ACTION setRequestHeader(X-Foo=bar)'
  end

  def test_logargs_waggle01
    clipp(
      modhtp: true,
      modules: %w{ lua pcre },
      config: '''
        RuleEngineLogLevel INFO
        RuleEngineLogData all
      ''',
      lua_include: %q{
        Rule("sig01", 1):
          fields("request_uri"):
          phase("REQUEST_HEADER"):
          op('rx', [[f\x00?oo]]):
          action("setRequestHeader:X-Foo=bar")
        Rule("sig02", 1):
          fields("request_uri"):
          phase("REQUEST_HEADER"):
          op('streq', [[f\x00?oo]]):
          action("setRequestHeader:X-Bar=baz")
      },
      default_site_config:'''
        RuleEnable all
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1", headers: [ "User-Agent: RandomAgent"] )
      end
    end
    assert_log_match 'OP rx("f\x00?oo") TRUE'
    assert_log_match 'ACTION setRequestHeader(X-Foo=bar)'
  end

  def test_unknown_rule_functions_become_actions
    clipp(
      modhtp: true,
      modules: %w{ lua pcre },
      config: '''
        RuleEngineLogLevel INFO
        RuleEngineLogData all
      ''',
      lua_include: %q{
        Rule("sig01", 1):
          fields("request_uri"):
          phase("REQUEST_HEADER"):
          op('rx', [[f\x00?oo]]):
          setRequestHeader("X-Foo=bar"):
          severity('1'):
          confidence('2'):
          message("First event"):
          block():
          event()
        Rule("sig02", 1):
          fields("request_uri"):
          phase("REQUEST_HEADER"):
          op('streq', [[f\x00?oo]]):
          setRequestHeader("X-Bar=baz"):
          severity('3'):
          confidence('4'):
          block():
          message("Second event"):
          event()
      },
      default_site_config:'''
        RuleEnable all
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1", headers: [ "User-Agent: RandomAgent"] )
      end
    end
    assert_log_match 'OP rx("f\x00?oo") TRUE'
    assert_log_match 'ACTION setRequestHeader(X-Foo=bar)'
    assert_log_match 'EVENT main/sig01 Observation NoAction [2/1] [] "First event"'

  end

end