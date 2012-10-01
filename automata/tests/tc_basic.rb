$:.unshift(File.dirname(File.dirname(File.expand_path(__FILE__))))
$:.unshift(File.dirname(File.expand_path(__FILE__)))

require 'automata_test'
require 'test/unit'

if ! ENV['abs_builddir']
  raise "Need environmental variable abs_builddir properly set."
end

class TestBasic < Test::Unit::TestCase
  include AutomataTest

  def test_traditional
    words = ["he", "she", "his", "hers"]
    text = "she saw his world as he saw hers..."

    ac_test(words, text)
  end
end
