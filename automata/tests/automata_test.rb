$:.unshift(File.dirname(File.dirname(File.expand_path(__FILE__))))
require 'ac_generator'
require 'set'

module AutomataTest
  BENCH = File.join(ENV['abs_builddir'], "..", "bench")
  EC = File.join(ENV['abs_builddir'], "..", "ec")

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

  def ac_test(words, text, prefix = "full_test")
    dir = "/tmp/automata_test_#{prefix}#{$$}"
    Dir.mkdir(dir)
    puts "Test files are in #{dir}"
    automata_path = File.join(dir, "automata")
    File.open(automata_path, "w") do |fp|
      fp.print IronAutomata::aho_corasick(words)
    end

    input_path = File.join(dir, "input")
    File.open(input_path, "w") do |fp|
      fp.print text
    end

    eudoxus_path = File.join(dir, "eudoxus")
    system(EC, "-i", automata_path, "-o", eudoxus_path)

    output_path = File.join(dir, "output")
    system(BENCH, "-a", eudoxus_path, "-o", output_path, "-i", input_path, "-t", "length")

    output_substrings = parse_bench_output(IO.read(output_path))
    assert_substrings_equal(substrings(words, text), output_substrings)
  end
end
