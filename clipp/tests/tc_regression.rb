$:.unshift(File.dirname(File.dirname(__FILE__)))
require 'clipp_test'

class TestBasic < CLIPPTestCase
  def test_trivial
    clipp(input: "echo:foo")
  end
end
