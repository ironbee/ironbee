
class TestTesting < Test::Unit::TestCase
  include CLIPPTest

  TEST_DIR = File.dirname(__FILE__)

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

  def test_blocking_api_do_block
    clipp(
      modules: %w{ lua },
      lua_module: '''
        local m = ...

        m:request_finished_state(function(tx)

          tx:disableBlocking()
          if tx:isBlockingEnabled(tx) then
            tx:logInfo("Transaction is blockable.")
          else
            tx:logInfo("Transaction is not blockable.")
          end

          tx:enableBlocking()
          if tx:isBlockingEnabled(tx) then
            tx:logInfo("Transaction is blockable.")
          else
            tx:logInfo("Transaction is not blockable.")
          end

          if tx:isBlocked() then
            tx:logInfo("Transaction is blocked.")
          else
            tx:logInfo("Transaction is not blocked.")
          end

          local rc = tx:block()

          if rc == ffi.C.IB_OK then
            tx:logInfo("Blocked transaction.")
          else
            tx:logInfo("Failed to block transaction.")
          end

          return 0
        end)

        return 0
      ''',
      config: '''
      ''',
      default_site_config: '''
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET /")
        t.response(raw: "HTTP/1.0 200 OK")
      end
    end

    assert_log_match('INFO      -  LuaAPI - [INFO ] Transaction is not blockable.')
    assert_log_match('INFO      -  LuaAPI - [INFO ] Transaction is blockable.')
    assert_log_match('INFO      -  LuaAPI - [INFO ] Transaction is not blocked.')
    assert_log_match(/ERROR     - \[tx:.*\]  clipp_error: 403/)
    assert_log_match('INFO      -  LuaAPI - [INFO ] Blocked transaction.')
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

  def test_ib_list_pair
    clipp(
      modhtp: true,
      modules: %w{ lua },
      config: '',
      lua_include: %q{
        ibutil = require("ironbee/util")

        s1  = "s1"
        s2  = "s2"
        lst = ffi.new("ib_list_t *[1]")
        mm  = ffi.C.ib_engine_mm_main_get(IB.ib_engine)
        rc  = ffi.C.ib_list_create(lst, mm)
        if rc ~= ffi.C.IB_OK then
          error("Not OK")
        end

        ffi.C.ib_list_push(lst[0], ffi.cast("void *", s1))
        ffi.C.ib_list_push(lst[0], ffi.cast("void *", s2))

        for i,j in ibutil.ib_list_pairs(lst[0], "char *") do
          print("Value ".. ffi.string(j))
        end

      },
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1", headers: [ "User-Agent: RandomAgent"] )
      end
    end

    assert_no_issues
    assert_log_match 'Value s1'
    assert_log_match 'Value s2'

  end

  def test_lua_simple_directives
    clipp(
      modhtp: true,
      modules: %w{ lua },
      config: '',
      lua_include: %q{
        -- Lua InitVar
        InitVar("a", "b")

      },
      default_site_config: '''
        Rule a @clipp_print A rev:1 id:1 phase:REQUEST
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1", headers: [ "User-Agent: RandomAgent"] )
      end
    end

    assert_no_issues
    assert_log_match '[A]: b'
  end

  def test_lua_site_directives
    clipp(
      modhtp: true,
      modules: %w{ lua },
      config: '''
        Action id:my_action_1 rev:1 phase:REQUEST clipp_announce:act_1
        Action id:my_action_2 rev:1 phase:REQUEST clipp_announce:act_2
      ''',
      lua_include: %q{
        Site('www.foo.com') [[
          SiteId('006d6c58-5286-11e4-88e4-58b035fe7204')
          Hostname('*')
          Service('*:*')
          RuleEnable('id:my_action_1')
          Location('/foo') [=[
            RuleEnable('id:my_action_2')
          ]=]
        ]]
      }
    ) do
      transaction do |t|
        t.request(
          raw: "GET /foo HTTP/1.1",
          headers: [ "Host: www.foo.com"]
        )
        t.response(
          raw: 'HTTP/1.1 200 OK',
          headers: [
            'Content-Type: text/plain'
          ]
        )
      end
    end

    assert_no_issues
    assert_log_match 'CLIPP ANNOUNCE: act_1'
    assert_log_match 'CLIPP ANNOUNCE: act_2'
  end

  def test_lua_site_directives_fn
    clipp(
      modhtp: true,
      modules: %w{ lua },
      config: '''
        Action id:my_action_1 rev:1 phase:REQUEST clipp_announce:act_1
        Action id:my_action_2 rev:1 phase:REQUEST clipp_announce:act_2
      ''',
      lua_include: %q{
        Site('www.foo.com')(function()
          SiteId('006d6c58-5286-11e4-88e4-58b035fe7204')
          Hostname('*')
          Service('*:*')
          RuleEnable('id:my_action_1')
          Location('/foo')(function()
            RuleEnable('id:my_action_2')
          end)
        end)
      }
    ) do
      transaction do |t|
        t.request(
          raw: "GET /foo HTTP/1.1",
          headers: [ "Host: www.foo.com"]
        )
        t.response(
          raw: 'HTTP/1.1 200 OK',
          headers: [
            'Content-Type: text/plain'
          ]
        )
      end
    end

    assert_no_issues
    assert_log_match 'CLIPP ANNOUNCE: act_1'
    assert_log_match 'CLIPP ANNOUNCE: act_2'
  end


  def test_lua_site_directives_table
    clipp(
      modhtp: true,
      modules: %w{ lua },
      config: '''
        Action id:my_action_1 rev:1 phase:REQUEST clipp_announce:act_1
        Action id:my_action_2 rev:1 phase:REQUEST clipp_announce:act_2
      ''',
      lua_include: %q{
        Site('www.foo.com'){
          SiteId('006d6c58-5286-11e4-88e4-58b035fe7204'),
          Hostname('*'),
          Service('*:*'),
          RuleEnable('id:my_action_1'),
          Location('/foo') {
            RuleEnable('id:my_action_2')
          }
        }
      }
    ) do
      transaction do |t|
        t.request(
          raw: "GET /foo HTTP/1.1",
          headers: [ "Host: www.foo.com"]
        )
        t.response(
          raw: 'HTTP/1.1 200 OK',
          headers: [
            'Content-Type: text/plain'
          ]
        )
      end
    end

    assert_no_issues
    assert_log_match 'CLIPP ANNOUNCE: act_1'
    assert_log_match 'CLIPP ANNOUNCE: act_2'
  end

  # Mixing quotes and tables is allowed so long as the quote is in
  # lower level. This is an accidental feature and should not
  # be maintained if the code is negatively impacted.
  def test_lua_site_directives_table_mix1
    clipp(
      modhtp: true,
      modules: %w{ lua },
      config: '''
        Action id:my_action_1 rev:1 phase:REQUEST clipp_announce:act_1
        Action id:my_action_2 rev:1 phase:REQUEST clipp_announce:act_2
      ''',
      lua_include: %q{
        Site('www.foo.com'){
          SiteId('006d6c58-5286-11e4-88e4-58b035fe7204'),
          Hostname('*'),
          Service('*:*'),
          RuleEnable('id:my_action_1'),
          Location('/foo') [[
            RuleEnable('id:my_action_2')
          ]]
        }
      }
    ) do
      transaction do |t|
        t.request(
          raw: "GET /foo HTTP/1.1",
          headers: [ "Host: www.foo.com"]
        )
        t.response(
          raw: 'HTTP/1.1 200 OK',
          headers: [
            'Content-Type: text/plain'
          ]
        )
      end
    end

    assert_no_issues
    assert_log_match 'CLIPP ANNOUNCE: act_1'
    assert_log_match 'CLIPP ANNOUNCE: act_2'
  end


  def test_lua_errors
    clipp(
      modhtp: true,
      modules: %w{ lua },
      config: '''
        Action id:my_action_1 rev:1 phase:REQUEST clipp_announce:act_1
        Action id:my_action_2 rev:1 phase:REQUEST clipp_announce:act_2
      ''',
      lua_include: %q{
        i broke it
      }
    ) do
      transaction do |t|
        t.request(
          raw: "GET /foo HTTP/1.1",
          headers: [ "Host: www.foo.com"]
        )
        t.response(
          raw: 'HTTP/1.1 200 OK',
          headers: [
            'Content-Type: text/plain'
          ]
        )
      end
    end

    assert_no_issues
    assert_log_match 'CLIPP ANNOUNCE: act_1'
    assert_log_match 'CLIPP ANNOUNCE: act_2'
  end
end
