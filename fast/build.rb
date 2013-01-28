#!/usr/bin/env ruby

HOME = File.expand_path(File.dirname(__FILE__))

if ! File.exists?('./generate')
  STDERR.puts "Must run this in the same directory as 'generate'."
  exit 1
end

# Run cmd with cin and cout as input and output and return status.
# cerr is left alone.
def run(cmd, cin, cout)
  pid = fork do
    STDIN.reopen(cin)
    STDOUT.reopen(cout)

    exec(*cmd)
  end

  cin.close
  cout.close

  Process::wait2(pid)[1]
end

def v(msg)
  STDERR.puts(msg)
end

def step(what, cmd, infile, outfile)
  v what
  v "  #{cmd.join(' ')}"
  input = File.open(infile, 'r')
  output = File.open(outfile, 'w')
  status = run(cmd, input, output)
  if ! status.success?
    v "Failed: #{status}"
    exit 1
  end
end

if ARGV.length != 1
  STDERR.puts "Usage: #{$0} <rules>"
  exit 1
end

rules = ARGV[0]
manifest = rules + '.manifest'
step(
  "Extracting rules from #{rules} to #{manifest}",
  [File.join(HOME, 'extract.rb')],
  rules,
  manifest
)

automata = rules + '.automata'
step(
  "Generating AC automata from #{manifest} to #{automata}",
  ['./generate'],
  manifest,
  automata
)

optimized = rules + '.optimized'
step(
  "Optimizing automata from #{automata} to #{optimized}",
  ['../automata/bin/optimize', '--translate-nonadvancing-structural'],
  automata,
  optimized
)

eudoxus = rules + '.e'
compile_cmd = [
  '../automata/bin/ec',
  '-i',
  automata,
  '-o',
  eudoxus,
  '-h',
  '0.5'
]

v "Compiling optimized from #{optimized} to #{eudoxus}"
v "  #{compile_cmd.join(' ')}"
fork {exec(*compile_cmd)}
status = Process::wait2[1]
if ! status.success?
  v "Failed: #{status}"
  exit 1
end


