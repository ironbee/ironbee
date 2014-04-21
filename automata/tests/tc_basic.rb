$:.unshift(File.dirname(File.dirname(File.expand_path(__FILE__))))
$:.unshift(File.dirname(File.expand_path(__FILE__)))

require 'rubygems'
begin
gem "minitest", "~> 4.0"
rescue Gem::LoadError => e
    # nop
end
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

    ac_test(words, text, "traditional")
    ac_test(words, text, "traditional_fast", :fast)
  end

  def test_traditional2
    words = ["he", "she", "his", "hers"]
    text = "she saw his world as he saw heros..."

    ac_test(words, text, "traditional")
  end

  def test_large
    n = 1000

    words = Set.new
    while words.size < n
      words << random_word(10)
    end
    words = words.to_a

    text = words.join(" ")

    ac_test(words, text, "large")
  end

  def test_moderate
    n = 200

    words = Set.new
    while words.size < n
      words << random_word(10)
    end
    words = words.to_a

    text = words.join(" ")

    ac_test(words, text, "moderate")
    ac_test(words, text, "moderate_fast", :fast)
    ac_test(words, text, "moderate_space", :space)
  end

  def test_aaaa
    words = ["a", "aa", "aaa", "aaaa"]
    text = "aaaaaaaaaaaa"

    ac_test(words, text, "aaaa")
    ac_test(words, text, "aaaa_fast", :fast)
    ac_test(words, text, "aaaa_space", :space)
  end

  def test_wide
    words = []
    ('a'..'z').each do |x|
      words << "a#{x}#{x}"
      words << "a#{x.upcase}#{x}"
    end
    text = words.join(" ")

    ac_test(words, text, "wide")
    ac_test(words, text, "wide_fast", :fast)
    ac_test(words, text, "wide_space", :space)
  end

  def test_tails
    words = ["afoobar", "bfoobar", "cfoobar"]
    text = words.join(" ")

    ac_test(words, text, "tails")
    ac_test(words, text, "tails_fast", :fast)
    ac_test(words, text, "tails_space", :space)
  end

  def test_overlap
    words = ["aaabbb", "bbbccc", "cccddd", "dddaaa"]
    text = "aaabbbcccdddaaa"

    ac_test(words, text, "overlap")
    ac_test(words, text, "overlap_fast", :fast)
    ac_test(words, text, "overlap_space", :space)
  end

  def test_trie
    words = ["foo", "foobar", "foobaz", "world"]

    automata_test(words, TRIEGEN, "trie") do |dir, eudoxus_path|
      words.each do |word|
        output = ee(eudoxus_path, dir, word, "input_#{word}", "output_#{word}", "integer", ["-f"])
        assert(! output.empty?)
      end
      output = ee(eudoxus_path, dir, "goodbye", "input_goodbye", "output_goodbyte", "integer", ["-f"])
      assert(output.empty?)
    end
  end

  def test_foo
    words = ["forb"]
    text = "aaa ford bbb"
    ac_test(words, text, "foo")
  end

  def test_tail
    words = ["foobar"]
    text = "foobar"
    ac_test(words, text, "foobar")
  end

  def test_end_in_pc
    words = ["tesx"]
    text = "te"
    ac_test( words, text, "end_in_pc")
  end

  def test_pc_across_input
    words = ["foobar"]
    text = "foobar"
    # Note: output will be scrambled by -l 0
    automata_test(words, ACGEN, "foobar", false) do |dir, eudoxus_path|
      output = ee(eudoxus_path, dir, text, 'input', 'output', 'integer', ['-s', '1', '-l', '0'])
      assert(! output.empty?)
    end
  end
end
