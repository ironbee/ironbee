psparse = File.join(ENV['abs_builddir'], 'psparse')

if ! File.exists?(psparse) # May not be part of build.
  puts "No psparse; Skipping tests."
else

  class TestModPS < CLIPPTest::TestCase

    parallelize_me!

    include CLIPPTest

    def make_request(s)
      simple_hash("GET #{s} HTTP/1.1\n\n")
    end

    def test_load
      clipp(
        :input_hashes => [make_request('foobar')],
        :modules => ['ps', 'pcre'],
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
        :modules => ['ps'],
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

    def test_parse_request_line
      clipp(
        :input_hashes => [make_request('uri')],
        :modules => ['ps'],
        :default_site_config => <<-EOS
          Rule REQUEST_LINE @parseRequestLine "" id:1 phase:REQUEST_HEADER CAPTURE:parsed_request_line
          Rule parsed_request_line:method @streq "GET" id:2 phase:REQUEST_HEADER clipp_announce:method
          Rule parsed_request_line:uri @streq "uri" id:4 phase:REQUEST_HEADER clipp_announce:uri
          Rule parsed_request_line:version @streq "HTTP/1.1" id:3 phase:REQUEST_HEADER clipp_announce:version
        EOS
      )
      assert_no_issues
      assert_log_match /CLIPP ANNOUNCE: method/
      assert_log_match /CLIPP ANNOUNCE: uri/
      assert_log_match /CLIPP ANNOUNCE: version/
    end

    def test_parse_response_line
      clipp(
        :input_hashes => [simple_hash("HTTP/1.1 404 FooBar")],
        :modules => ['ps'],
        :default_site_config => <<-EOS
          Rule REQUEST_LINE @parseResponseLine "" id:1 phase:REQUEST_HEADER CAPTURE:parsed_response_line
          Rule parsed_response_line:version @streq "HTTP/1.1" id:3 phase:REQUEST_HEADER clipp_announce:version
          Rule parsed_response_line:status @streq "404" id:2 phase:REQUEST_HEADER clipp_announce:status
          Rule parsed_response_line:message @streq "FooBar" id:4 phase:REQUEST_HEADER clipp_announce:message
        EOS
      )
      assert_no_issues
      assert_log_match /CLIPP ANNOUNCE: version/
      assert_log_match /CLIPP ANNOUNCE: status/
      assert_log_match /CLIPP ANNOUNCE: message/
    end

    def test_parse_authority
      clipp(
        :input_hashes => [make_request('user:password@host:port')],
        :modules => ['ps'],
        :default_site_config => <<-EOS
          Rule REQUEST_URI_RAW @parseAuthority "" id:1 phase:REQUEST_HEADER CAPTURE:parsed_authority
          Rule parsed_authority:username @streq "user" id:2 phase:REQUEST_HEADER clipp_announce:username
          Rule parsed_authority:password @streq "password" id:3 phase:REQUEST_HEADER clipp_announce:password
          Rule parsed_authority:host @streq "host" id:4 phase:REQUEST_HEADER clipp_announce:host
          Rule parsed_authority:port @streq "port" id:5 phase:REQUEST_HEADER clipp_announce:port
        EOS
      )
      assert_no_issues
      assert_log_match /CLIPP ANNOUNCE: user/
      assert_log_match /CLIPP ANNOUNCE: password/
      assert_log_match /CLIPP ANNOUNCE: host/
      assert_log_match /CLIPP ANNOUNCE: port/
    end

    def test_parse_path
      clipp(
        :input_hashes => [make_request('/a/b/c/d.txt')],
        :modules => ['ps'],
        :default_site_config => <<-EOS
          Rule REQUEST_URI_RAW @parsePath "" id:1 phase:REQUEST_HEADER CAPTURE:parsed_path
          Rule parsed_path:directory @streq "/a/b/c" id:2 phase:REQUEST_HEADER clipp_announce:directory
          Rule parsed_path:file @streq "d.txt" id:3 phase:REQUEST_HEADER clipp_announce:file
          Rule parsed_path:base @streq "d" id:4 phase:REQUEST_HEADER clipp_announce:base
          Rule parsed_path:extension @streq "txt" id:5 phase:REQUEST_HEADER clipp_announce:extension
        EOS
      )
      assert_no_issues
      assert_log_match /CLIPP ANNOUNCE: directory/
      assert_log_match /CLIPP ANNOUNCE: file/
      assert_log_match /CLIPP ANNOUNCE: base/
      assert_log_match /CLIPP ANNOUNCE: extension/
    end

    def test_parse_custom
      clipp(
        :input_hashes => [make_request(':a:b:c:d,txt')],
        :modules => ['ps'],
        :default_site_config => <<-EOS
          Rule REQUEST_URI_RAW @parsePath ":," id:1 phase:REQUEST_HEADER CAPTURE:parsed_path
          Rule parsed_path:directory @streq ":a:b:c" id:2 phase:REQUEST_HEADER clipp_announce:directory
          Rule parsed_path:file @streq "d,txt" id:3 phase:REQUEST_HEADER clipp_announce:file
          Rule parsed_path:base @streq "d" id:4 phase:REQUEST_HEADER clipp_announce:base
          Rule parsed_path:extension @streq "txt" id:5 phase:REQUEST_HEADER clipp_announce:extension
        EOS
      )
      assert_no_issues
      assert_log_match /CLIPP ANNOUNCE: directory/
      assert_log_match /CLIPP ANNOUNCE: file/
      assert_log_match /CLIPP ANNOUNCE: base/
      assert_log_match /CLIPP ANNOUNCE: extension/
    end
  end
end