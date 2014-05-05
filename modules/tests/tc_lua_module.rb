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
        LuaTestDirectiveP1 a_second_string
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
    assert_log_match /Str2 is a_second_string/
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

  def test_set_persisted_collection
    rand_val = rand(100000)
    lua_module_name = File.join(BUILDDIR, "test_set_persisted_collection.lua")
    lua_module = <<-EOLM
      m = ...

      m:request_finished_event(function(tx, event)
        tx:logInfo("Running test with rand_val=#{rand_val}.")

        tx:logInfo("X=%s", tx:get("X"))

        tx:set("A:a", #{rand_val})
        tx:logInfo("A=%s", tx:get("A:a")[1][2])

        tx:logInfo("Collection Y is of size %d", #tx:get("Y"))
        tx:logInfo("Collection Y:y1 is of size %d", #tx:get("Y:y1"))
        return 0
      end)

      return 0
    EOLM

    File.unlink(lua_module_name) if File.exists?(lua_module_name)
    File.open(lua_module_name, 'w') { |io| io.write lua_module }

    clipp(
      modules: %w[ persistence_framework persist init_collection lua ],
      config: """
        PersistenceStore persist persist-fs:///tmp/ironbee
        PersistenceMap A persist key=k expire=10
        InitVar X #{rand_val}
        InitCollection Y vars: y1=1 y2=2
        LuaLoadModule #{lua_module_name}
      """,
      default_site_config: '''
        Rule X @clipp_print "X" id:1 rev:1 phase:REQUEST
        Rule Y @clipp_print "Y" id:2 rev:1 phase:REQUEST
        Rule A @clipp_print "A" id:3 rev:1 phase:REQUEST
      '''
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

    # Check persistence value set by Lua module.
    assert_log_match "clipp_print [X]: #{rand_val}"
    assert_log_match "clipp_print [Y]: 1"
    assert_log_match "clipp_print [Y]: 2"
    assert_log_match "clipp_print [A]: #{rand_val}"
    assert_log_match "Running test with rand_val=#{rand_val}"
    assert_log_match "X=#{rand_val}"
    assert_log_match "A=#{rand_val}"
    assert_log_match "Collection Y is of size 2"
    assert_log_match "Collection Y:y1 is of size 1"
  end

  # Multi elements in the collections persisted.
  def test_set_persisted_collection_multi
    lua_module_name = File.join(BUILDDIR, "test_set_persisted_collection_multi.lua")
    lua_module = <<-EOLM
      m = ...

      m:request_finished_event(function(tx, event)
        tx:set("A:a", 3)
        tx:set("A:b", 5)
        return 0
      end)

      return 0
    EOLM

    File.unlink(lua_module_name) if File.exists?(lua_module_name)
    File.open(lua_module_name, 'w') { |io| io.write lua_module }

    clipp(
      modules: %w[ persistence_framework persist lua ],
      config: """
        PersistenceStore persist persist-fs:///tmp/ironbee
        PersistenceMap A persist key=k expire=10
        LuaLoadModule #{lua_module_name}
      """,
      default_site_config: '''
        Rule A @clipp_print "A REQUEST" id:1 rev:1 phase:REQUEST
        Rule A @clipp_print "A RESPONSE" id:2 rev:1 phase:RESPONSE
      '''
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