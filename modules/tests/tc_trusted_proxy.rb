class TestTrustedProxy < CLIPPTest::TestCase
  include CLIPPTest

  CONFIG = <<-EOS
    LoadModule "ibmod_trusted_proxy.so"
    TrustedProxyUseXFFHeader on
    TrustedProxyIPs "99.99.99.99/24" 100.100.100.100
  EOS

  def make_site_config(net_list=nil)
    config = ""
    if net_list then
      config += "TrustedProxyIPs #{net_list}\n"
    end
    config += "Rule REMOTE_ADDR @clipp_print \"val of remote_addr\" id:2 rev:1 phase:REQUEST\n"

    return config
  end

  def test_load
    clipp(config: CONFIG,
          modules: ['pcre'],
          default_site_config: <<-EOS
            Rule REQUEST_LINE @rx test id:1 phase:REQUEST_HEADER clipp_announce:basic_rule
          EOS
         ) do
      connection(remote_ip:"5.5.5.5") do |c|
        c.transaction do |t|
          t.request(
                    method: 'GET',
                    uri: '/test',
                    protocol: 'HTTP/1.0',
                    headers: {
                      'Host' => 'Foo.Com',
                      'X-Forwarded-For' => '1.1.1.1 , 2.2.2.2, 3.3.3.3 , 4.4.4.4'
                    }
                   )
        end
      end
    end
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: basic_rule/
  end

  def test_without_module
    clipp(modhtp: true,
          default_site_config: make_site_config()
          ) do
      connection(remote_ip:"5.5.5.5") do |c|
        c.transaction do |t|
          t.request(
                    method: 'GET',
                    uri: '/hello/world',
                    protocol: 'HTTP/1.0',
                    headers: {
                      'Host' => 'Foo.Com',
                      'X-Forwarded-For' => '1.1.1.1 , 2.2.2.2, 3.3.3.3 , 4.4.4.4'
                    }
                   )
        end
      end
    end
    assert_no_issues
    assert_log_match /val of remote_addr.*5\.5\.5\.5/
  end

  def test_xff_default
    clipp(modhtp: true,
          config: <<-EOS,
            LoadModule "ibmod_trusted_proxy.so"
          EOS
          default_site_config: make_site_config()
          ) do
      connection(remote_ip:"5.5.5.5") do |c|
        c.transaction do |t|
          t.request(
                    method: 'GET',
                    uri: '/hello/world',
                    protocol: 'HTTP/1.0',
                    headers: {
                      'Host' => 'Foo.Com',
                      'X-Forwarded-For' => '1.1.1.1 , 2.2.2.2, 3.3.3.3 , 4.4.4.4'
                    }
                    )
        end
      end
    end
    assert_no_issues
    assert_log_match /val of remote_addr.*5\.5\.5\.5/
  end

  def test_untrusted_proxy
    clipp(modhtp: true,
          config: CONFIG,
          default_site_config: make_site_config()
         ) do
      connection(remote_ip:"5.5.5.5") do |c|
        c.transaction() do |t|
          t.request(
                    method: 'GET',
                    uri: '/hello/world',
                    protocol: 'HTTP/1.0',
                    headers: {
                      'Host' => 'Foo.Com',
                      'X-Forwarded-For' => '1.1.1.1 , 2.2.2.2, 3.3.3.3 , 4.4.4.4'
                    }
                    )
        end
      end
    end
    assert_no_issues
    assert_log_match /val of remote_addr.*5\.5\.5\.5/
  end

  def test_additive_networks
    clipp(modhtp: true,
          config: CONFIG,
          default_site_config: make_site_config("+5.5.5.5")
         ) do
      connection(remote_ip:"5.5.5.5") do |c|
        c.transaction() do |t|
          t.request(
                    method: 'GET',
                    uri: '/hello/world',
                    protocol: 'HTTP/1.0',
                    headers: {
                      'Host' => 'Foo.Com',
                      'X-Forwarded-For' => '1.1.1.1 , 2.2.2.2, 3.3.3.3 , 4.4.4.4'
                    }
                    )
        end
      end
    end
    assert_no_issues
    assert_log_match /val of remote_addr.*4\.4\.4\.4/
  end

  def test_main_context_networks
    clipp(modhtp: true,
          config: CONFIG,
          default_site_config: make_site_config()
         ) do
      connection(remote_ip:"99.99.99.45") do |c|
        c.transaction() do |t|
          t.request(
                    method: 'GET',
                    uri: '/hello/world',
                    protocol: 'HTTP/1.0',
                    headers: {
                      'Host' => 'Foo.Com',
                      'X-Forwarded-For' => '1.1.1.1 , 2.2.2.2, 3.3.3.3 , 4.4.4.4'
                    }
                    )
        end
      end
    end
    assert_no_issues
    assert_log_match /val of remote_addr.*4\.4\.4\.4/
  end

  def test_subtractive_networks
    clipp(modhtp: true,
          config: CONFIG,
          default_site_config: make_site_config("-99.99.99.45")
         ) do
      connection(remote_ip:"99.99.99.45") do |c|
        c.transaction() do |t|
          t.request(
                    method: 'GET',
                    uri: '/hello/world',
                    protocol: 'HTTP/1.0',
                    headers: {
                      'Host' => 'Foo.Com',
                      'X-Forwarded-For' => '1.1.1.1 , 2.2.2.2, 3.3.3.3 , 4.4.4.4'
                    }
                    )
        end
      end
    end
    assert_no_issues
    assert_log_match /val of remote_addr.*99\.99\.99\.45/
  end

  def test_reset_networks
    clipp(modhtp: true,
          config: CONFIG,
          default_site_config: make_site_config("5.5.5.5")
         ) do
      connection(remote_ip:"99.99.99.45") do |c|
        c.transaction() do |t|
          t.request(
                    method: 'GET',
                    uri: '/hello/world',
                    protocol: 'HTTP/1.0',
                    headers: {
                      'Host' => 'Foo.Com',
                      'X-Forwarded-For' => '1.1.1.1 , 2.2.2.2, 3.3.3.3 , 4.4.4.4'
                    }
                    )
        end
      end
    end
    assert_no_issues
    assert_log_match /val of remote_addr.*99\.99\.99\.45/
  end
  def test_reset_networks_2
    clipp(modhtp: true,
          config: CONFIG,
          default_site_config: make_site_config("5.5.5.5")
         ) do
      connection(remote_ip:"5.5.5.5") do |c|
        c.transaction() do |t|
          t.request(
                    method: 'GET',
                    uri: '/hello/world',
                    protocol: 'HTTP/1.0',
                    headers: {
                      'Host' => 'Foo.Com',
                      'X-Forwarded-For' => '1.1.1.1 , 2.2.2.2, 3.3.3.3 , 4.4.4.4'
                    }
                    )
        end
      end
    end
    assert_no_issues
    assert_log_match /val of remote_addr.*4\.4\.4\.4/
  end
end
