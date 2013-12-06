# Test features of the core module which may include directives, actions, etc.
class TestAction < Test::Unit::TestCase
  include CLIPPTest

  def test_setvar_init_float
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :config => """
        InitVar A 2.toFloat()
      """,
      :default_site_config => <<-EOS
        Rule A @clipp_print_type "type of A" id:1 rev:1 phase:REQUEST_HEADER 
        Rule A @clipp_print      "val of A"  id:2 rev:1 phase:REQUEST_HEADER 
      EOS
    )
    assert_log_match /val of A.*2/
    assert_log_match /type of A.*FLOAT/
  end

  def test_setvar_init_int
    clipp(
      :input_hashes => [simple_hash("GET /foobar/a\n")],
      :config => """
        InitVar A 2.toInteger()
      """,
      :default_site_config => <<-EOS
        Rule A @clipp_print_type "type of A" id:1 rev:1 phase:REQUEST_HEADER 
        Rule A @clipp_print      "val of A"  id:2 rev:1 phase:REQUEST_HEADER 
      EOS
    )
    assert_log_match /val of A.*2/
    assert_log_match /type of A.*NUMBER/
  end
end
