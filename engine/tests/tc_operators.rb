class TestOperators < CLIPPTest::TestCase
  include CLIPPTest

  def test_match
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :default_site_config => <<-EOS
        Rule REQUEST_METHOD @match "GET HEAD" id:1 phase:REQUEST_HEADER capture clipp_announce:A=%{CAPTURE:0}
        Rule REQUEST_METHOD @match "POST DELETE" id:2 phase:REQUEST_HEADER clipp_announce:B
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: A=GET/
    assert_log_no_match /CLIPP ANNOUNCE: B/
  end

  def test_imatch
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :default_site_config => <<-EOS
        Rule REQUEST_METHOD @match "get head" id:1 phase:REQUEST_HEADER capture clipp_announce:A=%{CAPTURE:0}
        Rule REQUEST_METHOD @imatch "GET HEAD" id:2 phase:REQUEST_HEADER clipp_announce:B
      EOS
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: A=GET/
    assert_log_match /CLIPP ANNOUNCE: B/
  end

  def test_streq
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :default_site_config => <<-EOS
        Rule REQUEST_METHOD @streq get id:1 phase:REQUEST_HEADER clipp_announce:A
        Rule REQUEST_METHOD @streq GET id:2 phase:REQUEST_HEADER clipp_announce:B
      EOS
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: A/
    assert_log_match /CLIPP ANNOUNCE: B/
  end

  def test_istreq
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :default_site_config => <<-EOS
        Rule REQUEST_METHOD @istreq get id:1 phase:REQUEST_HEADER clipp_announce:A
        Rule REQUEST_METHOD @istreq GET id:2 phase:REQUEST_HEADER clipp_announce:B
        Rule REQUEST_METHOD @istreq HEAD id:3 phase:REQUEST_HEADER clipp_announce:C
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: A/
    assert_log_match /CLIPP ANNOUNCE: B/
    assert_log_no_match /CLIPP ANNOUNCE: C/
  end

  def test_ipmatch_09
    clipp(
      :input => "echo:\"GET /foo\" @parse @set_remote_ip:6.6.6.6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch "6.6.6.6" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch_09
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: ipmatch_09/
  end

  def test_ipmatch_11
    request = <<-EOS
GET / HTTP/1.1
Content-Length: 1234

    EOS
    input = [simple_hash(request)]
    clipp(
      :input_hashes => input,
      :input => "pb:INPUT_PATH @parse @set_remote_ip:6.6.6.6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch "10.11.12.13 6.6.6.6 1.2.3.4" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch_11a
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: ipmatch_11a/

    clipp(
      :input_hashes => input,
      :input => "pb:INPUT_PATH @parse @set_remote_ip:6.6.6.6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch "10.11.12.13 6.6.6.0/24 1.2.3.4" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch_11b
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: ipmatch_11b/

    clipp(
      :input_hashes => input,
      :input => "pb:INPUT_PATH @parse @set_remote_ip:6.6.6.6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch "10.11.12.13 6.6.5.0/24 1.2.3.4" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch_11c
      EOS
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: ipmatch_11c/
  end

  def test_ipmatch6_11
    request = <<-EOS
GET / HTTP/1.1
Content-Length: 1234

    EOS
    input = [simple_hash(request)]
    clipp(
      :input_hashes => input,
      :input => "pb:INPUT_PATH @parse @set_remote_ip:6::6:6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch6 "1::12:13 6::6:6 1::2:3" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch6_11a
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: ipmatch6_11a/

    clipp(
      :input_hashes => input,
      :input => "pb:INPUT_PATH @parse @set_remote_ip:6::6:6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch6 "1::12:13 6::6:0/112 1::2:3" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch6_11b
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: ipmatch6_11b/

    clipp(
      :input_hashes => input,
      :input => "pb:INPUT_PATH @parse @set_remote_ip:6::6:6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch6 "1::12:13 6::5:0/112 1::2:3" id:1 rev:1 phase:REQUEST_HEADER clipp_announce:ipmatch6_11c
      EOS
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: ipmatch6_11c/
  end
end
