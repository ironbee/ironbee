Gem::Specification.new do |s|
  s.name = "htp"
  s.version = "0.1"
  
  s.authors = ["Chrustopher Alfeld"]
  s.description = "Ruby Bindings for libHTP."
  s.email = "calfeld@qualys.com"
  s.files = ["htp_ruby.rb", "HTP.c", "extconf.rb", "example.rb"]
  s.extensions = ["extconf.rb"]
  s.summary = "libHTP Ruby bindings."
  s.require_path = '.'
end
