class TestLibInjection < Test::Unit::TestCase
  include CLIPPTest

  CONFIG = [
    'LoadModule "ibmod_libinjection.so"',
    "SQLiPatternSet a #{Dir.pwd}/sqli_a.txt"
  ].join("\n")

  def make_request(s)
    simple_hash("GET /foobar HTTP/1.1\nHost: #{s}\n\n")
  end

  def test_load
    clipp(
      :input_hashes => [make_request('host')],
      :config => CONFIG,
      :modules => ['pcre'],
      :default_site_config => <<-EOS
        Rule REQUEST_URI_RAW @rx foobar id:1 phase:REQUEST_HEADER clipp_announce:YES
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: YES/
  end

  def test_positive
    clipp(
      :input_hashes => [make_request('-1 UNION ALL SELECT')],
      :config => CONFIG,
      :default_site_config => <<-EOS
        Rule REQUEST_HEADERS:Host @is_sqli 'default' id:1 phase:REQUEST_HEADER clipp_announce:YES
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: YES/
  end

  def test_negative
    clipp(
      :input_hashes => [make_request('Not.SQLi')],
      :config => CONFIG,
      :default_site_config => <<-EOS
        Rule REQUEST_HEADERS:Host @is_sqli 'default' id:1 phase:REQUEST_HEADER clipp_announce:YES
      EOS
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: YES/
  end

  def test_pattern_set_negative
    clipp(
      :input_hashes => [make_request('-1 UNION ALL SELECT')],
      :config => CONFIG,
      :default_site_config => <<-EOS
        Rule REQUEST_HEADERS:Host @is_sqli 'a' id:1 phase:REQUEST_HEADER clipp_announce:YES
      EOS
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: YES/
  end

  def test_pattern_set_positive
    clipp(
      :input_hashes => [make_request('IS IS IS IS IS')],
      :config => CONFIG,
      :default_site_config => <<-EOS
        Rule REQUEST_HEADERS:Host @is_sqli 'a' id:1 phase:REQUEST_HEADER clipp_announce:YES
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: YES/
  end

  def test_normalize
    clipp(
      :input_hashes => [make_request('IS IS IS IS IS')],
      :config => CONFIG,
      :default_site_config => <<-EOS
        Rule REQUEST_HEADERS:Host.normalizeSqli() @clipp_print result id:1 phase:REQUEST_HEADER clipp_announce:YES
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: YES/
    assert_log_match /clipp_print \[result\]: IS/
  end
end
