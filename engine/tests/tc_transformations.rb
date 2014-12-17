class TestTransformations < CLIPPTest::TestCase
  include CLIPPTest

  def test_tfn_first
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :default_site_config => <<-EOS
        Action id:1 rev:1 phase:REQUEST_HEADER setvar:list:element1=1
        Action id:2 rev:1 phase:REQUEST_HEADER setvar:list:element2=2

        Rule list.first() @eq 1 id:3 rev:1 phase:REQUEST_HEADER clipp_announce:result
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: result/
  end

  def test_tfn_last
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :default_site_config => <<-EOS
        Action id:1 rev:1 phase:REQUEST_HEADER setvar:list:element1=1
        Action id:2 rev:1 phase:REQUEST_HEADER setvar:list:element2=2

        Rule list.last() @eq 2 id:3 rev:1 phase:REQUEST_HEADER clipp_announce:result
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: result/
  end

  def test_tfn_first_noexist
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :default_site_config => <<-EOS
        Rule list.first() @eq 1 id:1 rev:1 phase:REQUEST_HEADER clipp_announce:result
      EOS
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: result/
  end

  def test_tfn_last_noexist
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :default_site_config => <<-EOS
        Rule list.last() @eq 1 id:1 rev:1 phase:REQUEST_HEADER clipp_announce:result
      EOS
    )
    assert_no_issues
    assert_log_no_match /CLIPP ANNOUNCE: result/
  end

  def test_tfn_first_notlist
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :default_site_config => <<-EOS
        Action id:1 rev:1 phase:REQUEST_HEADER setvar:list=1
        Rule list.first() @eq 1 id:2 rev:1 phase:REQUEST_HEADER clipp_announce:result
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: result/
  end

  def test_tfn_last_notlist
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :default_site_config => <<-EOS
        Action id:1 rev:1 phase:REQUEST_HEADER setvar:list=1
        Rule list.last() @eq 1 id:2 rev:1 phase:REQUEST_HEADER clipp_announce:result
      EOS
    )
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: result/
  end

  # Obseved bug in testing.
  def test_init_collection_assert_on_missing_tfn
    clipp(
      modules: %w[ ],
      default_site_config: <<-EOS
        InitVar tests 234.567.is_int()
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / foo\n")
        t.response(raw: "HTTP/1.1 200 OK\n\n")
      end
    end

    # Check for the proper error that caused a crash.
    assert_log_match 'Cannot find transformation "is_int".'

    # Check that we didn't crash, but just failed to configure the engine.
    assert_log_match 'Failed to configure the IronBee engine.'
  end
end

