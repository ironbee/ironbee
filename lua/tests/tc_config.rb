
class TestConfig < CLIPPTest::TestCase
  include CLIPPTest

  def test_lua_simple_directives
    clipp(
      log_level: 'DEBUG',
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
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end

    assert_no_issues
    assert_log_match '[A]: b'
  end

  def test_lua_site_directives
    clipp(
      log_level: 'DEBUG',
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
          raw: "GET /foo HTTP/1.1"
        )
        t.response(
          raw: 'HTTP/1.1 200 OK'
        )
      end
    end

    assert_no_issues
    assert_log_match 'CLIPP ANNOUNCE: act_1'
    assert_log_match 'CLIPP ANNOUNCE: act_2'
  end

  def test_lua_site_directives_fn
    clipp(
      log_level: 'DEBUG',
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
          raw: "GET /foo HTTP/1.1"
        )
        t.response(
          raw: 'HTTP/1.1 200 OK'
        )
      end
    end

    assert_no_issues
    assert_log_match 'CLIPP ANNOUNCE: act_1'
    assert_log_match 'CLIPP ANNOUNCE: act_2'
  end


  def test_lua_site_directives_table
    clipp(
      log_level: 'DEBUG',
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
          raw: "GET /foo HTTP/1.1"
        )
        t.response(
          raw: 'HTTP/1.1 200 OK'
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
      log_level: 'DEBUG',
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
          raw: "GET /foo HTTP/1.1"
        )
        t.response(
          raw: 'HTTP/1.1 200 OK'
        )
      end
    end

    assert_no_issues
    assert_log_match 'CLIPP ANNOUNCE: act_1'
    assert_log_match 'CLIPP ANNOUNCE: act_2'
  end

  def test_lua_errors
    clipp(
      log_level: 'DEBUG',
      modhtp: true,
      modules: %w{ lua },
      config: '''
        Action id:my_action_1 rev:1 phase:REQUEST clipp_announce:act_1
        Action id:my_action_2 rev:1 phase:REQUEST clipp_announce:act_2
      ''',
      lua_include: %q{
        x = i_broke_it
      }
    ) do
      transaction do |t|
        t.request(
          raw: "GET /foo HTTP/1.1"
        )
        t.response(
          raw: 'HTTP/1.1 200 OK'
        )
      end
    end

    assert_log_match %r{Unknown directive: i_broke_it @ .*clipp_test_lua_errors_\d+_config.lua:2}

  end

  def test_lua_errors_again
    clipp(
      log_level: 'DEBUG',
      modhtp: true,
      modules: %w{ lua },
      config: '''
        Action id:my_action_1 rev:1 phase:REQUEST clipp_announce:act_1
        Action id:my_action_2 rev:1 phase:REQUEST clipp_announce:act_2
      ''',
      lua_include: %q{
        i_broke_it again
      }
    ) do
      transaction do |t|
        t.request(
          raw: "GET /foo HTTP/1.1"
        )
        t.response(
          raw: 'HTTP/1.1 200 OK'
        )
      end
    end

    assert_log_match %r{Error accessing or parsing file .*clipp_test_lua_errors_again_\d+_config.lua}
  end

  def test_lua_block_errors_string
    clipp(
      log_level: 'DEBUG',
      modhtp: true,
      modules: %w{ lua },
      config: '''
        Action id:my_action_1 rev:1 phase:REQUEST clipp_announce:act_1
        Action id:my_action_2 rev:1 phase:REQUEST clipp_announce:act_2
      ''',
      lua_include: %q{
        Site("a") [[
          break_it()
        ]]
      }
    ) do
      transaction do |t|
        t.request(
          raw: "GET /foo HTTP/1.1"
        )
        t.response(
          raw: 'HTTP/1.1 200 OK'
        )
      end
    end

    assert_log_match %r{Unknown directive: break_it @ \[.*break_it\(\).*\]:1}
  end

  def test_lua_block_errors_table
    clipp(
      log_level: 'DEBUG',
      modhtp: true,
      modules: %w{ lua },
      config: '''
        Action id:my_action_1 rev:1 phase:REQUEST clipp_announce:act_1
        Action id:my_action_2 rev:1 phase:REQUEST clipp_announce:act_2
      ''',
      lua_include: %q{
        Site("a") {
          break_it
        }
      }
    ) do
      transaction do |t|
        t.request(
          raw: "GET /foo HTTP/1.1"
        )
        t.response(
          raw: 'HTTP/1.1 200 OK'
        )
      end
    end
    assert_log_match %r{Unknown directive: break_it @ .*clipp_test_lua_block_errors_table_\d+_config.lua:4}
  end

  def test_lua_block_errors_function
    clipp(
      log_level: 'DEBUG',
      modhtp: true,
      modules: %w{ lua },
      config: '''
        Action id:my_action_1 rev:1 phase:REQUEST clipp_announce:act_1
        Action id:my_action_2 rev:1 phase:REQUEST clipp_announce:act_2
      ''',
      lua_include: %q{
        Site("a")(function()
          break_it()
        end)
      }
    ) do
      transaction do |t|
        t.request(
          raw: "GET /foo HTTP/1.1"
        )
        t.response(
          raw: 'HTTP/1.1 200 OK'
        )
      end
    end
    assert_log_match %r{Unknown directive: break_it @ .*/clipp_test_lua_block_errors_function_\d+_config.lua:3}
  end

  def test_lua_action_exists
    clipp(
      modules: %w{ lua },
      config: '',
      lua_include: %q{
        if IB:action_exists("setvar") then
          IB:logInfo("Success setvar")
        end

        if not IB:action_exists("nothere") then
          IB:logInfo("Success nothere")
        end
      }
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end

    assert_no_issues
    assert_log_match "Success setvar"
    assert_log_match "Success nothere"
  end

  def test_lua_module_exists
    clipp(
      modules: %w{ lua },
      config: '',
      lua_include: %q{
        if IB:module_exists("lua") then
          IB:logInfo("Success lua")
        end

        if not IB:module_exists("nothere") then
          IB:logInfo("Success nothere")
        end
      }
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end

    assert_no_issues
    assert_log_match "Success lua"
    assert_log_match "Success nothere"
  end

  def test_lua_operator_exists
    clipp(
      modules: %w{ lua },
      config: '',
      lua_include: %q{
        if IB:operator_exists("eq") then
          IB:logInfo("Success eq")
        end

        if not IB:operator_exists("nothere") then
          IB:logInfo("Success nothere")
        end
      }
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end

    assert_no_issues
    assert_log_match "Success eq"
    assert_log_match "Success nothere"
  end

  def test_lua_transformation_exists
    clipp(
      modules: %w{ lua },
      config: '',
      lua_include: %q{
        if IB:transformation_exists("count") then
          IB:logInfo("Success count")
        end

        if not IB:transformation_exists("nothere") then
          IB:logInfo("Success nothere")
        end
      }
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end

    assert_no_issues
    assert_log_match "Success count"
    assert_log_match "Success nothere"
  end

  def test_lua_directive_exists
    clipp(
      modules: %w{ lua },
      config: '',
      lua_include: %q{
        if CP:directive_exists("LuaInclude") then
          IB:logInfo("Success LuaInclude")
        end

        if not CP:directive_exists("nothere") then
          IB:logInfo("Success nothere")
        end
      }
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end

    assert_no_issues
    assert_log_match "Success LuaInclude"
    assert_log_match "Success nothere"
  end
end