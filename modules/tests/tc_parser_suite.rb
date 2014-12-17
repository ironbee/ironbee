psparse_path = File.join(ENV['abs_builddir'], 'psparse_path')

if ! File.exists?(psparse_path) # May not be part of build.
  puts "No psparse; Skipping tests."

else

  class TestParserSuite < CLIPPTest::TestCase

    parallelize_me!

    def parse_psparse(text)
      r = {}
      text.split("\n").each do |x|
        if x =~ /^([^=]+)=(.+)$/
          r[$1] = $2
        end
      end
      r
    end

    def psparse(parser, input)
      r,w = IO.pipe
      ir,iw = IO.pipe

      pid = fork do
        r.close
        iw.close
        STDIN.reopen(ir)
        STDOUT.reopen(w)
        STDERR.reopen(w)

        exec(psparse_path, parser)
      end

      w.close
      ir.close
      iw.write(input)
      iw.close
      output = r.read
      pid, status = Process::wait2(pid)

      if status.exitstatus != 0
        STDERR.puts output
        nil
      else
        parse_psparse(output)
      end
    end

    def assert_parse(parser, text, expected)
      result = psparse(parser, text)
      assert(! result.nil?, "Parse failed: #{parser}(#{text})")
      expected.each do |k, v|
        r = result[k.to_s]
        assert(v == result[k.to_s], "Expect #{k} to be #{v} but was #{r} on input #{text}")
        result.delete(k.to_s)
      end
      result.each do |k, v|
        assert(v.empty?, "Result unexpected: #{k} = #{v} on input #{text}")
      end
    end

    # Tests

    def test_parse_uri
      assert_parse('uri',
        'http://foo.com',
        :scheme    => 'http',
        :authority => 'foo.com'
      )

      assert_parse('uri',
        'http://user:password@foo.com:1000/a/b/c',
        :scheme    => 'http',
        :authority => 'user:password@foo.com:1000',
        :path     =>  '/a/b/c'
      )

      assert_parse('uri',
        'http://foo.com/a/b/c?x=1&y=2#foo',
        :scheme    => 'http',
        :authority => 'foo.com',
        :path      => '/a/b/c',
        :query     => 'x=1&y=2',
        :fragment  => 'foo'
      )

      assert_parse('uri',
        'http://foo.com?x=1&y=2#foo',
        :scheme    => 'http',
        :authority => 'foo.com',
        :query     => 'x=1&y=2',
        :fragment  => 'foo'
      )

      assert_parse('uri',
        'http://foo.com/a/b/c#foo',
        :scheme    => 'http',
        :authority => 'foo.com',
        :path      => '/a/b/c',
        :fragment  => 'foo'
      )

      assert_parse('uri',
        '?x=1&y=2#foo',
        :query     => 'x=1&y=2',
        :fragment  => 'foo'
      )

      assert_parse('uri',
        '/a/b/c?x=1&y=2#foo',
        :path      => '/a/b/c',
        :query     => 'x=1&y=2',
        :fragment  => 'foo'
      )
    end

    def test_parse_request_line
      assert_parse('request_line',
        'GET / HTTP/1.1',
        :method  => 'GET',
        :uri     => '/',
        :version => 'HTTP/1.1'
      )

      assert_parse('request_line',
        'GET /',
        :method  => 'GET',
        :uri     => '/'
      )
    end

    def test_parse_response_line
      assert_parse('response_line',
        'HTTP/1.1 200 All is okay...',
        :version => 'HTTP/1.1',
        :status => '200',
        :message => 'All is okay...'
      )
      assert_parse('response_line',
        'HTTP/1.1 201',
        :version => 'HTTP/1.1',
        :status => '201'
      )
    end

    def test_parse_authority
      assert_parse('authority',
        "foo.com",
        :host => 'foo.com'
      )

      assert_parse('authority',
        "foo.com:1000",
        :host => 'foo.com',
        :port => '1000'
      )

      assert_parse('authority',
        "user@foo.com",
        :username => 'user',
        :host => 'foo.com'
      )

      assert_parse('authority',
        "user:password@foo.com",
        :username => 'user',
        :password => 'password',
        :host => 'foo.com'
      )

      assert_parse('authority',
        "user:password@foo.com:1000",
        :username => 'user',
        :password => 'password',
        :host => 'foo.com',
        :port => '1000'
      )

      assert_parse('authority',
        "user:@foo.com:1000",
        :username => 'user',
        :host => 'foo.com',
        :port => '1000'
      )

      assert_parse('authority',
        ":password@foo.com:1000",
        :password => 'password',
        :host => 'foo.com',
        :port => '1000'
      )

      assert_parse('authority',
        "@foo.com:1000",
        :host => 'foo.com',
        :port => '1000'
      )

      assert_parse('authority',
        "@:1000",
        :port => '1000'
      )
    end

    def test_parse_headers
      assert_parse('headers',
        'Foo: Bar',
        'Foo' => 'Bar',
        :terminated => 'false'
      )

      assert_parse('headers',
        "Foo: Bar\n\n",
        'Foo' => 'Bar',
        :terminated => 'true'
      )

      assert_parse('headers',
        "Foo: Bar\nHello:World\n",
        'Foo' => 'Bar',
        'Hello' => 'World',
        :terminated => 'false'
      )

      assert_parse('headers',
        "Foo: Bar\n Baz\n Buzz\n\n",
        'Foo' => 'Bar Baz Buzz',
        :terminated => 'true'
      )
    end

    def test_parse_request
      text = <<EOS
