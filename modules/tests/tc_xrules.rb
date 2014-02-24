# Integration testing.
class TestXRules < Test::Unit::TestCase
  include CLIPPTest

  def do_clipp_test(action, flag_to_check)
    clipp(
      :input_hashes => [
        simple_hash("GET / HTTP/1.1\nHost: foo.bar\n\n")
      ],
      :config => '''
        LoadModule ibmod_xrules.so
      ''',
      :default_site_config => <<-EOS
        XRuleIpv4 "0.0.0.0/0" #{action}
        Rule #{flag_to_check} @clipp_print #{flag_to_check} id:1 rev:1 phase:RESPONSE_HEADER
      EOS
    )
  end

  [
    [ "EnableBlockingMode",              "FLAGS:blockingMode",          1 ],
    [ "DisableBlockingMode",             "FLAGS:blockingMode",          0 ],
    [ "EnableRequestHeaderInspection",   "FLAGS:inspectRequestHeader",  1 ],
    [ "DisableRequestHeaderInspection",  "FLAGS:inspectRequestHeader",  0 ],
    [ "EnableRequestURIInspection",      "FLAGS:inspectRequestUri",     1 ],
    [ "DisableRequestURIInspection",     "FLAGS:inspectRequestUri",     0 ],
    [ "EnableRequestParamInspection",    "FLAGS:inspectRequestParams",  1 ],
    [ "DisableRequestParamInspection",   "FLAGS:inspectRequestParams",  0 ],
    [ "EnableRequestBodyInspection",     "FLAGS:inspectRequestBody",    1 ],
    [ "DisableRequestBodyInspection",    "FLAGS:inspectRequestBody",    0 ],
    [ "EnableResponseHeaderInspection",  "FLAGS:inspectResponseHeader", 1 ],
    [ "DisableResponseHeaderInspection", "FLAGS:inspectResponseHeader", 0 ],
    [ "EnableResponseBodyInspection",    "FLAGS:inspectResponseBody",   1 ],
    [ "DisableResponseBodyInspection",   "FLAGS:inspectResponseBody",   0 ],
  ].each do |testspec|

    action, flag_to_check, flag_value = testspec

    self.class_eval(
      <<-EOS
        def test_xrules_#{action}
          do_clipp_test("#{action}", "#{flag_to_check}")
          assert_no_issues
          assert_log_match /\\\[#{flag_to_check}\\\]: #{flag_value}/
        end
      EOS
    )
  end

  def test_xruleipv4_no_subnet
    clipp(
      modules: %w{ xrules },
      default_site_config: <<-EOS
        XRuleIpv4 "0.0.0.0" EnableBlockingMode
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
      end
    end

    assert_no_issues
  end

  def test_xruleipv6_no_subnet
    clipp(
      modules: %w{ xrules },
      default_site_config: <<-EOS
        XRuleIpv6 "::1" EnableBlockingMode
      EOS
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1\nHost: foo.bar\n\n")
      end
    end

    assert_no_issues
  end
end
