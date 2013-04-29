require File.join(File.dirname(__FILE__), '..', 'clipp_test')

class TestRegression < Test::Unit::TestCase
  include CLIPPTest

  CONFIG = <<-EOS
    LoadModule "ibmod_lua.so"
  EOS

  def test_sig
    lua = <<-EOS
      Sig("basic1", "1"):
        fields([[REQUEST_METHOD]]):
        op('imatch', [[GET]]):
        phase([[REQUEST_HEADER]]):
        action([[clipp_announce:basic1]])
    EOS
    lua_file = File.expand_path(File.join(BUILDDIR, rand(10000).to_s + '.lua'))
    File.open(lua_file, 'w') {|fp| fp.print lua}

    clipp(
      :input => "echo:\"GET /foo\"",
      :config => CONFIG,
      :default_site_config => <<-EOC
          LuaInclude "#{lua_file}"
          LuaCommitRules
          RuleEnable all
      EOC
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic1/
  end

  def test_action
    lua = <<-EOS
      Action("basic2", "1"):
        phase([[REQUEST_HEADER]]):
        action([[clipp_announce:basic2]])
    EOS
    lua_file = File.expand_path(File.join(BUILDDIR, rand(10000).to_s + '.lua'))
    File.open(lua_file, 'w') {|fp| fp.print lua}

    clipp(
      :input => "echo:\"GET /foo\"",
      :config => CONFIG,
      :default_site_config => <<-EOC
          LuaInclude "#{lua_file}"
          LuaCommitRules
      EOC
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic2/
  end
end