GET /hello/world?q=foo HTTP/1.1
Location: testing
Length: 0
Content-Type: something
  long
  and
  weird

EOS
      assert_parse('request',
        text,
        :raw_request_line => 'GET /hello/world?q=foo HTTP/1.1',
        :method => 'GET',
        :uri => '/hello/world?q=foo',
        :path => '/hello/world',
        :query => 'q=foo',
        :version => 'HTTP/1.1',
        'Location' => 'testing',
        'Length' => '0',
        'Content-Type' => 'something long and weird',
        :terminated => 'true'
      )
    end

    def test_parse_response
      text = <<EOS
HTTP/1.1 200 Is ok.
Location: testing
Length: 0
Content-Type: something
  long
  and
  weird

EOS
      assert_parse('response',
        text,
        :raw_response_line => 'HTTP/1.1 200 Is ok.',
        :version => 'HTTP/1.1',
        :status => '200',
        :message => 'Is ok.',
        'Location' => 'testing',
        'Length' => '0',
        'Content-Type' => 'something long and weird',
        :terminated => 'true'
      )
    end

    def test_parse_path
      assert_parse('path',
        'foo',
        :file => 'foo',
        :base => 'foo'
      )
      assert_parse('path',
        'foo/bar',
        :directory => 'foo',
        :file => 'bar',
        :base => 'bar'
      )
      assert_parse('path',
        '/foo/bar',
        :directory => '/foo',
        :file => 'bar',
        :base => 'bar'
      )
      assert_parse('path',
        '/foo/bar/baz',
        :directory => '/foo/bar',
        :file => 'baz',
        :base => 'baz'
      )
      assert_parse('path',
        '/foo/bar/baz.txt',
        :directory => '/foo/bar',
        :file => 'baz.txt',
        :base => 'baz',
        :extension => 'txt'
      )
      assert_parse('path',
        '/foo/bar/baz.txt.zip',
        :directory => '/foo/bar',
        :file => 'baz.txt.zip',
        :base => 'baz.txt',
        :extension => 'zip'
      )
      assert_parse('path',
        'baz.txt.zip',
        :file => 'baz.txt.zip',
        :base => 'baz.txt',
        :extension => 'zip'
      )
    end

  end
end
