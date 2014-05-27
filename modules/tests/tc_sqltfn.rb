# Integration testing.
class TestSqltfn < Test::Unit::TestCase
  include CLIPPTest

  def test_sqltfn_zero_length_input
    clipp(
      modules: ['sqltfn'],
      default_site_config: <<-EOS
        InitVar A ""
        Action id:1 rev:1 phase:REQUEST "setvar:B=%{A}.normalizeSqlPg()"
        Rule A @clipp_print "A" id:2 rev:1 phase:REQUEST
        Rule B @clipp_print "B" id:3 rev:1 phase:REQUEST
      EOS
    ) do
      transaction do |t|
        t.request(
          method: "GET",
          uri: "/",
          protocol: "HTTP/1.1",
          headers: {"Host" => "foo.bar"}
        )
      end
    end

    assert_no_issues
    assert_log_match /clipp_print \[A\]:.*$/
    assert_log_match /clipp_print \[B\]:.*$/
  end
end
