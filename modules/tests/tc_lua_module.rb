require 'fileutils'

# Integration testing.
class TestLuaModule < Test::Unit::TestCase
  include CLIPPTest

  def test_lua_module_set

    mod = File.join(BUILDDIR, "test_lua_set.lua")
    config = """
        LuaLoadModule #{mod}
        LuaSet #{mod} num 3423
        LuaSet #{mod} str a_string
    """

    clipp(
      modules: ['lua'],
      config: config
    ) do
      transaction do |t|
        t.request(
          method: "GET",
          uri: "/",
          protocol: "HTTP/1.1",
          headers: {"Host" => "foo.bar"}
        )
      end
    end

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
      modules: ['lua'],
      config: "LuaLoadModule test_load_relative_to_config_file.lua\n",
    ) do
      transaction do |t|
        t.request(
          method: "GET",
          uri: "/",
          protocol: "HTTP/1.1",
          headers: {"Host" => "foo.bar"}
        )
      end
    end
    assert_no_issues
  end

  # This tests the loading of module files
  # that are not located in the modules directory, but in another directory.
  def test_load_lua_module_base_path
    lua_modules = File.join(BUILDDIR, "lua_modules")
    FileUtils.mkdir_p(lua_modules)
    File.open(File.join(lua_modules, "module.lua"), 'w') do |io|
      io.write <<-EOS
        m = ...
        return 0
      EOS
    end

    clipp(
      modules: ['lua'],
      config: """
        LuaModuleBasePath #{lua_modules}
        LuaLoadModule module.lua
      """,
    ) do
      transaction do |t|
        t.request(
          method: "GET",
          uri: "/",
          protocol: "HTTP/1.1",
          headers: {"Host" => "foo.bar"}
        )
      end
    end
    assert_no_issues
  end
end