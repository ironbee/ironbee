require File.join(File.dirname(__FILE__), '..', 'clipp_test')

class TestPredicate < Test::Unit::TestCase
  include CLIPPTest

  CONFIG = {
    :config => 'LoadModule "' + BUILDDIR + '/../.libs/ibmod_predicate.so"'
  }

  def make_request(s)
    simple_hash("GET /#{s}/a HTTP/1.1\nHost: foo.bar\n\n")
  end

  def test_load
    clipp(CONFIG.merge(
      :input_hashes => [make_request('foobar')],
      :default_site_config => <<-EOS
        Rule REQUEST_URI @rx foobar id:1 phase:REQUEST_HEADER clipp_announce:basic_rule
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
end
