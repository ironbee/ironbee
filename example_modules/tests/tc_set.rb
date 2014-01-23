require File.join(File.dirname(__FILE__), '..', '..', 'clipp', 'clipp_test')

class TestSet < Test::Unit::TestCase
  include CLIPPTest

  CONFIGS = [
    'LoadModule "' + BUILDDIR + '/../.libs/ibmod_set_c.so"',
    'LoadModule "' + BUILDDIR + '/../.libs/ibmod_set_cpp.so"'
  ]

  def make_request(method)
    simple_hash("#{method} /foobar/a HTTP/1.1\nHost: foo.bar\n\n")
  end

  def test_load
    CONFIGS.each do |config|
      clipp(
        :config => config,
        :input_hashes => [make_request('GET')],
        :modules => ['pcre'],
        :default_site_config => <<-EOS
          Rule REQUEST_METHOD @rx GET id:1 phase:REQUEST_HEADER clipp_announce:YES
        EOS
      )
      assert_no_issues
      assert_log_match /CLIPP ANNOUNCE: YES/
    end
  end

  def test_basic
    CONFIGS.each do |config|
      clipp(
        :config => config,
        :input_hashes => [make_request('GET')],
        :default_site_config => <<-EOS
          SetDefine set1 GET POST
          SetDefine set2 FOO BAR
          Rule REQUEST_METHOD @set_member "set1" id:1 phase:REQUEST_HEADER clipp_announce:YES
          Rule REQUEST_METHOD @set_member "set2" id:2 phase:REQUEST_HEADER clipp_announce:NO
        EOS
      )
      assert_no_issues
      assert_log_match /CLIPP ANNOUNCE: YES/
      assert_log_no_match /CLIPP ANNOUNCE: NO/
    end
  end

  def test_debug
    CONFIGS.each do |config|
      clipp(
        :config => config,
        :input_hashes => [make_request('GET')],
        :default_site_config => <<-EOS
          Set set.debug 1
          SetDefine set1 GET POST
          SetDefine set2 FOO BAR
          Rule REQUEST_METHOD @set_member "set1" id:1 phase:REQUEST_HEADER clipp_announce:YES
          Rule REQUEST_METHOD @set_member "set2" id:2 phase:REQUEST_HEADER clipp_announce:NO
        EOS
      )
      assert_no_issues
      assert_log_match /set_member set1 for GET = yes/
      assert_log_match /set_member set2 for GET = no/
    end
  end

  def test_inheritance
    CONFIGS.each do |config|
      clipp(
        :config => config + "\nSetDefine set1 GET POST",
        :input_hashes => [make_request('GET')],
        :default_site_config => <<-EOS
          SetDefine set2 FOO BAR
          Rule REQUEST_METHOD @set_member "set1" id:1 phase:REQUEST_HEADER clipp_announce:YES
          Rule REQUEST_METHOD @set_member "set2" id:2 phase:REQUEST_HEADER clipp_announce:NO
        EOS
      )
      assert_no_issues
      assert_log_match /CLIPP ANNOUNCE: YES/
      assert_log_no_match /CLIPP ANNOUNCE: NO/
    end
  end

  def test_from_file
    CONFIGS.each do |config|
      clipp(
        :config => config,
        :input_hashes => [make_request('GET')],
        :default_site_config => <<-EOS
          SetDefineFromFile set1 set.txt
          SetDefine set2 FOO BAR
          Rule REQUEST_METHOD @set_member "set1" id:1 phase:REQUEST_HEADER clipp_announce:YES
          Rule REQUEST_METHOD @set_member "set2" id:2 phase:REQUEST_HEADER clipp_announce:NO
        EOS
      )
      assert_no_issues
      assert_log_match /CLIPP ANNOUNCE: YES/
      assert_log_no_match /CLIPP ANNOUNCE: NO/

      clipp(
        :config => config,
        :input_hashes => [make_request('get')],
        :default_site_config => <<-EOS
          SetDefineFromFile set1 set.txt
          Rule REQUEST_METHOD @set_member "set1" id:1 phase:REQUEST_HEADER clipp_announce:YES
        EOS
      )
      assert_no_issues
      assert_log_no_match /CLIPP ANNOUNCE: YES/

      clipp(
        :config => config,
        :input_hashes => [make_request('get')],
        :default_site_config => <<-EOS
          SetDefineInsensitiveFromFile set1 set.txt
          Rule REQUEST_METHOD @set_member "set1" id:1 phase:REQUEST_HEADER clipp_announce:YES
        EOS
      )
      assert_no_issues
      assert_log_match /CLIPP ANNOUNCE: YES/
    end
  end

  def test_case_sensitivity
    CONFIGS.each do |config|
      clipp(
        :config => config,
        :input_hashes => [make_request('GET')],
        :default_site_config => <<-EOS
          SetDefine set1 get post
          Rule REQUEST_METHOD @set_member "set1" id:1 phase:REQUEST_HEADER clipp_announce:YES
        EOS
      )
      assert_no_issues
      assert_log_no_match /CLIPP ANNOUNCE: YES/
      clipp(
        :config => config,
        :input_hashes => [make_request('GET')],
        :default_site_config => <<-EOS
          SetDefineInsensitive set1 get post
          Rule REQUEST_METHOD @set_member "set1" id:1 phase:REQUEST_HEADER clipp_announce:YES
        EOS
      )
      assert_no_issues
      assert_log_match /CLIPP ANNOUNCE: YES/
    end
  end
end