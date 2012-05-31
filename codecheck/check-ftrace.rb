#!/usr/bin/env ruby

MY_DIR = File.expand_path(File.dirname(__FILE__))
IRONBEE_DIR = File.dirname(MY_DIR)

Dir.chdir( IRONBEE_DIR )
Dir.glob( '**/*.[ch]' ).each do |path|
  prev_line = nil
  current_function = nil
  last_function_line = nil
  in_ftrace_function = false
  IO.foreach(path).each do |line|
    error = lambda do |msg|
      puts "#{path}:#{$.}: #{msg}"
    end
    
    line.chomp!
    next if line =~ %r{^\s*$} || line =~ %r{^\s*/?\*\s} || line =~ %r{^\s*//}
    line.gsub!(%r{/\* .+ \*/},'')
    
    if line =~ /^\w/
      last_function_line = line
    end
    
    if line[0..0] == '{'
      if prev_line =~ /\)/
        current_function = last_function_line
      else 
        current_function = nil
      end
      in_ftrace_function = false
    elsif prev_line && prev_line[0..0] == '{' && current_function
      if line =~ /FTRACE_INIT/
        in_ftrace_function = true
      end
    elsif line =~ /\breturn(\s|;)/ && in_ftrace_function
      if line !~ /"return/
          error["MISSING FTRACE_RET: #{current_function}"]
      end
    end
    
    prev_line = line
  end
end
