$:.unshift(File.dirname(File.dirname(__FILE__)))
require 'clipp_test'

class TestBasic < CLIPPTestCase
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
end
