$:.unshift(File.dirname(File.dirname(__FILE__)))
require 'clipp_test'

class TestRegression < CLIPPTestCase
  def test_trivial
    clipp(input: "echo:foo")
  end

  def test_body_before_header
    clipp(input: "htp:body_before_header.t")
  end

  def test_empty_header
    clipp(input: "raw:empty_header.req,empty_header.resp")
  end

  def test_http09
    clipp(input: "raw:http09.req,http09.resp")
  end

  def test_manyblank
    clipp(input: "raw:manyblank.req,manyblank.resp")
  end

  def test_basic_rule
    clipp(
      input: "echo:\"GET /foo\"",
      default_site_config: <<-EOS
        Rule REQUEST_METHOD "@rx GET" id:1 phase:REQUEST_HEADER block event
      EOS
    )
    assert_log_match /action "block" executed/
  end

  def test_negative_content_length
    request = <<-EOS
      GET / HTTP/1.1
      Content-Length: -1

    EOS
    request.gsub!(/^\s+/,"")
    clipp(
      input_hashes: [simple_hash(request)],
      input: "pb:- @parse @fillbody"
    )
  end
end
