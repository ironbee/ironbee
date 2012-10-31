$:.unshift(File.dirname(File.dirname(File.expand_path(__FILE__))))
$:.unshift(File.dirname(File.expand_path(__FILE__)))

require 'automata_test'
require 'test/unit'

if ! ENV['abs_builddir']
  raise "Need environmental variable abs_builddir properly set."
end

class TestPattern < Test::Unit::TestCase
  include AutomataTest

  def test_simple
    words = ['\u']
    text = "ABCdefGHI"
    automata_test(words, [ACGEN, '-p'], "simple") do |dir, eudoxus_path|
      output_substrings = ee(eudoxus_path, dir, text, "input", "output", "string")
      assert_equal(1, output_substrings.size)
      assert_equal([1 , 2, 3, 7, 8, 9].to_set, output_substrings['\u'])
    end
  end

  def test_foobar
    words = ['foo\lbar', 'foo\ubar', 'foo\abar']
    text = "foobar fooabar fooAbar fooAbaz hello world"

    automata_test(words, [ACGEN, '-p'], "foobar") do |dir, eudoxus_path|
      output_substrings = ee(eudoxus_path, dir, text, "input", "output", "string")
      assert_equal(3, output_substrings.size)
      assert_equal([22, 14].to_set, output_substrings['foo\abar'])
      assert_equal([14].to_set, output_substrings['foo\lbar'])
      assert_equal([22].to_set, output_substrings['foo\ubar'])
    end
  end

  def test_easy_dense
    words = ['\a\u\l', '\l\u\a']
    text = "Foo FOO foo fOo foO"
    automata_test(words, [ACGEN, '-p'], "easy_dense") do |dir, eudoxus_path|
      output_substrings = ee(eudoxus_path, dir, text, "input", "output", "string")
      assert_equal(2, output_substrings.size)
      assert_equal([15].to_set, output_substrings['\l\u\a'])
      assert_equal([15].to_set, output_substrings['\a\u\l'])
    end
  end

  def test_full_absorb
    words = ['\u\a', 'aa']
    text = "aa AA"
    automata_test(words, [ACGEN, '-p'], "easy_dense") do |dir, eudoxus_path|
      output_substrings = ee(eudoxus_path, dir, text, "input", "output", "string")
      assert_equal(2, output_substrings.size)
      assert_equal([5].to_set, output_substrings['\u\a'])
      assert_equal([2].to_set, output_substrings['aa'])
    end
  end

  def test_hard_dense
    words = ['\u\a\l', '\u\l\a', '\a\u\l', '\a\l\u', '\l\a\u', '\l\u\a', '\a\a\a']
    text = "Foo FOO foo fOo foO"
    # Foo = 3
    # FOO = 7
    # foo = 11
    # fOo = 15
    # foO = 19
    automata_test(words, [ACGEN, '-p'], "hard_dense") do |dir, eudoxus_path|
      output_substrings = ee(eudoxus_path, dir, text, "input", "output", "string")
      assert_equal(7, output_substrings.size)
      assert_equal([3].to_set, output_substrings['\u\a\l'])
      assert_equal([3].to_set, output_substrings['\u\l\a'])
      assert_equal([15].to_set, output_substrings['\a\u\l'])
      assert_equal([19].to_set, output_substrings['\a\l\u'])
      assert_equal([19].to_set, output_substrings['\l\a\u'])
      assert_equal([15].to_set, output_substrings['\l\u\a'])
      assert_equal([3, 7, 11, 15, 19].to_set, output_substrings['\a\a\a'])
    end
  end

  def test_singles
    words = ['\\\\', '\t', '\v', '\n', '\r', '\f', '\0', '\e']
    text = "\\\t\v\n\r\f\0\e"
    automata_test(words, [ACGEN, '-p'], "test_singles") do |dir, eudoxus_path|
      output_substrings = ee(eudoxus_path, dir, text, "input", "output", "string")
      assert_equal(words.size, output_substrings.size)
      words.each_with_index do |w, i|
        assert_equal([i+1].to_set, output_substrings[w])
      end
    end
  end

  def test_params
    words = ['\x46\x6f\x6f', '\^J']
    text = "Foo\n"
    automata_test(words, [ACGEN, '-p'], "test_params") do |dir, eudoxus_path|
      output_substrings = ee(eudoxus_path, dir, text, "input", "output", "string")
      assert_equal(words.size, output_substrings.size)
      assert_equal([3].to_set, output_substrings[words[0]])
      assert_equal([4].to_set, output_substrings[words[1]])
    end
  end

  def test_multiple
    words = ['\d', '\D', '\h', '\w', '\W', '\a', '\l', '\u', '\s', '\S', '\p', '\.']
    text = "1.FZ.ZzZ ..."
    #  1: 1
    #  2: .
    #  3: F
    #  4: Z
    #  5: .
    #  6: Z
    #  7: z
    #  8: Z
    #  9: space
    # 10: .
    # 11: .
    # 12: .
    automata_test(words, [ACGEN, '-p'], "test_multiple") do |dir, eudoxus_path|
      output_substrings = ee(eudoxus_path, dir, text, "input", "output", "string")
      assert_equal(words.size, output_substrings.size)
      assert_equal([1].to_set, output_substrings[words[0]])
      assert_equal([2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12].to_set, output_substrings[words[1]])
      assert_equal([1, 3].to_set, output_substrings[words[2]])
      assert_equal([1, 3, 4, 6, 7, 8].to_set, output_substrings[words[3]])
      assert_equal([2, 5, 9, 10, 11, 12].to_set, output_substrings[words[4]])
      assert_equal([3, 4, 6, 7, 8].to_set, output_substrings[words[5]])
      assert_equal([7].to_set, output_substrings[words[6]])
      assert_equal([3, 4, 6, 8].to_set, output_substrings[words[7]])
      assert_equal([9].to_set, output_substrings[words[8]])
      assert_equal([1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12].to_set, output_substrings[words[9]])
      assert_equal([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12].to_set, output_substrings[words[10]])
      assert_equal([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12].to_set, output_substrings[words[11]])
    end
  end

  def test_overlap
    words = ['\w\w\d', '\l\w', '\w\w']
    text = "AA9aa9"
    # 1: A
    # 2: A
    # 3: 9
    # 4: a
    # 5: a
    # 6: 9
    automata_test(words, [ACGEN, '-p'], "test_overlap") do |dir, eudoxus_path|
      output_substrings = ee(eudoxus_path, dir, text, "input", "output", "string")
      assert_equal(words.size, output_substrings.size)
      assert_equal([3, 6].to_set, output_substrings[words[0]])
      assert_equal([5, 6].to_set, output_substrings[words[1]])
      assert_equal([2, 3, 4, 5, 6].to_set, output_substrings[words[2]])
    end
  end
end
