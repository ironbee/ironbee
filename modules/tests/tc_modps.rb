PSPARSE = File.join(ENV['abs_builddir'], 'psparse')

if ! File.exists?(PSPARSE) # May not be part of build.
  puts "No psparse; Skipping tests."
else

  class TestModPS < Test::Unit::TestCase
    include CLIPPTest

    def make_request(s)
      simple_hash("GET #{s} HTTP/1.1\n\n")
    end

    def test_load
      clipp(
        :input_hashes => [make_request('foobar')],
        :config => 'LoadModule "ibmod_ps.so"',
        :default_site_config => <<-EOS
          Rule REQUEST_URI_RAW @rx foobar id:1 phase:REQUEST_HEADER clipp_announce:basic_rule
        EOS
      )
      assert_no_issues
      assert_log_match /CLIPP ANNOUNCE: basic_rule/
    end

    def test_parse_uri
      clipp(
        :input_hashes => [make_request('http://a.b.c/foo/bar?query#fragment')],
        :config => 'LoadModule "ibmod_ps.so"',
        :default_site_config => <<-EOS
          Rule REQUEST_URI_RAW @parseURI "" id:1 phase:REQUEST_HEADER CAPTURE:parsed_uri
          Rule parsed_uri:scheme @streq "http" id:2 phase:REQUEST_HEADER clipp_announce:scheme
          Rule parsed_uri:authority @streq "a.b.c" id:4 phase:REQUEST_HEADER clipp_announce:authority
          Rule parsed_uri:path @streq "/foo/bar" id:3 phase:REQUEST_HEADER clipp_announce:path
          Rule parsed_uri:query @streq "query" id:5 phase:REQUEST_HEADER clipp_announce:query
          Rule parsed_uri:fragment @streq "fragment" id:6 phase:REQUEST_HEADER clipp_announce:fragment
        EOS
      )
      assert_no_issues
      assert_log_match /CLIPP ANNOUNCE: scheme/
      assert_log_match /CLIPP ANNOUNCE: authority/
      assert_log_match /CLIPP ANNOUNCE: path/
      assert_log_match /CLIPP ANNOUNCE: query/
      assert_log_match /CLIPP ANNOUNCE: fragment/
    end
  end
end
