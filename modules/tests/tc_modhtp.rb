class TestModHTP < CLIPPTest::TestCase
  include CLIPPTest

  def test_modhtp_load
    clipp(
      modules: %w[ htp ],
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1", headers: { Host: 'www.myhost.com' })
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
  end

  def test_modhtp_finds_invalid_host_headers
    clipp(
      modules: %w[ htp ],
      default_site_config: '''
        Rule HTP_REQUEST_FLAGS  @eq "1" id:1 rev:1 phase:LOGGING "clipp_announce:REQ - %{FIELD_NAME}=%{FIELD}"
        Rule HTP_RESPONSE_FLAGS @eq "1" id:2 rev:1 phase:LOGGING "clipp_announce:RESP - %{FIELD_NAME}=%{FIELD}"
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1", headers: { Host: 'www.myh/ost.com' , 'Content-Type' => 'text/plain'})
        t.response(raw: "HTTP/1.1 200 OK", headers: { 'Content-Type' => 'text/plain'})
      end
    end

    assert_no_issues
    assert_log_match 'REQ - HOSTH_INVALID=1'
    assert_log_match 'RESP - HOSTH_INVALID=1'
  end

  def test_modhtp_defines_empty_array
    clipp(
      modules: %w[ htp ],
      default_site_config: '''
        Rule HTP_REQUEST_FLAGS  @clipp_print "req"  id:1 rev:1 phase:LOGGING
        Rule HTP_RESPONSE_FLAGS @clipp_print "resp" id:2 rev:1 phase:LOGGING
        Rule I_AM_NULL          @clipp_print "null" id:3 rev:1 phase:LOGGING
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end
    # Ensure that clipp_print prints "NULL" for undefined fields.
    assert_log_match /clipp_print \[null\]: NULL/

    # If the above assert works, then these lines should NOT appear
    assert_log_no_match /clipp_print \[req\]: NULL/
    assert_log_no_match /clipp_print \[resp\]: NULL/
  end

  def test_modhtp_null_headers_work
    clipp(
      modules: %w[ htp ],
      default_site_config: '''
        Rule HTP_REQUEST_FLAGS  @eq "1" id:1 rev:1 phase:LOGGING "clipp_announce:REQ - %{FIELD_NAME}=%{FIELD}"
        Rule HTP_RESPONSE_FLAGS @eq "1" id:2 rev:1 phase:LOGGING "clipp_announce:RESP - %{FIELD_NAME}=%{FIELD}"
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET / HTTP/1.1")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_no_issues
    assert_log_match 'REQ - HOST_MISSING=1'
    assert_log_match 'RESP - HOST_MISSING=1'
  end
end