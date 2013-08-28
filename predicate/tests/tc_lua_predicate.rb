class TestLuaPredicate < Test::Unit::TestCase
  include CLIPPTest

  def make_config(file, extras = {})
    return {
      :config => "LoadModule \"ibmod_lua.so\"\n" +
        "LoadModule \"../../predicate/.libs/ibmod_predicate.so\"\n" +
        "LuaInclude \"#{file}\"\n" +
        "LuaCommitRules",
      :default_site_config => "RuleEnable all",
    }.merge(extras)
  end

  def test_basic
    lua = <<-EOS
      Action("basic1", "1"):
        phase([[REQUEST_HEADER]]):
        action([[clipp_announce:basic1]]):
        predicate(P.Rx('GET', P.Field('REQUEST_METHOD')))
    EOS
    lua_file = File.expand_path(File.join(BUILDDIR, rand(10000).to_s + '.lua'))
    File.open(lua_file, 'w') {|fp| fp.print lua}

    clipp(make_config(lua_file,
      :input => "echo:\"GET /foo\""
    ))
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic1/
  end

  def test_template
    lua = <<-EOS
      local getField = P.define('tc_lua_predicate_getField', {'name'},
        P.Field(P.Ref('name'))
      )
      Action("basic1", "1"):
        phase([[REQUEST_HEADER]]):
        action([[clipp_announce:basic1]]):
        predicate(P.Rx('GET', getField('REQUEST_METHOD')))
    EOS
    lua_file = File.expand_path(File.join(BUILDDIR, rand(10000).to_s + '.lua'))
    File.open(lua_file, 'w') {|fp| fp.print lua}

    clipp(make_config(lua_file,
      :input => "echo:\"GET /foo\""
    ))
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic1/
  end
end
