# Also see test_module_ee_oper.cpp
# TODO: move test_module_ee_oper tests into this file.

class TestEE < CLIPPTest::TestCase

  parallelize_me!

  include CLIPPTest

  ABIN = File.join(TOP_BUILDDIR, 'automata', 'bin')
  TRIE = File.join(ABIN, 'trie_generator')
  AC = File.join(ABIN, 'ac_generator')
  EC = File.join(ABIN, 'ec')

  def trie(input, output)
    File.open(output, 'w') do |out|
      IO.popen(TRIE, 'r+') do |io|
        io.puts input.join("\n")
        io.close_write
        out.write io.read
      end
    end
    ec(output)
  end

  def ac(input, output)
    File.open(output, 'w') do |out|
      IO.popen(AC, 'r+') do |io|
        io.puts input.join("\n")
        io.close_write
        out.write io.read
      end
    end
    ec(output)
  end

  def acp(input, output)
    File.open(output, 'w') do |out|
      IO.popen([AC, '-p'], 'r+') do |io|
        io.puts input.join("\n")
        io.close_write
        out.write io.read
      end
    end
    ec(output)
  end

  def ec(input)
    system(EC, input)
    input.gsub(/\.a$/, '.e')
  end

  def test_non_streaming
    id = generate_id
    prefix = "#{BUILDDIR}/clipp_test_non_streaming_#{id}"
    e = trie(['foo'], "#{prefix}.a")
    clipp(
      id: id,
      modules: ['htp', 'ee'],
      default_site_config: <<-EOS
        LoadEudoxus "test" "#{e}"
        Rule ARGS @ee test id:1 phase:REQUEST_HEADER "clipp_announce:MATCH=%{FIELD_NAME}"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET /?a=foobaz&b=foobar")
      end
    end
    assert_no_issues
    # Important that both a and b match.
    assert_log_match 'CLIPP ANNOUNCE: MATCH=a'
    assert_log_match 'CLIPP ANNOUNCE: MATCH=b'
  end

  def test_ee_match_trie
    id = generate_id
    prefix = "#{BUILDDIR}/clipp_test_ee_match_trie#{id}"
    e = trie(['foo'], "#{prefix}.a")
    clipp(
      id: id,
      modules: ['htp', 'ee'],
      default_site_config: <<-EOS
        LoadEudoxus "test" "#{e}"
        Rule ARGS @ee_match test id:1 phase:REQUEST_HEADER "clipp_announce:MATCH=%{FIELD_NAME}"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET /?a=foobaz&b=foobar&c=foo")
      end
    end
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=a/
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=b/
    assert_log_match /CLIPP ANNOUNCE: MATCH=c/
  end

  def test_ee_match_ac
    id = generate_id
    prefix = "#{BUILDDIR}/clipp_test_ee_match_ac#{id}"
    e = ac(['foo'], "#{prefix}.a")
    clipp(
      id: id,
      modules: ['htp', 'ee'],
      default_site_config: <<-EOS
        LoadEudoxus "test" "#{e}"
        Rule ARGS @ee_match test id:1 phase:REQUEST_HEADER "clipp_announce:MATCH=%{FIELD_NAME}"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET /?a=foobaz&b=foobar&c=foo&d=barfoo&e=arf")
      end
    end
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=a/
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=b/
    assert_log_match /CLIPP ANNOUNCE: MATCH=c/
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=d/
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=e/
  end

  def test_ee_match_ac_pattern
    id = generate_id
    prefix = "#{BUILDDIR}/clipp_test_ee_match_ac_pattern#{id}"
    e = acp(['foo\d\d'], "#{prefix}.a")
    clipp(
      id: id,
      modules: ['htp', 'ee'],
      default_site_config: <<-EOS
        LoadEudoxus "test" "#{e}"
        Rule ARGS @ee test id:1 phase:REQUEST_HEADER "clipp_announce:MATCH=%{FIELD_NAME}"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET /?a=foo17&b=fooba")
      end
    end
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: MATCH=a/
    assert_log_no_match /CLIPP ANNOUNCE: MATCH=b/
  end

  # RNS-839
  def test_ee_module_nothing_to_do
    clipp(
      modules: ['ee']
    ) do
      transaction do |t|
        t.request(
          raw: "GET /"
        )
        t.response(
          raw: "HTTP/1.1 200 OK"
        )
      end
    end
    assert_no_issues
    assert_log_no_match /ENOENT/
  end

  def test_streaming
    id = generate_id
    prefix = "#{BUILDDIR}/clipp_test_streaming#{id}"
    e = trie(['foo'], "#{prefix}.a")
    clipp(
      id: id,
      input: 'pb:INPUT_PATH',
      modules: ['htp', 'ee'],
      default_site_config: <<-EOS
        LoadEudoxus "test" "#{e}"
        StreamInspect request_body_stream @ee test capture id:1 \
          "clipp_announce:MATCH=%{CAPTURE:0}"
      EOS
    ) do
      transaction do |t|
        t.request_started(raw: "GET / HTTP/1.1")
        t.request_header(
          headers: {"Content-Length" => "6"}
        )
        "foobar".each_char {|c| t.request_body(data: c)}
      end
    end
    assert_no_issues
    assert_log_match 'CLIPP ANNOUNCE: MATCH=foo'
  end

  def test_streaming2
    id = generate_id
    prefix = "#{BUILDDIR}/clipp_test_streaming2#{id}"
    e = trie(['foo'], "#{prefix}.a")
    clipp(
      input: 'pb:INPUT_PATH',
      modules: ['htp', 'ee'],
      default_site_config: <<-EOS
        LoadEudoxus "test" "#{e}"
        StreamInspect request_body_stream @ee test capture id:1 \
          "clipp_announce:MATCH=%{CAPTURE:0}"
      EOS
    ) do
      transaction do |t|
        t.request_started(raw: "GET / HTTP/1.1")
        t.request_header(
          headers: {"Content-Length" => "6"}
        )
        t.request_body(data: "foobar")
      end
    end
    assert_no_issues
    assert_log_match 'CLIPP ANNOUNCE: MATCH=foo'
  end
end
