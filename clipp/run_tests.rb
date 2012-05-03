#!/usr/bin/env ruby

TESTDIR = File.join(File.dirname(__FILE__), 'tests')

if ARGV.size != 2
    puts "Usage: #{$0} <path to clipp> <path to config>"
    exit 1
end

clipp  = ARGV[0]
config = ARGV[1]

failure = false

Dir.chdir(TESTDIR)
Dir.glob('*.req').each do |test|
    base = File.basename(test,'.req')
    print base
    STDOUT.flush
    if ! File.exists?("#{base}.resp")
        puts "FAIL -- Missing #{base}.resp"
        failure = true
        continue
    end    
    if ! system(clipp,"raw:#{base}.req,#{base}.resp","ironbee:#{config}")
        puts "FAIL -- clipp existed non-zero"
        failure = true
    else
        puts "PASS"
    end
end

exit 1 if failure
