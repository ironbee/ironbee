require File.join(File.dirname(__FILE__), '..', '..', 'clipp', 'clipp_test')

class TestPredicate < Test::Unit::TestCase
  include CLIPPTest

  CONFIG = {
    :config => 'LoadModule "' + BUILDDIR + '/../.libs/ibmod_predicate.so"' + "\n" + 'LoadModule "ibmod_htp.so"'
  }

  def make_request(s)
    simple_hash("GET /#{s}/a HTTP/1.1\nHost: foo.bar\n\n")
  end

  def test_load
    clipp(CONFIG.merge(
      :input_hashes => [make_request('foobar')],
      :default_site_config => <<-EOS
        Rule REQUEST_URI_RAW @rx foobar id:1 phase:REQUEST_HEADER clipp_announce:basic_rule
      EOS
    ))
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic_rule/
  end

  def test_field_present
    clipp(CONFIG.merge(
      :input_hashes => [make_request('foobar')],
      :default_site_config => <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:field_present "predicate:(field 'REQUEST_URI')"
      EOS
    ))
    assert_no_issues
    assert_log_no_match /NOTICE/
    assert_log_match /CLIPP ANNOUNCE: field_present/
  end

  def test_two_rules
    clipp(CONFIG.merge(
      :input_hashes => [make_request('foobar')],
      :default_site_config => <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:field_present "predicate:(field 'REQUEST_URI')"
        Action id:2 phase:REQUEST_HEADER clipp_announce:field_not_present "predicate:(field 'FOOBAR')"
      EOS
    ))
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
          Action id:2 phase:REQUEST_HEADER clipp_announce:foo "predicate:(field 'REQUEST_URI')"
      </Site>
    EOS
    clipp(CONFIG.merge(
      :input_hashes => [make_request('foobar')],
      :config_trailer => foo_config,
      :default_site_config => <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:default "predicate:(field 'REQUEST_URI')"
      EOS
    ))
    assert_no_issues
    assert_log_no_match /NOTICE/
    assert_log_match /CLIPP ANNOUNCE: default/
    assert_log_no_match /CLIPP ANNOUNCE: foo/
  end

  def test_assert_valid
    clipp(CONFIG.merge(
      :input_hashes => [make_request('foobar')],
      :default_site_config => <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:field_present "predicate:(field 'REQUEST_URI')"
        PredicateAssertValid ""
      EOS
    ))
    assert_no_issues
    assert_log_no_match /NOTICE/
  end

  def test_debug_report
    clipp(CONFIG.merge(
      :input_hashes => [make_request('foobar')],
      :default_site_config => <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:field_present "predicate:(field 'REQUEST_URI')"
        PredicateDebugReport ""
      EOS
    ))
    assert_no_issues
    assert_log_no_match /NOTICE/
  end

  def test_define
    clipp(CONFIG.merge(
      :input_hashes => [make_request('foobar')],
      :default_site_config => <<-EOS
        PredicateDefine "foo" "name" "(field (ref 'name'))"
        Action id:1 phase:REQUEST_HEADER clipp_announce:field_present "predicate:(foo 'REQUEST_URI')"
      EOS
    ))
    assert_no_issues
    assert_log_no_match /NOTICE/
    assert_log_match /CLIPP ANNOUNCE: field_present/
  end

  def test_params
    clipp(CONFIG.merge(
      :input_hashes => [make_request('hello?a=foo&b=bar')],
      :default_site_config => <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:field_present "predicate:(operator 'rx' 'foo|bar' (p 'args=' (field 'request_uri_params')))"
      EOS
    ))
    assert_no_issues
    assert_log_no_match /NOTICE/
    assert_log_match /CLIPP ANNOUNCE: field_present/
  end

  def test_phaseless
    clipp(CONFIG.merge(
      :input_hashes => [make_request('foobar')],
      :default_site_config => <<-EOS
        Action id:1 clipp_announce:field_present "predicate:(var 'REQUEST_URI')"
      EOS
    ))
    assert_no_issues
    assert_log_no_match /NOTICE/
    assert_log_match /CLIPP ANNOUNCE: field_present/
  end

  def test_trace
    clipp(CONFIG.merge(
      default_site_config: <<-EOS
       Action id:1 phase:REQUEST_HEADER clipp_announce:a "predicate:(eq 'foo' (sub 'x' (var 'ARGS')))"
       Action id:2 phase:REQUEST_HEADER clipp_announce:b "predicate:(eq 'bar' (sub 'x' (var 'ARGS')))"
       Action id:3 clipp_announce:c "predicate:(or (eq 'bar' (sub 'x' (var 'ARGS'))) (eq 'bar' (sub 'y' (var 'ARGS'))))"
       PredicateTrace ""
      EOS
    )) do
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

  def test_context_phaseless
    foo_config = <<-EOS
      Action id:1 clipp_announce:outer "predicate:(true)"
      <Location /foo>
          Action id:2 phase:REQUEST_HEADER clipp_announce:inner "predicate:(true)"
      </Location>
    EOS
    clipp(CONFIG.merge(
      :default_site_config => foo_config
    )) do
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
    clipp(CONFIG.merge(
      :input_hashes => [make_request('foobar')],
      :default_site_config => <<-EOS
        Action id:1 phase:REQUEST_HEADER clipp_announce:yes "predicate:(field 'REQUEST_URI')"
        Action id:2 phase:REQUEST_HEADER clipp_announce:no "predicate:(field 'REQUEST_URI')"
        RuleDisable id:2
      EOS
    ))
    assert_no_issues
    assert_log_no_match /NOTICE/
    assert_log_match /CLIPP ANNOUNCE: yes/
    assert_log_no_match /CLIPP ANNOUNCE: no/
  end
end
