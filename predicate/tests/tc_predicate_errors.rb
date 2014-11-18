class TestPredicateErrors < CLIPPTest::TestCase
  include CLIPPTest

  def simple_clipp(config = {})
    config[:predicate] = true
    clipp(config) do
      transaction {|t| t.request(raw: "GET /")}
    end
  end

  def test_template_arg
    simple_clipp(
      default_site_config: <<-EOS
        PredicateDefine "foo" "a" "(lt 5 (var (ref 'a')))"
        Action id:1 phase:REQUEST_HEADER "predicate:(foo [1])"
      EOS
    )

    assert_no_clean_exit
    assert_log_match /\(var \[1\]\) : Child 1 must be a string literal./
    assert_log_match /origin .*clipp_test_template_arg_.*\.config:\d+ \(var \(ref 'a'\)\)/
    assert_log_match /root \(lt 5 \(var \[1\]\)\)/
    assert_log_match /origin .*clipp_test_template_arg_.*\.config:\d+ \(foo \[1\]\)/
    assert_log_match /origin .*clipp_test_template_arg_.*\.config:\d+ \(lt 5 \(var \(ref 'a'\)\)\)/
  end
end

