#!/usr/bin/env ruby

# Generate report on a Waggle Predicate file.
#
# Evaluates a waggle file and detects any usage of Predicate.  Produces an
# HTML file reporting on those uses, including validation.  See `pp.pdf`.
#
# Requires `gem install ruby-lua`.
#
# Author: Christopher Alfeld <calfeld@qualys.com>

require 'open3'
require 'cgi'
$:.unshift(File.dirname(File.expand_path(__FILE__)))
require 'fmt_sexpr'
require 'render'

BASEDIR  = File.dirname(File.expand_path(__FILE__))
EXTRACT  = File.join(BASEDIR, "extract_predicate_from_waggle.rb")
DOT      = 'dot'
RESOURCE = split_data()

# Try to find pp_dot.
path = File.join(BASEDIR, "pp_dot")
if File.exists?(path)
  PP_DOT = path
elsif ENV['PP_DOT'] && File.exists?(ENV['PP_DOT'])
  PP_DOT = ENV['PP_DOT']
else
  # It's probably in a peer object directory.
  Dir.glob("#{BASEDIR}/../../*").each do |peer|
    path = File.join(peer, "predicate", "pp_dot")
    if File.directory?(peer) && File.exists?(path)
      PP_DOT = path
      break
    end
  end
end
if ! defined?(PP_DOT)
  STDERR.puts "I could not find pp_dot.  You can tell me where it is by " +
    "setting the PP_DOT environmental variable."
  exit 1
end

if ARGV.length != 1
  puts "Usage: #{$0} <waggle file>"
  exit 1
end

# All PP_DOT runs consist of zero to two graphs and zero or more other lines.
# The first graph is pre-transform, the second is post-transform.
# Other lines are WARNINGs or ERRORs.
#
# Returns text and either :ok, :warn, or :error
def renderpp(text)
  # state is
  # :pre -- Before pre-transform graph.
  # :transform -- Post pre-transform graph, before post-transform graph.
  # :post -- Post post-transform graph.
  state = :pre
  result = ""
  status = :ok

  extract_dot(text) do |type, data|
    if type == :dot
      if data =~ /fillcolor=red/
        status = :error
      elsif data =~ /fillcolor=orange/
        staus = :warn if status == :ok
      end
      if state == :pre
        result += "<h2>Pre-Transform Graph</h2>\n"
        state = :transform
      elsif state == :transform
        result += "<h2>Post-Transform Graph</h2>\n"
        state = :post
      else
        throw "Insanity"
      end
      result += render_dot_as_svg(data) + "\n"
    elsif type == :line
      # Not a dot line.
      if data =~ /^ERROR: /
        result += "<span class=pperror>"
        status = :error
      elsif data =~ /^WARNING: /
        result += "<span class=ppwarn>"
        status = :warn if status == :ok
      else
        result += "<span class=ppother>"
      end
      result += data + "</span>\n"
    else
      throw "Insanity"
    end
  end

  [result, status]
end

# Render a list of defines for consumption by pp_dot.
def puts_defines(to, defines)
  defines.each do |d|
    to.puts "Define #{d.name} #{d.args.join(',')} #{d.sexpr}"
  end
end

# Render a single sexpr as a tree.
def render_tree(defines, sexpr, extra_args = [])
  Open3.popen2(PP_DOT, "--expr", *extra_args) do |input, output|
    puts_defines(input, defines)

    input.puts sexpr
    input.close

    renderpp(output.read)
  end
end

# Render an sexpr as an sexpr.
def render_sexpr(sexpr)
  CGI::escapeHTML(
    IronBee::Predicate::fmt_sexpr(sexpr)
  ).gsub(/(?<=\()\w+/) do |x|
    "<b>#{x}</b>"
  end.gsub(/'(?:\\?+.)*?'/) do |x|
    "<i>#{x}</i>"
  end
end

# Main

waggle_file = ARGV[0]

Predicate = Struct.new(:rule_id, :line_number, :sexpr)
Define = Struct.new(:name, :line_number, :args, :sexpr)

# Extract predicate rules from the waggle file.
expressions = []
predicates = []
defines = []
IO.popen([EXTRACT, waggle_file]) do |r|
  r.each_line do |line|
    if line =~ /^PREDICATE\s+(\w+)\s+(\d+)\s+(.+)$/
      p = Predicate.new($1, $2.to_i, $3)
      expressions << p
      predicates << p
    elsif line =~ /^Define (\d+) (\w+)\((.+?)\): (.+)$/
      d = Define.new($2, $1.to_i, $3.split(' '), $4)
      expressions << d
      defines << d
    end
  end
end

expressions.sort! {|a,b| a.line_number <=> b.line_number}

puts RESOURCE['Header']

puts "<table>"
line_number = 0
IO.foreach(waggle_file) do |line|
  line_number += 1
  line.chomp!

  post = ""
  print "<tr>"

  if expressions.first && line_number == expressions.first.line_number
    e = expressions.shift
    pp, ppstatus = render_tree(defines, e.sexpr, (e.is_a?(Define) ? ['--no-post-validate'] : []))
    print "<td class=predicate-#{ppstatus.to_s} onclick=\"expand_collapse('p-#{line_number}')\">Predicate</td>"
    post = "<div style=display:#{ppstatus == :ok ? 'none' : 'inline'} class=output id=p-#{line_number}>\n"
    post += "<pre>" + render_sexpr(e.sexpr) + "</pre>\n"
    post += "<pre>" + pp + "</pre>\n"
    post += "</div>\n"
  else
    print "<td class=predicate></td>"
  end
  puts "<td class=linenumber>#{line_number}</td>"
  puts "<td><span class=line>#{CGI::escapeHTML(line)}</span>#{post}</td>"

  puts "</tr>"
end

puts "</table>"
puts "<hr>"
puts "<pre>"

Open3.popen2(PP_DOT, "--graph") do |input, output|
  puts_defines(input, defines)
  predicates.each do |predicate|
    input.puts "#{predicate.rule_id} #{predicate.sexpr}"
  end
  input.close

  puts renderpp(output.read)[0]
end

puts "</pre>"

puts RESOURCE['Footer']

__END__
<<<Header>>>
<!DOCTYPE HTML>
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<title>ExtractPredicateFromWaggle Report</title>
<script type='application/javascript'>
function expand_collapse(id)
{
  var e = document.getElementById(id)
  if (e.style.display == 'none') {e.style.display='inline';}
  else {e.style.display='none';}
}
</script>
<style type='text/css'>
.predicate-ok {
  color:green;
}
.predicate-error {
  color:red;
}
.predicate-warn {
  color:orange;
}
td {
  vertical-align: top;
}
.line {
  white-space: pre;
  font-family:monospace;
}
.pperror {
  color:red;
}
.ppwarn {
  color:orange;
}
</style>
</head>
<body>
<<<Footer>>>
</body>
</html>
