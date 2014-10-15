
class TestConfig < Test::Unit::TestCase
  include CLIPPTest

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