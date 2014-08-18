class TestLuaWaggle < Test::Unit::TestCase
  include CLIPPTest

  def make_config(file, extras = {})
    return {
      :config => "LoadModule \"ibmod_lua.so\"\n" +
        "LuaInclude \"#{file}\"\n",
      :default_site_config => "RuleEnable all",
    }.merge(extras)
  end

  SITE_CONFIG = "RuleEnable all"

  def lua_path
    File.expand_path(File.join(BUILDDIR, "lua_test_" + rand(10000).to_s + '.lua'))
  end

  def test_sig
    lua = <<-EOS
      Rule("basic1", "1"):
        fields([[REQUEST_METHOD]]):
        op('imatch', [[GET]]):
        phase([[REQUEST_HEADER]]):
        action([[clipp_announce:basic1]])
    EOS
    lua_file = lua_path()
    File.open(lua_file, 'w') {|fp| fp.print lua}

    clipp(make_config(lua_file,
      :input => "echo:\"GET /foo\""
    ))
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic1/
  end

  def test_transformations
    lua = <<-EOS
      Rule("basic1", "1"):
        fields([[REQUEST_METHOD.removeWhitespace()]]):
        op('imatch', [[GET]]):
        phase([[REQUEST_HEADER]]):
        action([[clipp_announce:basic1]])
    EOS
    lua_file = lua_path()
    File.open(lua_file, 'w') {|fp| fp.print lua}

    clipp(make_config(lua_file,
      :input => "echo:\"GET /foo\""
    ))
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic1/
  end

  def test_action
    lua = <<-EOS
      Action("basic2", "1"):
        phase([[REQUEST_HEADER]]):
        action([[clipp_announce:basic2]])
    EOS
    lua_file = lua_path()
    File.open(lua_file, 'w') {|fp| fp.print lua}

    clipp(make_config(lua_file,
      :input => "echo:\"GET /foo\""
    ))
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic2/
  end

  def test_rule_id_collision
    clipp(
      modules: %w{ lua },
      lua_include: '''
        -- Make rule01.
        Rule("rule01", 1):
          fields("ARGS"):
          op("eq", 0):
          phase("REQUEST"):
          actions("event", "block")

        -- Intentional duplication of above rule.
        Rule("rule01", 1):
          fields("ARGS"):
          op("eq", 0):
          phase("REQUEST"):
          actions("event", "block")

      ''',
      default_site_config: ''
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { 'Host' => 'www.example.com' })
      end
    end

    assert_no_clean_exit
    assert_log_match /LuaAPI - \[ERROR\] Failed to eval Lua DSL for rule rule01 rev 1: Cannot redefine signature\/rule rule01:1\./
  end
end
