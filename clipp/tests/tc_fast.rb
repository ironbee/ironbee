$:.unshift(File.dirname(File.dirname(File.expand_path(__FILE__))))
require 'clipp_test'

class TestFast < Test::Unit::TestCase
  include CLIPPTest
  
  CONFIG = [
    'LoadModule "ibmod_fast.so"',
    "FastAutomata \"#{TESTDIR}/fast_rules.txt.e\""
  ].join("\n")

  REQUEST = "GET /foobar/a HTTP/1.1\nHost: foo.bar\n\n"

  def test_without_fast
    clipp(
      :input_hashes => [simple_hash(REQUEST)],
      :default_site_config => <<-EOS
        Rule REQUEST_URI_RAW @rx foobar id:1 phase:REQUEST_HEADER clipp_announce:basic_rule
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic_rule/
  end

  def test_load
    clipp(
      :input_hashes => [simple_hash(REQUEST)],
      :config => 'LoadModule "ibmod_fast.so"',
      :default_site_config => <<-EOS
        Rule REQUEST_URI_RAW @rx foobar id:1 phase:REQUEST_HEADER clipp_announce:basic_rule
      EOS
    )
    puts clipp_log
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic_rule/
  end

  def test_load_automata
    clipp(
      :input_hashes => [simple_hash(REQUEST)],
      :config => CONFIG,
      :default_site_config => "Include \"#{TESTDIR}/fast_rules.txt\""
    )
    puts clipp_log
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: foobar/
  end

end
