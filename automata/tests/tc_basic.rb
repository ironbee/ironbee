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

  def test_large
    words_input = "/usr/share/dict/words"
    n = 291

    words = []
    File.open(words_input, "r") do |fp|
      while n > 0
        words << fp.gets.chomp
        n -= 1
      end
    end

    text = words.join(" ")

    ac_test(words, text)
  end
end
