
require 'set'

module AutomataTest

  # Returns map of word to ending position of word in input.
  def substrings(words, input)
    result = Hash.new {|h,k| h[k] = Set.new}
    words.each do |word|
      input.scan(word) do
        result[word] << $`.length + word.length
      end
    end
    result
  end

  def parse_bench_output(output)
    result = Hash.new {|h,k| h[k] = Set.new}
    output.split("\n").each do |line|
      line.chomp!
      if line =~ /^\s*(\d+): (\w+)$/
        result[$2] << $1.to_i
      end
    end
    result
  end

  def assert_substrings_equal(a, b)
    a_keys = Set.new a.keys
    b_keys = Set.new b.keys
    assert_equal(a_keys, b_keys, "Keys mismatch.")

    a.each do |word, locations|
      assert_equal(locations, b[word], "Locations of #{word} mismatch.")
    end
  end

end
