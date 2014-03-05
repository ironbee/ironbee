#!/usr/bin/env ruby

# Render PredicateTrace output.
#
# Author: Christopher Alfeld <calfeld@qualys.com>

require 'open3'
require 'cgi'
$:.unshift(File.dirname(File.expand_path(__FILE__)))
require 'render'

DOT = 'dot'
RESOURCES = split_data()

if ARGV.length > 1
  puts "Usage: #{$0} [<trace>]"
  exit 1
end

input = STDIN
if ! ARGV.empty?
  input = File.open(ARGV[0])
end

# Write header.
puts RESOURCES['Header']

id = 0
extract_dot(input.read) do |type, data|
  if type == :line
    if data =~ /^PredicateTrace (\w+) context=([^ ]+) consider=(\d+) inject=(\d+)/
      phase, context, consider, inject = $1, $2, $3, $4
      id += 1
      puts "<div class=ptrace_header onclick=\"expand_collapse('ptrace_body-#{id}')\">#{data}</div>"
      puts "<div id=ptrace_body-#{id} style='display:inline'>"
    elsif data =~ /^End PredicateTrace/
      puts "</div>"
    else
      puts data
    end
  else
    puts render_dot_as_svg(data)
  end
end

puts RESOURCES['Footer']

__END__
<<<Header>>>
<!DOCTYPE HTML>
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<title>PredicateTrace</title>
<style type='text/css'>
.ptrace_header {
  color:green;
}

</style>
<script type='application/javascript'>
function expand_collapse(id)
{
  var e = document.getElementById(id)
  if (e.style.display == 'none') {e.style.display='inline';}
  else {e.style.display='none';}
}
</script>
</head>
<body>
<pre>
<<<Footer>>>
</pre></body>
