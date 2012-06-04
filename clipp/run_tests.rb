#!/usr/bin/env ruby

require 'erb'
require 'tempfile'
require 'tmpdir'

def color_puts(code, s)
    if STDOUT.isatty
        puts "\033[#{code}m#{s}\033[0m"
    else
        puts s
    end
end
   
def green(s)
    color_puts(32, s)
end

def yellow(s)
    color_puts(33, s)
end

def red(s)
    color_puts(31, s)
end
     
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
    r, w = IO.pipe
    
    Process.fork do
        r.close
        STDOUT.reopen(w)
        STDERR.reopen(w)
        exec($clipp, '-c', config_path)
    end
    
    w.close
    
    result = nil
    r.each do |line|
        print line
        if result.nil?
            is_fail  = ! FAIL_MESSAGES.find      {|x| x=~ line}.nil?
            is_white = ! WHITELIST_MESSAGES.find {|x| x=~ line}.nil?
            if is_fail && ! is_white
                result = "Failure message: #{line}"
            end 
        end
    end
    Process.wait
    exitstatus = $?.exitstatus
    if exitstatus != 0 && result.nil?
        "Non-zero exit status: #{$?.to_s}"
    else
        result
    end
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
        
    failure = false
    result = run_clipp(tmppath)
    if result
        red "#{base} FAIL #{result}"
        failure = true
    else 
        green "#{base} PASS"
    end
    if failure
        yellow "Configuration at #{tmppath}"
    else
        File.unlink(tmppath) if ! failure
    end
end

exit 1 if failure
