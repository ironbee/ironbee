class TestUTF8 < CLIPPTest::TestCase
  include CLIPPTest

  # Make sure that the string encoder works.
  def test_utf8_check_stringencoders_works
    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        InitVar "A" "\x61"
      ''',
      default_site_config: '''
        Rule A.smart_hex_decode() @clipp_print "A" id:1 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A]: a'
  end

  def test_utf8_validateUtf8_ok
    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        InitVar "A" "\x61"
      ''',
      default_site_config: '''
        Rule A.smart_hex_decode() @validateUtf8 ""  "setvar:A=b" id:1 rev:1 phase:REQUEST
        Rule A                    @clipp_print  "A" "setvar:A=b" id:2 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A]: b'
  end

  def test_utf8_validateUtf8_overlong_notok
    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        InitVar "A" "\xc0\x61"
      ''',
      default_site_config: '''
        Rule A.smart_hex_decode() @validateUtf8 ""  "setvar:A=b" id:1 rev:1 phase:REQUEST
        Rule A                    @clipp_print  "A" id:2 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match 'clipp_print [A]: \xc0\x61'
  end

  def test_utf8_validateUtf8_multibyte
    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        InitVar "A" "\xe2\x82\xac"
      ''',
      default_site_config: '''
        Rule A.smart_hex_decode() @validateUtf8 ""  id:1 rev:1 phase:REQUEST "setvar:A=b"
        Rule A                    @clipp_print  "A" id:2 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match "clipp_print [A]: b"
  end

  def test_utf8_normalizeUtf8_overlong_a
    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        # Overlong "a".
        InitVar "A" "\xc1\xa1"
      ''',
      default_site_config: '''
        Rule A.smart_hex_decode().normalizeUtf8() @streq "a"  id:1 rev:1 phase:REQUEST "setvar:A=b"
        Rule A @clipp_print  "A" id:2 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match "clipp_print [A]: b"
  end


  def test_utf8_normalizeUtf8_overlong_dot
    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        # Overlong "a".
        InitVar "A" "%c0%ae"
      ''',
      default_site_config: '''
        Rule A.smart_url_hex_decode().normalizeUtf8() @streq "."  id:1 rev:1 phase:REQUEST "setvar:A=b"
        Rule A.smart_url_hex_decode().normalizeUtf8() @clipp_print  "A" id:2 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match "clipp_print [A]: b"
  end

  def test_utf8_normalizeUtf8_very_overlong_a
    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        # Overlong "a".
        InitVar "A" "\xf0\x80\x81\xa1"
      ''',
      default_site_config: '''
        Rule A.smart_hex_decode().normalizeUtf8() @streq "a"  id:1 rev:1 phase:REQUEST "setvar:A=b"
        Rule A @clipp_print  "A" id:2 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match "clipp_print [A]: b"
  end

  # Test to ensure we don't break regular characters.
  def test_utf8_normalizeUtf8_a
    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        # Normal "a".
        InitVar "A" "\x61"
      ''',
      default_site_config: '''
        Rule A.smart_hex_decode().normalizeUtf8() @streq "a"  id:1 rev:1 phase:REQUEST "setvar:A=b"
        Rule A @clipp_print  "A" id:2 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match "clipp_print [A]: b"
  end

  # Test that a broken byte generates the UTF-8 replacement character.
  def test_utf8_normalizeUtf8_invalid_and_a
    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        # Normal "a".
        InitVar "A" "\xf0\x61"
      ''',
      default_site_config: '''
        Rule A.smart_hex_decode().normalizeUtf8() @clipp_print  "A" id:2 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match "clipp_print [A]: \xef\xbf\xbda".force_encoding('binary')
  end

  # This builds on the test_utf8_normalizeUtf8_invalid_and_a test.
  def test_utf8_removeUtf8ReplacementCharacter
    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        # Normal "a".
        InitVar "A" "\xef\xbf\xbd\x61\xef\xbf\xbd"
      ''',
      default_site_config: '''
        Rule A.smart_hex_decode().removeUtf8ReplacementCharacter() @clipp_print  "A" id:2 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match "clipp_print [A]: a"
  end

  def test_utf8_normalizeUtf8_empty
    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        InitVar "A" ""
      ''',
      default_site_config: '''
        Rule A.smart_hex_decode().normalizeUtf8() @streq "a"  id:1 rev:1 phase:REQUEST "setvar:A=b"
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
  end

  # Because a library does the conversion, just make sure the code executes w/o errors.
  def test_utf8_utf8To16
    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        InitVar "A" "a"
      ''',
      default_site_config: '''
        Rule A.utf8To16().utf16to8() @clipp_print "A"  id:1 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match "clipp_print [A]: a"
  end

  # Because a library does the conversion, just make sure the code executes w/o errors.
  def test_utf8_utf8To32
    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        InitVar "A" "a"
      ''',
      default_site_config: '''
        Rule A.utf8To32().utf32to8() @clipp_print "A"  id:1 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match "clipp_print [A]: a"
  end

  # Because a library does the conversion, just make sure the code executes w/o errors.
  def test_utf8_replaceInvalidUtf8

    # ClippTest converts the resultant IronBee log to ASCII-8BIT.
    # This 3-byte sequence is what the utf8 invalid character \ufffd,
    # is converted to.
    invalid_char = [0xef, 0xbf, 0xbd].pack('ccc')

    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        InitVar "A" "\xc1\xa1hello\xff"
      ''',
      default_site_config: '''
        Rule A.smart_hex_decode().replaceInvalidUtf8() @clipp_print "A"  id:1 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert (@log.index("%shello%s"%[invalid_char, invalid_char]) > 0)
  end

  def test_utf8_utf8ToAscii

    clipp(
      modules: %w/ utf8 smart_stringencoders /,
      config: '''
        InitVar "A" "\xc3\xbf"
      ''',
      default_site_config: '''
        Rule A.smart_hex_decode().utf8ToAscii() @clipp_print "A"  id:1 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match "clipp_print [A]: y"
  end

  def test_utf8_utf8ToAscii_list

    clipp(
      modules: %w/ persistence_framework init_collection utf8 smart_stringencoders /,
      config: '''
        InitCollection "A" vars: "a=\xc3\xbf" "b=\xc3\xbf"
      ''',
      default_site_config: '''
        Rule A.smart_hex_decode().utf8ToAscii() @clipp_print "A"  id:1 rev:1 phase:REQUEST
      ''',
    ) do
      transaction do |t|
        t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
        t.response(raw: 'HTTP/1.1 200 OK')
      end
    end

    assert_no_issues
    assert_log_match "clipp_print [A]: y"
  end

  [
    [ '..%c0%afFile.txt', '../File.txt' ],
    [ '%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/%c0%ae%c0%ae/File.txt',
      '../../../../../../../../File.txt' ],
    [ '..%c1%9cFile.txt', '..\\File.txt' ],
    [ 'http:\/\/www.example.com\/cgi-bin\/bad.cgi?foo=..%c1%9c..\/bin\/ls%20-al',
      'http:\/\/www.example.com\/cgi-bin\/bad.cgi?foo=..\\..\/bin\/ls -al' ],
  ].each_with_index do |args, idx|
    input, expected = args

    define_method "test_utf8_url_#{idx}".to_sym do
      clipp(
        modules: %w/ utf8 smart_stringencoders /,
        config: """
          InitVar \"A\" #{input}
        """,
        default_site_config: '''
          Rule A.smart_url_hex_decode().normalizeUtf8() @clipp_print "A"  id:1 rev:1 phase:REQUEST
        ''',
      ) do
        transaction do |t|
          t.request(raw: 'GET / HTTP/1.1', headers: { Host: 'a.b.c' })
          t.response(raw: 'HTTP/1.1 200 OK')
        end
      end

      assert_no_issues
      assert_log_match "clipp_print [A]: #{expected}"
    end
  end
end