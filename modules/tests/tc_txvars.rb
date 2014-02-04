# Integration testing.
class TestLua < Test::Unit::TestCase
  include CLIPPTest

  def test_txvars_lua
    clipp(
      :input_hashes => [
        simple_hash("GET / HTTP/1.1\nHost: foo.bar\n\n")
      ],
      :config => """
        LoadModule    ibmod_lua.so
        LuaLoadModule #{File.join(TOP_SRCDIR, 'modules', 'ibmod_txvars.lua')}
      """,
      :default_site_config => '''
        Rule TX_ID @clipp_print TX_ID id:TX_ID rev:i phase:REQUEST
        Rule CONN_ID @clipp_print CONN_ID id:CONN_ID rev:i phase:REQUEST
        Rule CTX_NAME_FULL @clipp_print CTX_NAME_FULL id:CTX_NAME_FULL rev:i phase:REQUEST
      '''
    )

    assert_no_issues
    assert_log_match /clipp_print \[TX_ID\]: .{8}-.{4}-.{4}-.{4}-.{12}/
    assert_log_match /clipp_print \[CONN_ID\]: .{8}-.{4}-.{4}-.{4}-.{12}/
    assert_log_match /clipp_print \[CTX_NAME_FULL\]: engine:main:main/
  end
end