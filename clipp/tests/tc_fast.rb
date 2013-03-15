$:.unshift(File.dirname(File.dirname(File.expand_path(__FILE__))))
require 'clipp_test'

class TestFast < Test::Unit::TestCase
  include CLIPPTest

  CONFIG = [
    'LoadModule "ibmod_fast.so"',
    "FastAutomata \"#{TESTDIR}/fast_rules.txt.e\""
  ].join("\n")

  def make_request(s)
    simple_hash("GET /#{s}/a HTTP/1.1\nHost: foo.bar\n\n")
  end

  def test_without_fast
    clipp(
      :input_hashes => [make_request('foobar')],
      :default_site_config => <<-EOS
        Rule REQUEST_URI @rx foobar id:1 phase:REQUEST_HEADER clipp_announce:basic_rule
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic_rule/
  end

  def test_load
    clipp(
      :input_hashes => [make_request('foobar')],
      :config => 'LoadModule "ibmod_fast.so"',
      :default_site_config => <<-EOS
        Rule REQUEST_URI @rx foobar id:1 phase:REQUEST_HEADER clipp_announce:basic_rule
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic_rule/
  end

  def test_load_automata
    clipp(
      :input_hashes => [make_request('foobar')],
      :config => CONFIG,
      :default_site_config => "Include \"#{TESTDIR}/fast_rules.txt\""
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: foobar/
  end

  def test_multiple
    clipp(
      :input_hashes => [make_request('abcdef')],
      :config => CONFIG,
      :default_site_config => "Include \"#{TESTDIR}/fast_rules.txt\""
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: abc/
    assert_log_match /CLIPP ANNOUNCE: def/
  end

  def test_falseinject
    clipp(
      :input_hashes => [make_request('abcdef')],
      :config => CONFIG,
      :default_site_config => "Include \"#{TESTDIR}/fast_rules.txt\""
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: somethingelse/
  end

  def test_nonfast
    clipp(
      :input_hashes => [make_request('nonfast')],
      :config => CONFIG,
      :default_site_config => "Include \"#{TESTDIR}/fast_rules.txt\""
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: nonfast/
  end

  def test_contradiction
    clipp(
      :input_hashes => [make_request('contradiction')],
      :config => CONFIG,
      :default_site_config => "Include \"#{TESTDIR}/fast_rules.txt\""
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: contradiction/
  end

  def test_headers
    clipp(
      :input_hashes => [simple_hash("GET /a HTTP/1.1\nHost: headervalue\n\n")],
      :config => CONFIG,
      :default_site_config => "Include \"#{TESTDIR}/fast_rules.txt\""
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: headervalue/
  end

  def test_response
    clipp(
      :input_hashes => [simple_hash(
        "GET /a HTTP/1.1\nHost: headervalue\n\n",
        "HTTP/1.1 200 HelloWorld\nABC: DEF\n\n"
      )],
      :config => CONFIG,
      :default_site_config => "Include \"#{TESTDIR}/fast_rules.txt\""
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: rmessage/
    assert_log_match /CLIPP ANNOUNCE: rheader/
  end
end
