class TestWriteClipp < CLIPPTest::TestCase
  include CLIPPTest

  def generate_example_input
    path = File.join(BUILDDIR, generate_id + "_example.pb")
    clipp(
      consumer: "writepb:#{path}"
    ) do
      connection(
        id: 'test_write_clipp',
        local_ip: '18.19.20.21',
        local_port: 22,
        remote_ip: '23.24.25.26',
        remote_port: 27
      ) do |c|
        c.transaction do |t|
          t.request(
            method: 'GET',
            uri: '/hello/world',
            protocol: 'HTTP/1.9',
            headers: {
              'Host' => 'Foo.Com',
              'Content-Length' => 12
            },
            body: "Hello World!"
          )
          t.response(
            protocol: 'HTTP/1.9',
            status: '200',
            message: 'OK',
            headers: {
              'Content-Length' => 3
            },
            body: "Yo!"
          )
        end
        c.transaction do |t|
          t.request(
            method: 'POST',
            uri: '/a/b',
            protocol: 'HTTP/1.9',
            headers: {
              'Host' => 'Foo.Com',
              'Content-Length' => 1
            },
            body: "X"
          )
          t.response(
            protocol: 'HTTP/1.9',
            status: '200',
            message: 'OK',
            headers: {
              'Content-Length' => 1
            },
            body: "Z"
          )
        end
      end
    end
    assert_no_issues
    path
  end

  def test_write_clipp_conn
    prefix = generate_id
    a = generate_example_input
    b = File.join(BUILDDIR, prefix + "_b.pb")
    clipp(
      input: "pb:#{a}",
      modules: ['write_clipp'],
      config: "RequestBuffering On\nResponseBuffering On\n",
      default_site_config: "Action phase:REQUEST_HEADER id:1 write_clipp_conn:#{b}"
    )
    assert_no_issues

    clipp(input:"pb:#{a}", consumer:"view")
    assert_no_issues
    a_txt = log

    clipp(input:"pb:#{b}", consumer:"view")
    assert_no_issues
    b_txt = log

    # a_txt and b_txt be identical except for the ID.
    a_txt.gsub!(/^---- .+ ----$/, "---- ID ----")
    b_txt.gsub!(/^---- .+ ----$/, "---- ID ----")

    if a_txt != b_txt
      puts "== A: #{a} =="
      puts a_txt

      puts
      puts "== B: #{b} =="
      puts b_txt

      assert(false, "Input does not equal output (up to ID)")
    end
  end

  def test_write_clipp_tx
    prefix = generate_id
    a = generate_example_input
    b = File.join(BUILDDIR, prefix + "_b.pb")
    clipp(
      input: "pb:#{a}",
      modules: ['htp', 'write_clipp'],
      config: "RequestBuffering On\nResponseBuffering On\n",
      default_site_config: "Rule REQUEST_METHOD @match \"POST\" phase:REQUEST_HEADER id:1 write_clipp_tx:#{b}"
    )
    assert_no_issues

    clipp(input:"pb:#{a}", consumer:"view")
    assert_no_issues
    a_txt = log

    clipp(input:"pb:#{b}", consumer:"view")
    assert_no_issues
    b_txt = log

    # a_txt and b_txt be identical except for the ID.
    a_txt.gsub!(/^---- .+ ----$/, "---- ID ----")
    b_txt.gsub!(/^---- .+ ----$/, "---- ID ----")

    assert(a_txt =~ %r{/hello/world})
    assert(b_txt !~ %r{/hello/world})
    assert(b_txt =~ %r{/a/b})
  end

  def test_write_clipp_tx_expansion
    prefix = generate_id

    file = File.join(BUILDDIR, prefix + "_%{tx_id}.pb")

    clipp(
      modules: %w/ htp txdump txvars write_clipp /,
      config: '''
        RequestBuffering On
        ResponseBuffering On
        TxVars On
        TxDump TxFinished stderr +All
      ''',
      default_site_config: """
        Action phase:REQUEST_HEADER id:1 write_clipp_tx:#{file}
      """
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    tx_id = /tx_id = \"(.*)\"/.match(log)[1]

    assert File.exists?(File.join(BUILDDIR, prefix + "_#{tx_id}.pb"))
  end
end
