$:.unshift(File.dirname(File.dirname(__FILE__)))
require 'clipp_test'

class TestRegression < Test::Unit::TestCase
  include CLIPPTest

  def test_trivial
    clipp(:input => "echo:foo")
    assert_no_issues
  end

  def test_body_before_header
    clipp(:input => "htp:body_before_header.t")
    assert_no_issues
  end

  def test_empty_header
    clipp(:input => "raw:empty_header.req,empty_header.resp")
    assert_no_issues
  end

  def test_http09
    clipp(:input => "raw:http09.req,http09.resp")
    assert_no_issues
  end

  def test_manyblank
    clipp(:input => "raw:manyblank.req,manyblank.resp")
    assert_no_issues
  end

  def test_basic_rule
    clipp(
      :input => "echo:\"GET /foo\"",
      :default_site_config => <<-EOS
        Rule REQUEST_METHOD @rx GET id:1 phase:REQUEST_HEADER block event
      EOS
    )
    assert_no_issues
    assert_log_match /ACTION block/
  end

  def test_negative_content_length
    request = <<-EOS
      GET / HTTP/1.1
      Content-Length: -1

    EOS
    request.gsub!(/^\s+/, "")
    clipp(
      :input_hashes => [simple_hash(request)],
      :input        => "pb:INPUT_PATH @parse @fillbody"
    )
    assert_no_issues
  end

  def test_parse_http09
    request = <<-EOS
      POST /
      This is the body.
    EOS
    request.gsub!(/^\s+/, "")
    response = <<-EOS
      This is the body.
    EOS
    response.gsub!(/^\s+/, "")
    clipp(
      :input_hashes => [simple_hash(request, response)],
      :input        => "pb:INPUT_PATH @parse",
      :consumer     => "view"
    )
    assert_no_issues
    assert_log_no_match /_HEADER/
  end

  def test_ipmatch_09
    clipp(
      :input => "echo:\"GET /foo\" @set_remote_ip:6.6.6.6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch "6.6.6.6" id:1 rev:1 phase:REQUEST_HEADER event block
      EOS
    )
    assert_no_issues
    assert_log_match /ACTION block/
  end

  def test_ipmatch_11
    request = <<-EOS
GET / HTTP/1.1
Content-Length: 1234

    EOS
    input = [simple_hash(request)]
    clipp(
      :input_hashes => input,
      :input => "pb:INPUT_PATH @set_remote_ip:6.6.6.6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch "10.11.12.13 6.6.6.6 1.2.3.4" id:1 rev:1 phase:REQUEST_HEADER event block
      EOS
    )
    assert_no_issues
    assert_log_match /ACTION block/

    clipp(
      :input_hashes => input,
      :input => "pb:INPUT_PATH @set_remote_ip:6.6.6.6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch "10.11.12.13 6.6.6.0/24 1.2.3.4" id:1 rev:1 phase:REQUEST_HEADER event block
      EOS
    )
    assert_no_issues
    assert_log_match /ACTION block/

    clipp(
      :input_hashes => input,
      :input => "pb:INPUT_PATH @set_remote_ip:6.6.6.6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch "10.11.12.13 6.6.5.0/24 1.2.3.4" id:1 rev:1 phase:REQUEST_HEADER event block
      EOS
    )
    assert_no_issues
    assert_log_no_match /ACTION block/
  end

  def test_ipmatch6_11
    request = <<-EOS
GET / HTTP/1.1
Content-Length: 1234

    EOS
    input = [simple_hash(request)]
    clipp(
      :input_hashes => input,
      :input => "pb:INPUT_PATH @set_remote_ip:6::6:6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch6 "1::12:13 6::6:6 1::2:3" id:1 rev:1 phase:REQUEST_HEADER event block
      EOS
    )
    assert_no_issues
    assert_log_match /ACTION block/

    clipp(
      :input_hashes => input,
      :input => "pb:INPUT_PATH @set_remote_ip:6::6:6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch6 "1::12:13 6::6:0/112 1::2:3" id:1 rev:1 phase:REQUEST_HEADER event block
      EOS
    )
    assert_no_issues
    assert_log_match /ACTION block/

    clipp(
      :input_hashes => input,
      :input => "pb:INPUT_PATH @set_remote_ip:6::6:6",
      :default_site_config => <<-EOS
        Rule REMOTE_ADDR @ipmatch6 "1::12:13 6::5:0/112 1::2:3" id:1 rev:1 phase:REQUEST_HEADER event block
      EOS
    )
    assert_no_issues
    assert_log_no_match /ACTION block/
  end
end
