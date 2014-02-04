# Integration testing.
class TestLuaModule < Test::Unit::TestCase
  include CLIPPTest

  def test_lua_module_set

    mod = File.join(BUILDDIR, "test_lua_set.lua")

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

  # This tests the loading of module files
  # that are not located in the modules directory
  # but in the configuration directory (or
  # relative thereto).
  def test_load_relative_to_config_file
    clipp(
      :input_hashes => [
        simple_hash("GET / HTTP/1.1\nHost: foo.bar\n\n")
      ],
      :config => """
        LoadModule ibmod_lua.so
        LuaLoadModule test_load_relative_to_config_file.lua
      """,
      :default_site_config => '''
      '''
    )

    assert_no_issues
  end
end