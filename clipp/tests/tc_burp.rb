
class TestTesting < CLIPPTest::TestCase

  def test_burp_proxy_wp431_install_traffic_xml
    f = File.join(SRCDIR, 'burp_proxy_wp431_install_traffic.xml')
    clipp(
      input: "burp:#{f} @parse",
      modhtp: true,
      consumer: 'ironbee:IRONBEE_CONFIG @view:summary',
      default_site_config: '''
        Rule ARGS @clipp_print "Args" id:rule1 rev:1 phase:REQUEST
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET /local HTTP/1.1")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_log_no_match /CLIPP INPUT:  .*\.xml NO CONNECTION INFO/
    assert_log_match 'clipp_print [Args]: 1.11.3'
    assert_log_match 'clipp_print [Args]: 1.2.1'
    assert_log_match 'clipp_print [Args]: 4.3.1'
    assert_log_match 'clipp_print [Args]: 20131107'
    assert_log_match 'clipp_print [Args]: 1'
    assert_log_match 'clipp_print [Args]: 2'
    assert_log_match 'clipp_print [Args]: wordpress'
    assert_log_match 'clipp_print [Args]: root'
    assert_log_match 'clipp_print [Args]: decision'
    assert_log_match 'clipp_print [Args]: localhost'
    assert_log_match 'clipp_print [Args]: wp431_'
    assert_log_match 'clipp_print [Args]: '
    assert_log_match 'clipp_print [Args]: Submit'
    assert_log_match 'clipp_print [Args]: en_US'
    assert_log_match 'clipp_print [Args]: 1.0'
    assert_log_match 'clipp_print [Args]: 1.6.0'
    assert_log_match 'clipp_print [Args]: 20131107'
    assert_log_match 'clipp_print [Args]: adminpa$$'
    assert_log_match 'clipp_print [Args]: on'
    assert_log_match 'clipp_print [Args]: test@example.com'
    assert_log_match 'clipp_print [Args]: Install WordPress'
    assert_log_match 'clipp_print [Args]: en_US'

  end

  def test_burp_spider_wp431_install_traffic_xml
    f = File.join(SRCDIR, 'burp_spider_wp431_install_traffic.xml')
    clipp(
      input: "burp:#{f} @parse",
      modhtp: true,
      consumer: 'ironbee:IRONBEE_CONFIG @view:summary',
      default_site_config: '''
        Rule ARGS @clipp_print "Args" id:rule1 rev:1 phase:REQUEST
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET /local HTTP/1.1")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_log_no_match /CLIPP INPUT:  .*\.xml NO CONNECTION INFO/

    assert_log_match 'clipp_print [Args]: 20131107'
    assert_log_match 'clipp_print [Args]: en_US'
    assert_log_match 'clipp_print [Args]: 2'
    assert_log_match 'clipp_print [Args]: wordpress - 4.3.1'
    assert_log_match 'clipp_print [Args]: adminuser'
    assert_log_match 'clipp_print [Args]: adminpa$$'
    assert_log_match 'clipp_print [Args]: on'
    assert_log_match 'clipp_print [Args]: test@example.com'
    assert_log_match 'clipp_print [Args]: Install WordPress'
    assert_log_match 'clipp_print [Args]: en_US'
    assert_log_match 'clipp_print [Args]: 4.3.1'
    assert_log_match 'clipp_print [Args]: 2'
    assert_log_match 'clipp_print [Args]: wordpress'
    assert_log_match 'clipp_print [Args]: root'
    assert_log_match 'clipp_print [Args]: decision'
    assert_log_match 'clipp_print [Args]: localhost'
    assert_log_match 'clipp_print [Args]: wp431_'
    assert_log_match 'clipp_print [Args]: Submit'
    assert_log_match 'clipp_print [Args]: 1'
    assert_log_match 'clipp_print [Args]: 1.2.1'
    assert_log_match 'clipp_print [Args]: 1.11.3'
    assert_log_match 'clipp_print [Args]: 1.6.0'
    assert_log_match 'clipp_print [Args]: 4.3.1'
    assert_log_match 'clipp_print [Args]: 1.0'

  end

  def test_burp_base64
    f = File.join(SRCDIR, 'burp_proxy_base64.xml')
    clipp(
      input: "burp:#{f} @parse",
      modhtp: true,
      consumer: 'ironbee:IRONBEE_CONFIG @view:summary',
      default_site_config: '''
        Rule ARGS @clipp_print "Args" id:rule1 rev:1 phase:REQUEST
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET /local HTTP/1.1")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_log_no_match /CLIPP INPUT:  .*\.xml NO CONNECTION INFO/
    assert_log_match 'clipp_print [Args]: 1.11.3'
    assert_log_match 'clipp_print [Args]: 1.2.1'
    assert_log_match 'clipp_print [Args]: 4.3.1'
    assert_log_match 'clipp_print [Args]: 20131107'
    assert_log_match 'clipp_print [Args]: 1'
    assert_log_match 'clipp_print [Args]: 2'
    assert_log_match 'clipp_print [Args]: wordpress'
    assert_log_match 'clipp_print [Args]: root'
    assert_log_match 'clipp_print [Args]: decision'
    assert_log_match 'clipp_print [Args]: localhost'
    assert_log_match 'clipp_print [Args]: wp431_'
    assert_log_match 'clipp_print [Args]: '
    assert_log_match 'clipp_print [Args]: Submit'
    assert_log_match 'clipp_print [Args]: en_US'
    assert_log_match 'clipp_print [Args]: 1.0'
    assert_log_match 'clipp_print [Args]: 1.6.0'
    assert_log_match 'clipp_print [Args]: 20131107'
    assert_log_match 'clipp_print [Args]: adminpa$$'
    assert_log_match 'clipp_print [Args]: on'
    assert_log_match 'clipp_print [Args]: test@example.com'
    assert_log_match 'clipp_print [Args]: Install WordPress'
    assert_log_match 'clipp_print [Args]: en_US'

  end

  def test_burp_base64_2
    f = File.join(SRCDIR, 'burp_proxy_base64_2.xml')
    clipp(
      input: "burp:#{f} @parse",
      modhtp: true,
      consumer: 'ironbee:IRONBEE_CONFIG @view:summary',
      default_site_config: '''
        Rule ARGS @clipp_print "Args" id:rule1 rev:1 phase:REQUEST
      '''
    ) do
      transaction do |t|
        t.request(raw: "GET /local HTTP/1.1")
        t.response(raw: "HTTP/1.1 200 OK")
      end
    end

    assert_log_no_match /CLIPP INPUT:  .*\.xml NO CONNECTION INFO/

  end
end