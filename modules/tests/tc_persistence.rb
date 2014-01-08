require 'tmpdir'
require 'fileutils'

# Test features of the core module which may include directives, actions, etc.
class TestAction < Test::Unit::TestCase
  include CLIPPTest

  def test_init_collection_1
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :config => """
        LoadModule ibmod_persistence_framework.so
        LoadModule ibmod_persist.so
        LoadModule ibmod_init_collection.so
        InitCollection A vars: a=2.toFloat() b=1.toInteger()
      """,
      :default_site_config => <<-EOS
        Rule A:a @clipp_print_type "type of A:a" id:1 rev:1 phase:REQUEST_HEADER
        Rule A:a @clipp_print      "val of A:a"  id:2 rev:1 phase:REQUEST_HEADER
        Rule A:b @clipp_print_type "type of A:b" id:3 rev:1 phase:REQUEST_HEADER
        Rule A:b @clipp_print      "val of A:b"  id:4 rev:1 phase:REQUEST_HEADER
      EOS
    )
    assert_log_match /val of A:a.*2/
    assert_log_match /type of A:a.*FLOAT/
    assert_log_match /val of A:b.*1/
    assert_log_match /type of A:b.*NUMBER/
  end
end
