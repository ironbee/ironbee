require '../../clipp/clipp_test'

class TestConstant < Test::Unit::TestCase
  include CLIPPTest

  def constant_clipp(config = {})
    config[:modules] ||= []
    config[:modules] << 'constant'
    clipp(config) do
      transaction {|t| t.request(raw: "GET /")}
    end
  end
  
  def test_load
    constant_clipp
    assert_no_issues
  end
  
  def test_set
    constant_clipp(
      default_site_config: "ConstantSet Foo Bar"
    )
  end
  
  def test_oracle
    constant_clipp(
      default_site_config: <<-EOS
        ConstantSet Foo Bar
        Rule CONSTANT:Foo @match Bar phase:REQUEST_HEADER id:1 clipp_announce:Foo
        Rule CONSTANT:Baz @nop "" phase:REQUEST_HEADER id:2 clipp_announce:Baz
        Rule CONSTANT:Foo @match Fig phase:REQUEST_HEADER id:3 clipp_announce:BadFoo
      EOS
    )
    assert_log_match /CLIPP ANNOUNCE: Foo/
    assert_log_no_match /CLIPP ANNOUNCE: Baz/
    assert_log_no_match /CLIPP ANNOUNCE: BadFoo/
      assert_no_issues
  end
  
  def test_invalid_constant_set
    constant_clipp(
      default_site_config: "ConstantSet Foo Bar Baz"
    )
    assert_log_match /ConstantSet takes 1 or 2 arguments; has 3/
  end
  
  def test_constant_set_bool
    constant_clipp(
      default_site_config: <<-EOS
        ConstantSet Foo
        Rule CONSTANT:Foo @nop "" phase:REQUEST_HEADER id:1 clipp_announce:Foo
        Rule CONSTANT:Baz @nop "" phase:REQUEST_HEADER id:2 clipp_announce:Baz
      EOS
    )
    assert_log_match /CLIPP ANNOUNCE: Foo/
    assert_log_no_match /CLIPP ANNOUNCE: Baz/
    assert_no_issues
  end
end
