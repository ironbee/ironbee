$:.unshift(File.dirname(File.dirname(File.expand_path(__FILE__))))
require 'set'

module AutomataTest
  BUILDDIR = ENV['abs_builddir']
  BINDIR = File.expand_path(File.join(BUILDDIR, "..", "bin"))
  EE = File.join(BINDIR, "ee")
  EC = File.join(BINDIR, "ec")
  ACGEN = File.join(BINDIR, "ac_generator")
  TRIEGEN = File.join(BINDIR, "trie_generator")
  OPTIMIZE = File.join(BINDIR, "optimize")
  OPTIMIZE_ARGS = {
    :fast => ["--fast"],
    :space => ["--space"]
  }

  # Returns map of word to ending position of word in input.
  def substrings(words, input)
    result = Hash.new {|h,k| h[k] = Set.new}
    l = input.length
    words.each do |word|
      i = 0
      while i + word.length <= l
        j = input.index(word, i)
        break if ! j
        result[word] << j + word.length
        i = j + 1
      end
    end
    result
  end

  def parse_ee_output(output)
    result = Hash.new {|h,k| h[k] = Set.new}
    output.split("\n").each do |line|
      line.chomp!
      if line =~ /^\s*(\d+): (.+)$/
        result[$2] << $1.to_i
      end
    end
    result
  end

  def assert_substrings_equal(a, b)
    a_keys = Set.new a.keys
    b_keys = Set.new b.keys
    a_minus_b = a_keys - b_keys
    b_minus_a = b_keys - a_keys
    assert(a_minus_b.empty?, "Missing keys: #{a_minus_b.to_a.join(', ')}")
    assert(b_minus_a.empty?, "Extra keys: #{b_minus_a.to_a.join(', ')}")

    a.each do |word, locations|
      assert_equal(locations, b[word], "Locations of #{word} mismatch.")
    end
  end

  def run_from_file(cmd, inpath, outpath)
    child_pid = nil
    File.open(inpath, "r") do |inf|
      File.open(outpath, "w") do |outf|
        child_pid = fork
        if child_pid.nil?
          STDIN.reopen(inf)
          STDOUT.reopen(outf)
          exec(*cmd)
        end
      end
    end
    Process.wait(child_pid)
    assert($?.success?)
  end

  def ee(eudoxus_path, dir, text, input_name = "input", output_name = "output", output_type = "auto", extra_args = [])
    input_path = File.join(dir, input_name)
    output_path = File.join(dir, output_name)

    File.open(input_path, "w") do |fp|
      fp.print text
    end

    system(EE, "-a", eudoxus_path, "-o", output_path, "-i", input_path, "-t", output_type, *extra_args)

    parse_ee_output(IO.read(output_path))
  end

  def ac_test(words, text, prefix = "ac_test", optimize = false)
    automata_test(words, ACGEN, prefix, optimize) do |dir, eudoxus_path|
      output_substrings = ee(eudoxus_path, dir, text)
      assert_substrings_equal(substrings(words, text), output_substrings)
    end
  end

  def automata_test(words, generator, prefix = "automata_test", optimize = false)
    dir = File.join(BUILDDIR, "automata_test_#{prefix}#{$$}.#{rand(100000)}")
    Dir.mkdir(dir)
    puts "Test files are in #{dir}"

    words_path = File.join(dir, "words")
    File.open(words_path, "w") do |fp|
      fp.puts words.join("\n")
    end

    initial_automata_path = File.join(dir, "initial_automata")
    if ! generator.is_a?(Array)
      generator = [generator]
    end
    run_from_file(generator, words_path, initial_automata_path)

    if optimize
      automata_path = File.join(dir, "optimized_automata_#{optimize}")
      run_from_file([OPTIMIZE, *OPTIMIZE_ARGS[optimize]], initial_automata_path, automata_path)
    else
      automata_path = initial_automata_path
    end

    eudoxus_path = File.join(dir, "eudoxus")
    result = system(EC, "-i", automata_path, "-o", eudoxus_path)
    assert(result, "EC failed.")

    if block_given?
      yield dir, eudoxus_path
    end
  end
end
