require File.join(File.dirname(__FILE__), '..', 'clipp_test')

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
      print("I AM HERE")
      print(P.Rx('GET', P.Field('REQUEST_METHOD'))())
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
end
