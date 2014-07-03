require '../../clipp/clipp_test'

class TestLibInjection < Test::Unit::TestCase
  include CLIPPTest

  CONFIG = [
    'LoadModule "ibmod_libinjection.so"',
    "LibInjectionFingerprintSet a #{Dir.pwd}/sqli_a.txt",
    "LibInjectionFingerprintSet b #{Dir.pwd}/sqli_b.txt"
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

  def test_relative
    cwd = Dir.pwd
    Dir.chdir('/')
    clipp(
      input_hashes: [make_request('IS IS IS IS IS')],
      modules: ['libinjection'],
      config: "LibInjectionFingerprintSet a ../tests/sqli_a.txt",
      default_site_config: <<-EOS
        Rule REQUEST_HEADERS:Host @is_sqli 'a' id:1 phase:REQUEST_HEADER clipp_announce:YES
      EOS
    )
    Dir.chdir(cwd)
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: YES/
  end

  def test_no_set
    clipp(
      input_hashes: [make_request('-1 UNION ALL SELECT')],
      modules: ['libinjection'],
      default_site_config: <<-EOS
        Rule REQUEST_HEADERS:Host @is_sqli 'a' id:1 phase:REQUEST_HEADER clipp_announce:YES
      EOS
    )
    assert_log_no_match /Assertion failed/
    assert_log_match /No such fingerprint set: 'a'/
  end

  def test_capture
    clipp(
      :input_hashes => [make_request('-1 UNION ALL SELECT')],
      :config => CONFIG,
      :default_site_config => <<-EOS
        Rule REQUEST_HEADERS:Host @is_sqli 'default' capture id:1 phase:REQUEST_HEADER clipp_announce:%{CAPTURE:fingerprint},%{CAPTURE:confidence}
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: 1UE,0/
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

  def test_pattern_set_confidence1
    clipp(
      :input_hashes => [make_request('IS IS IS IS IS')],
      :config => CONFIG,
      :default_site_config => <<-EOS
        Rule REQUEST_HEADERS:Host @is_sqli 'a' capture id:1 phase:REQUEST_HEADER clipp_announce:%{CAPTURE:fingerprint},%{CAPTURE:confidence}
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: o,0/
  end

  def test_pattern_set_confidence2
    clipp(
      :input_hashes => [make_request('-1 UNION ALL SELECT')],
      :config => CONFIG,
      :default_site_config => <<-EOS
        Rule REQUEST_HEADERS:Host @is_sqli 'b' capture id:1 phase:REQUEST_HEADER clipp_announce:%{CAPTURE:fingerprint},%{CAPTURE:confidence}
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: 1UE,14/
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

  def test_xss_positive
    clipp(
      :input_hashes => [make_request('<script>alert("XSS")</script>')],
      :config => CONFIG,
      :default_site_config => <<-EOS
        Rule REQUEST_HEADERS:Host @is_xss 'default' id:1 phase:REQUEST_HEADER clipp_announce:YES
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: YES/
  end

  def test_negative2
    clipp(
      :input_hashes => [make_request('Not.XSS')],
      :config => CONFIG,
      :default_site_config => <<-EOS
        Rule REQUEST_HEADERS:Host @is_xss 'default' id:1 phase:REQUEST_HEADER clipp_announce:YES
      EOS
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: YES/
  end

  def test_libinjection_zero_length_input
    clipp(
      modules: ['sqltfn'],
      default_site_config: <<-EOS
        InitVar A ""
        Action id:1 rev:1 phase:REQUEST "setvar:B=%{A}.normalizeSqlPg()"
        Rule A @clipp_print "A" id:2 rev:1 phase:REQUEST
        Rule B @clipp_print "B" id:3 rev:1 phase:REQUEST
      EOS
    ) do
      transaction do |t|
        t.request(
          method: "GET",
          uri: "/",
          protocol: "HTTP/1.1",
          headers: {"Host" => "foo.bar"}
        )
      end
    end

    assert_no_issues
    assert_log_match /clipp_print \[A\]:.*$/
    assert_log_match /clipp_print \[B\]:.*$/
  end
end
