$:.unshift(File.dirname(File.dirname(File.expand_path(__FILE__))))
$:.unshift(File.dirname(File.expand_path(__FILE__)))

require 'automata_test'
require 'test/unit'

if ! ENV['abs_builddir']
  raise "Need environmental variable abs_builddir properly set."
end

def random_word(max_length)
  length = rand(max_length) + 1
  (1..length).collect {"%c" % (97 + rand(26))}.join
end

class TestBasic < Test::Unit::TestCase
  include AutomataTest

  def test_traditional
    words = ["he", "she", "his", "hers"]
    text = "she saw his world as he saw hers..."

    ac_test(words, text)
  end

  def test_large
    n = 1000

    words = Set.new
    while words.size < n
      words << random_word(10)
    end
    words = words.to_a

    text = words.join(" ")

    ac_test(words, text)
  end
end
