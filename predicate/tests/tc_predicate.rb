require File.join(File.dirname(__FILE__), '..', '..', 'clipp', 'clipp_test')

class TestPredicate < Test::Unit::TestCase
  include CLIPPTest

  def make_request(s)
    simple_hash("GET /#{s}/a HTTP/1.1\nHost: foo.bar\n\n")
  end

  def test_load
    clipp(
      predicate: true,
      input_hashes: [make_request('foobar')],
      modules: ['pcre'],
      default_site_config: <<-EOS
        Rule REQUEST_URI_RAW @rx foobar id:1 phase:REQUEST_HEADER clipp_announce:basic_rule
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic_rule/
  end

  def test_field_present
    clipp(
      predicate: true,
      modules: ['htp'],
      input_hashes: [make_request('foobar')],
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:field_present "predicate:(var 'REQUEST_URI')"
      EOS
    )
    assert_no_issues
    assert_log_no_match /NOTICE/
    assert_log_match /CLIPP ANNOUNCE: field_present/
  end

  def test_two_rules
    clipp(
      predicate: true,
      modules: ['htp'],
      input_hashes: [make_request('foobar')],
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:field_present "predicate:(var 'REQUEST_URI')"
        Action id:2 phase:REQUEST_HEADER clipp_announce:field_not_present "predicate:(var 'FOOBAR')"
      EOS
    )
    assert_no_issues
    assert_log_no_match /NOTICE/
    assert_log_match /CLIPP ANNOUNCE: field_present/
    assert_log_no_match /CLIPP ANNOUNCE: field_not_present/
  end

  def test_context
    foo_config = <<-EOS
      <Site foo>
          SiteId 26058ae0-22e4-0131-3b7a-001f5b320164
          Hostname foo
          Action id:2 phase:REQUEST_HEADER clipp_announce:foo "predicate:(var 'REQUEST_URI')"
      </Site>
    EOS
    clipp(
      predicate: true,
      modules: ['htp'],
      input_hashes: [make_request('foobar')],
      config_trailer: foo_config,
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:default "predicate:(var 'REQUEST_URI')"
      EOS
    )
    assert_no_issues
    assert_log_no_match /NOTICE/
    assert_log_match /CLIPP ANNOUNCE: default/
    assert_log_no_match /CLIPP ANNOUNCE: foo/
  end

  def test_debug_report
    clipp(
      predicate: true,
      input_hashes: [make_request('foobar')],
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:field_present "predicate:(var 'REQUEST_URI')"
        PredicateDebugReport ""
      EOS
    )
    assert_no_issues
    assert_log_no_match /NOTICE/
  end

  def test_define
    clipp(
      predicate: true,
      modules: ['htp'],
      input_hashes: [make_request('foobar')],
      default_site_config: <<-EOS
        PredicateDefine "foo" "name" "(var (ref 'name'))"
        Action id:1 phase:REQUEST_HEADER clipp_announce:field_present "predicate:(foo 'REQUEST_URI')"
      EOS
    )
    assert_no_issues
    assert_log_no_match /NOTICE/
    assert_log_match /CLIPP ANNOUNCE: field_present/
  end

  def test_template_transform
    clipp(
      predicate: true,
      modules: ['htp'],
      input_hashes: [make_request('foobar')],
      default_site_config: <<-EOS
        PredicateDefine "foo" "x" "(eq 'foo' (transformation 'name' '' (ref 'x')))"
        Action id:1 phase:REQUEST_HEADER clipp_announce:field_present "predicate:(foo 'bar')"
        Action id:2 phase:REQUEST_HEADER clipp_announce:field_present "predicate:(transformation 'name' '' 'bar')"
      EOS
    )
    assert_no_issues
  end

  def test_params
    clipp(
      predicate: true,
      input_hashes: [make_request('hello?a=foo&b=bar')],
      modules: ['pcre', 'htp'],
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:field_present "predicate:(operator 'rx' 'foo|bar' (p 'args=' (var 'request_uri_params')))"
      EOS
    )
    assert_no_issues
    assert_log_no_match /NOTICE/
    assert_log_match /CLIPP ANNOUNCE: field_present/
  end

  def test_phaseless
    clipp(
      predicate: true,
      modules: ['htp'],
      input_hashes: [make_request('foobar')],
      default_site_config: <<-EOS
        Action id:1 clipp_announce:field_present "predicate:(var 'REQUEST_URI')"
      EOS
    )
    assert_no_issues
    assert_log_no_match /NOTICE/
    assert_log_match /CLIPP ANNOUNCE: field_present/
  end

  def test_trace
    clipp(
      predicate: true,
      default_site_config: <<-EOS
       Action id:1 phase:REQUEST_HEADER clipp_announce:a "predicate:(eq 'foo' (namedi 'x' (var 'ARGS')))"
       Action id:2 phase:REQUEST_HEADER clipp_announce:b "predicate:(eq 'bar' (namedi 'x' (var 'ARGS')))"
       Action id:3 clipp_announce:c "predicate:(or (eq 'bar' (namedi 'x' (var 'ARGS'))) (eq 'bar' (namedi 'y' (var 'ARGS'))))"
       PredicateTrace -
      EOS
    ) do
      transaction do |t|
        t.request(
          raw: 'POST /a/b/c?x=bar&y=foo HTTP/1.1',
          headers: {
            "Content-Type" => "application/x-www-form-urlencoded",
            "Content-Length" => 5
          },
          body: "y=bar"
        )
      end
    end
    assert_no_issues
    assert_log_match /PredicateTrace/
  end

  def test_trace_single
    clipp(
      predicate: true,
      default_site_config: <<-EOS
       Action id:1 phase:REQUEST_HEADER clipp_announce:a "predicate:(eq 'foo' (namedi 'x' (var 'ARGS')))"
       Action id:2 phase:REQUEST_HEADER clipp_announce:b "predicate:(eq 'bar' (namedi 'x' (var 'ARGS')))"
       Action id:3 clipp_announce:c "predicate:(or (eq 'bar' (namedi 'x' (var 'ARGS'))) (eq 'bar' (namedi 'y' (var 'ARGS'))))"
       PredicateTrace - 1 2
      EOS
    ) do
      transaction do |t|
        t.request(
          raw: 'POST /a/b/c?x=bar&y=foo HTTP/1.1',
          headers: {
            "Content-Type" => "application/x-www-form-urlencoded",
            "Content-Length" => 5
          },
          body: "y=bar"
        )
      end
    end
    assert_no_issues
    assert_log_match /PredicateTrace/
    assert_log_no_match %r{site/57f2b6d0-7783-012f-86c6-001f5b320164/3}
  end

  def test_context_phaseless
    foo_config = <<-EOS
      Action id:1 clipp_announce:outer "predicate:(true)"
      <Location /foo>
          Action id:2 phase:REQUEST_HEADER clipp_announce:inner "predicate:(true)"
      </Location>
    EOS
    clipp(
      predicate: true,
      modules: ['htp'],
      default_site_config: foo_config
    ) do
      transaction do |t|
        t.request(
          raw: "GET /foo HTTP/1.1"
        )
      end
    end
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: outer/
    assert_log_match /CLIPP ANNOUNCE: inner/
  end

  def test_enable_disable
    clipp(
      predicate: true,
      modules: ['htp'],
      input_hashes: [make_request('foobar')],
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:yes "predicate:(var 'REQUEST_URI')"
        Action id:2 phase:REQUEST_HEADER clipp_announce:no "predicate:(var 'REQUEST_URI')"
        RuleDisable id:2
      EOS
    )
    assert_no_issues
    assert_log_no_match /NOTICE/
    assert_log_match /CLIPP ANNOUNCE: yes/
    assert_log_no_match /CLIPP ANNOUNCE: no/
  end

  def test_multiple_fires_phaseless
    clipp(
      predicate: true,
      modules: ['htp'],
      default_site_config: <<-EOS
        Action id:1 clipp_announce:X "predicate:(var 'ARGS')"
      EOS
    ) do
      transaction do |t|
        t.request(
          raw: 'POST /a/b/c?x=bar&y=foo HTTP/1.1',
          headers: {
            "Content-Type" => "application/x-www-form-urlencoded",
            "Content-Length" => 5
          },
          body: "y=bar"
        )
      end
    end

    assert(log.scan(/CLIPP ANNOUNCE: X/).size == 3)
  end

  def test_multiple_fires_phased
    clipp(
      predicate: true,
      modules: ['htp'],
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:X "predicate:(var 'ARGS')"
      EOS
    ) do
      transaction do |t|
        t.request(
          raw: 'POST /a/b/c?x=bar&y=foo HTTP/1.1',
          headers: {
            "Content-Type" => "application/x-www-form-urlencoded",
            "Content-Length" => 5
          },
          body: "y=bar"
        )
      end
    end

    assert(log.scan(/CLIPP ANNOUNCE: X/).size == 2)
  end

  def test_set_predicate_vars_phaseless
    clipp(
      predicate: true,
      modules: ['htp'],
      default_site_config: <<-EOS
        Action id:1 set_predicate_vars "predicate:(var 'ARGS')" "clipp_announce:PREDICATE_VALUE=%{PREDICATE_VALUE} PREDICATE_VALUE_NAME=%{PREDICATE_VALUE_NAME}"
      EOS
    ) do
      transaction do |t|
        t.request(
          raw: 'POST /a/b/c?x=bar&y=foo HTTP/1.1',
          headers: {
            "Content-Type" => "application/x-www-form-urlencoded",
            "Content-Length" => 5
          },
          body: "y=bar"
        )
      end
    end

    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: PREDICATE_VALUE=bar PREDICATE_VALUE_NAME=x/
    assert_log_match /CLIPP ANNOUNCE: PREDICATE_VALUE=foo PREDICATE_VALUE_NAME=y/
    assert_log_match /CLIPP ANNOUNCE: PREDICATE_VALUE=bar PREDICATE_VALUE_NAME=y/
  end

  def test_set_predicate_vars_phased
    clipp(
      predicate: true,
      modules: ['htp'],
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST_HEADER set_predicate_vars "predicate:(var 'ARGS')" "clipp_announce:PREDICATE_VALUE=%{PREDICATE_VALUE} PREDICATE_VALUE_NAME=%{PREDICATE_VALUE_NAME}"
      EOS
    ) do
      transaction do |t|
        t.request(
          raw: 'POST /a/b/c?x=bar&y=foo HTTP/1.1',
          headers: {
            "Content-Type" => "application/x-www-form-urlencoded",
            "Content-Length" => 5
          },
          body: "y=bar"
        )
      end
    end

    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: PREDICATE_VALUE=bar PREDICATE_VALUE_NAME=x/
    assert_log_match /CLIPP ANNOUNCE: PREDICATE_VALUE=foo PREDICATE_VALUE_NAME=y/
    assert_log_no_match /CLIPP ANNOUNCE: PREDICATE_VALUE=bar PREDICATE_VALUE_NAME=y/
  end

  def test_set_predicate_vars_phased2
    clipp(
      predicate: true,
      modules: ['htp'],
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST set_predicate_vars "predicate:(var 'ARGS')" "clipp_announce:PREDICATE_VALUE=%{PREDICATE_VALUE} PREDICATE_VALUE_NAME=%{PREDICATE_VALUE_NAME}"
      EOS
    ) do
      transaction do |t|
        t.request(
          raw: 'POST /a/b/c?x=bar&y=foo HTTP/1.1',
          headers: {
            "Content-Type" => "application/x-www-form-urlencoded",
            "Content-Length" => 5
          },
          body: "y=bar"
        )
      end
    end

    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: PREDICATE_VALUE=bar PREDICATE_VALUE_NAME=x/
    assert_log_match /CLIPP ANNOUNCE: PREDICATE_VALUE=foo PREDICATE_VALUE_NAME=y/
    assert_log_match /CLIPP ANNOUNCE: PREDICATE_VALUE=bar PREDICATE_VALUE_NAME=y/
  end

  def test_rx_capture
    clipp(
      predicate: true,
      modules: ['pcre', 'htp'],
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST "predicate:(p (operator 'rx' 'a' (var 'ARGS')))"
      EOS
    ) do
      transaction do |t|
        t.request(raw: 'GET /?x=aa&y=ab')
      end
    end

    assert_no_issues
    assert_log_match "[x:['0':'a'] y:['0':'a']]"
  end

  def test_set_predicate_vars_index_use_rns829
    clipp(
      predicate: 'true',
      modules: ['htp'],
      default_site_config: <<-EOS
        Action id:1 phase:REQUEST set_predicate_vars "predicate:(cat 1 2 3)" "clipp_announce:%{PREDICATE_VALUE_NAME} %{PREDICATE_VALUE}"
        Action id:2 phase:REQUEST set_predicate_vars "predicate:(cat 1 2 3)" "clipp_announce:%{PREDICATE_VALUE_NAME} %{PREDICATE_VALUE}"
      EOS
    ) do
      transaction {|t| t.request(raw:'GET /')}
    end
    assert_no_issues
  end

  def test_multiple_context
    clipp(
      predicate: 'true',
      modules: ['htp'],
      config: 'Action id:1 phase:REQUEST "predicate:(cat 1)" "clipp_announce:MAIN"',
      default_site_config: <<-EOS
        RuleEnable all
        Action id:2 phase:REQUEST "predicate:(cat 2)" "clipp_announce:SITE"
      EOS
    ) do
      transaction {|t| t.request(raw:'GET /')}
    end
    assert_no_issues
    assert_log_match "CLIPP ANNOUNCE: MAIN"
    assert_log_match "CLIPP ANNOUNCE: SITE"
  end
end
