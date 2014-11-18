require 'fileutils'

# Integration testing.
class TestRules < CLIPPTest::TestCase
  include CLIPPTest

  def test_rule_enable_all
    clipp(
      config: '''
        InitVar A1 1
        InitVar A2 2
        Rule A1 @clipp_print "A1" id:r1 rev:1 tag:rules/a/1 phase:REQUEST
        Rule A2 @clipp_print "A2" id:r2 rev:1 tag:rules/a/2 phase:REQUEST
      ''',
      default_site_config: <<-EOS
        RuleEnable "all"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A1]: 1'
    assert_log_match 'clipp_print [A2]: 2'
  end

  def test_rule_disable_all
    clipp(
      config: '''
        InitVar A1 1
        InitVar A2 2
        Rule A1 @clipp_print "A1" id:r1 rev:1 tag:rules/a/1 phase:REQUEST
        Rule A2 @clipp_print "A2" id:r2 rev:1 tag:rules/a/2 phase:REQUEST
      ''',
      default_site_config: <<-EOS
        RuleEnable "all"
        RuleDisable "all"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_no_match /clipp_print \[A1\]: 1/
    assert_log_no_match /clipp_print \[A2\]: 2/
  end

  def test_rule_enable_tag
    clipp(
      config: '''
        InitVar A1 1
        InitVar A2 2
        Rule A1 @clipp_print "A1" id:r1 rev:1 tag:rules/a/1 phase:REQUEST
        Rule A2 @clipp_print "A2" id:r2 rev:1 tag:rules/a/2 phase:REQUEST
      ''',
      default_site_config: <<-EOS
        RuleEnable "tag:rules/a/1"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A1]: 1'
    assert_log_no_match /clipp_print \[A2\]: 2/
  end

  def test_rule_disable_tag
    clipp(
      config: '''
        InitVar A1 1
        InitVar A2 2
        Rule A1 @clipp_print "A1" id:r1 rev:1 tag:rules/a/1 phase:REQUEST
        Rule A2 @clipp_print "A2" id:r2 rev:1 tag:rules/a/2 phase:REQUEST
      ''',
      default_site_config: <<-EOS
        RuleEnable "all"
        RuleDisable "tag:rules/a/2"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A1]: 1'
    assert_log_no_match /clipp_print \[A2\]: 2/
  end

  def test_rule_enable_id
    clipp(
      config: '''
        InitVar A1 1
        InitVar A2 2
        Rule A1 @clipp_print "A1" id:r1 rev:1 tag:rules/a/1 phase:REQUEST
        Rule A2 @clipp_print "A2" id:r2 rev:1 tag:rules/a/2 phase:REQUEST
      ''',
      default_site_config: <<-EOS
        RuleEnable "id:r1"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A1]: 1'
    assert_log_no_match /clipp_print \[A2\]: 2/
  end

  def test_rule_disable_id
    clipp(
      config: '''
        InitVar A1 1
        InitVar A2 2
        Rule A1 @clipp_print "A1" id:r1 rev:1 tag:rules/a/1 phase:REQUEST
        Rule A2 @clipp_print "A2" id:r2 rev:1 tag:rules/a/2 phase:REQUEST
      ''',
      default_site_config: <<-EOS
        RuleEnable all
        RuleDisable "id:r2"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A1]: 1'
    assert_log_no_match /clipp_print \[A2\]: 2/
  end

  def test_rule_enable_id_prefix
    clipp(
      config: '''
        InitVar A1 1
        InitVar A2 2
        Rule A1 @clipp_print "A1" id:r1 rev:1 tag:rules/a/1 phase:REQUEST
        Rule A2 @clipp_print "A2" id:r2 rev:1 tag:rules/a/2 phase:REQUEST
      ''',
      default_site_config: <<-EOS
        RuleEnable "id:r*"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A1]: 1'
    assert_log_match 'clipp_print [A2]: 2'
  end

  def test_rule_disable_id_prefix
    clipp(
      config: '''
        InitVar A1 1
        InitVar A2 2
        Rule A1 @clipp_print "A1" id:r1 rev:1 tag:rules/a/1 phase:REQUEST
        Rule A2 @clipp_print "A2" id:r2 rev:1 tag:rules/a/2 phase:REQUEST
      ''',
      default_site_config: <<-EOS
        RuleEnable all
        RuleDisable "id:r*"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_no_match /clipp_print \[A1\]: 1/
    assert_log_no_match /clipp_print \[A2\]: 2/
  end

  def test_rule_enable_tag_prefix
    clipp(
      config: '''
        InitVar A1 1
        InitVar A2 2
        Rule A1 @clipp_print "A1" id:r1 rev:1 tag:rules/a/1 phase:REQUEST
        Rule A2 @clipp_print "A2" id:r2 rev:1 tag:rules/a/2 phase:REQUEST
      ''',
      default_site_config: <<-EOS
        RuleEnable "tag:rules/a/*"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A1]: 1'
    assert_log_match 'clipp_print [A2]: 2'
  end

  def test_rule_disable_tag_prefix
    clipp(
      config: '''
        InitVar A1 1
        InitVar A2 2
        Rule A1 @clipp_print "A1" id:r1 rev:1 tag:rules/a/1 phase:REQUEST
        Rule A2 @clipp_print "A2" id:r2 rev:1 tag:rules/a/2 phase:REQUEST
      ''',
      default_site_config: <<-EOS
        RuleEnable all
        RuleDisable "tag:rules/a/*"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_no_match /clipp_print \[A1\]: 1/
    assert_log_no_match /clipp_print \[A2\]: 2/
  end
end