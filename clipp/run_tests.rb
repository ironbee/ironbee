#!/usr/bin/env ruby

require 'erb'
require 'tempfile'

TESTDIR = File.join(File.dirname(__FILE__), 'tests')

if ARGV.size != 2
    puts "Usage: #{$0} <path to clipp> <path to config>"
    exit 1
end

clipp  = ARGV[0]
config = ARGV[1]

failure = false

Dir.chdir(TESTDIR)
Dir.glob('*.erb').each do |test|
    base = File.basename(test,'.erb')
    print base
    STDOUT.flush
    
    erb = ERB.new(IO.read(test))
    Tempfile.open('clipp_tests') do |clipp_config|
        clipp_config.write(erb.result(binding))
        clipp_config.close
        
        test_cmd = "#{clipp} -c #{clipp_config.path}"
        if ! system(test_cmd)
            puts "FAIL -- clipp existed non-zero"
            puts "Command: #{test_cmd}"
            failure = true
        else
            puts "PASS"
        end
        clipp_config.unlink
    end
end

exit 1 if failure
