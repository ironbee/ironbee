# Test features of the core module which may include directives, actions, etc.
class TestActionLua < Test::Unit::TestCase
  include CLIPPTest

  def test_setvar_collection_expansion_assignment

    lua_include = <<-EOS
      --
      -- Duplicate Headers
      --
      Recipe "cat/HTTP/RepeatedHeader" {
       -- First we register existing header name for later analysis
        Rule("qrs/HTTP/InvalidHeader/79", "1"):
           fields('REQUEST_HEADERS'):
           op('!streq', ''):
           phase('REQUEST_HEADER'):
           action('setvar:LIST_HEADER_NUM+=1'):
           action('setvar:LIST_HEADER:%{LIST_HEADER_NUM}=%{FIELD_NAME}'):
           set_rule_meta_fields()
      }
    EOS

    clipp(
      predicate: true,
      modules: %w{htp pcre lua},
      lua_include: lua_include,
      config: '''
        ### Rule diagnostics
        RuleEngineLogData event audit
        RuleEngineLogLevel info
      ''',
      default_site_config: '''
        RuleEnable all
        Rule LIST_HEADER_NUM @clipp_print      LIST_HEADER_NUM id:1 phase:REQUEST_HEADER
        Rule LIST_HEADER_NUM @clipp_print_type LIST_HEADER_NUM id:2 phase:REQUEST_HEADER
        Rule LIST_HEADER     @clipp_print      LIST_HEADER     id:3 phase:REQUEST_HEADER
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET /test.php?param2=cat%20/etc/passwd HTTP/1.0", headers: {'Host' => '127.0.0.1' , 'Connection' => 'close'} )
        t.response(raw: "HTTP/1.1 200 OK", headers: {'Host' => '127.0.0.1', 'Content-Length' => 2 , 'Connection' => 'close'}, body: 'az')
      end
    end

    assert_no_issues
    assert_log_no_match /Action "setvar" returned an error: ENOENT/ms
  end
end
