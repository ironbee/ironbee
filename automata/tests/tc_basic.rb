$:.unshift(File.dirname(File.dirname(File.expand_path(__FILE__))))
$:.unshift(File.dirname(File.expand_path(__FILE__)))
require 'ac_generator'
require 'automata_test'
require 'test/unit'

if ! ENV['abs_builddir']
  raise "Need environmental variable abs_builddir properly set."
end

class TestBasic < Test::Unit::TestCase
  include AutomataTest

  BENCH = File.join(ENV['abs_builddir'], "..", "bench")
  EC = File.join(ENV['abs_builddir'], "..", "ec")

  def test_traditional
    words = ["he", "she", "his", "hers"]
    text = "she saw his world as he saw hers..."

    dir = "/tmp/tc_basic#{$$}"
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
