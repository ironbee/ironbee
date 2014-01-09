# Integration testing.
class TestLuaModule < Test::Unit::TestCase
  include CLIPPTest

  def test_lua_module_set

    mod = File.join(File.absolute_path('.'), "test_lua_set.lua")

    clipp(
      :input_hashes => [
        simple_hash("GET / HTTP/1.1\nHost: foo.bar\n\n")
      ],
      :config => """
        LoadModule ibmod_lua.so
        LuaLoadModule #{mod}
        LuaSet #{mod} num 3423
        LuaSet #{mod} str a_string
      """,
      :default_site_config => '''
      '''
    )

    assert_no_issues
    assert_log_match /Num is 3423/
    assert_log_match /Str is a_string/
  end
end