require 'tmpdir'
require 'fileutils'

# Test features of the core module which may include directives, actions, etc.
class TestPersistence < Test::Unit::TestCase
  include CLIPPTest

  def test_init_collection_1
    clipp(
      modules: %w[ persistence_framework persist init_collection ],
      config: """
        InitCollection A vars: a=2.toFloat() b=1.toInteger()
      """,
      default_site_config: <<-EOS
        Rule A:a @clipp_print_type "type of A:a" id:1 rev:1 phase:REQUEST_HEADER
        Rule A:a @clipp_print      "val of A:a"  id:2 rev:1 phase:REQUEST_HEADER
        Rule A:b @clipp_print_type "type of A:b" id:3 rev:1 phase:REQUEST_HEADER
        Rule A:b @clipp_print      "val of A:b"  id:4 rev:1 phase:REQUEST_HEADER
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET /foobar/a\n")
      end
    end

    assert_log_match /val of A:a.*2/
    assert_log_match /type of A:a.*FLOAT/
    assert_log_match /val of A:b.*1/
    assert_log_match /type of A:b.*NUMBER/
  end

  def test_persist
    clipp(
      modules: %w[ persistence_framework persist ],
      config: """
        PersistenceStore persist persist-fs:///tmp/ironbee
      """,
      default_site_config: <<-EOS
        PersistenceMap IP persist key=%{REMOTE_ADDR} expire=300

        Action id:1 rev:1 phase:REQUEST_HEADER "setvar:IP:count+=1"
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET /foobar/a\n")
      end
      transaction do |t|
        t.request(raw: "GET /foobar/a\n")
      end
    end

    assert_no_issues
  end
end
