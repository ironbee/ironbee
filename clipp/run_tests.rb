#!/usr/bin/env ruby

require 'erb'
require 'tempfile'
require 'tmpdir'
require 'open3'

TESTDIR = File.join(File.dirname(__FILE__), 'tests')
FAIL_MESSAGES = [
    / (CONFIG_)?(ERROR|CRITICAL|ALERT|EMERGENCY)\s+-/
]
WHITELIST_MESSAGES = [
    /Failed to create LuaJIT state./
]

if ARGV.size != 2
    puts "Usage: #{$0} <path to clipp> <path to config>"
    exit 1
end

$clipp  = ARGV[0]
config = ARGV[1]

def run_clipp(config_path)
    result = nil
    Open3.popen3($clipp, '-c', config_path) do |cin, cout, cerr, wt|
        cin.close
        
        while true
            reads = IO.select([cout, cerr])[0]
            break if reads.find {|i| i.eof?}
            
            reads.each do |i|
                line = i.gets
                print line
                if result.nil?
                    is_fail  = ! FAIL_MESSAGES.find      {|x| x=~ line}.nil?
                    is_white = ! WHITELIST_MESSAGES.find {|x| x=~ line}.nil?
                    if is_fail && ! is_white
                        result = "Failure message: #{line}"
                    end 
                end
            end
        end
        
        cout.close
        cerr.close
    end
    exit_status = $?
    if exit_status != 0 && result.nil?
        result = "Non-zero exit status"
    end
    result
end

failure = false

Dir.chdir(TESTDIR)
Dir.glob('*.erb').each do |test|
    base = File.basename(test,'.erb')
    puts "Running #{base}"
    
    erb = ERB.new(IO.read(test))
    tmppath = File.join(Dir::tmpdir, 'clipp_tests.conf')
    File.open(tmppath, 'w') do |clipp_config|
        clipp_config.write(erb.result(binding))
    end
        
    result = run_clipp(tmppath)
    if result
        puts "#{base} FAIL #{result}"
        failure = true
    else 
        puts "#{base} PASS"
    end
    File.unlink(tmppath) if ! failure
end

exit 1 if failure
