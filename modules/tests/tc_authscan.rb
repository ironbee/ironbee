require 'openssl'

class TestAuthScan < CLIPPTest::TestCase
    include CLIPPTest

    def test_authscan_basic
        secret = 'mysecret'
        req    = 'GET / HTTP/1.1'
        host   = 'www.myhost.com'
        date   = generate_date
        hash   = generate_hash secret, req, host, date
        clipp(
            modhtp: true,
            modules: %w{ authscan },
            log_level: 'debug',
            config: """
                AuthScanSharedSecret #{secret}

                # The default
                # AuthScanRequestHeader

                # 10 seconds
                AuthScanGracePeriod 10
            """,
            default_site_config: ''
        ) do
            transaction do |t|
                t.request(
                    raw: req,
                    headers: {
                        'Host' => host,
                        'X-Auth-Scan' => "#{hash};date=#{date}"
                    }
                )
                t.response(raw: 'HTTP/1.1 200 OK')
            end
        end

        assert_no_issues
        assert_log_match hash
        assert_log_match /authscan.cpp.*Allowing Transaction/
    end

    def test_authscan_alternate_header
        secret = 'mysecret'
        req    = 'GET / HTTP/1.1'
        host   = 'www.myhost.com'
        date   = generate_date
        hash   = generate_hash secret, req, host, date
        clipp(
            modhtp: true,
            modules: %w{ authscan },
            log_level: 'debug',
            config: """
                AuthScanSharedSecret #{secret}

                AuthScanRequestHeader MyAuthHeader

                # 10 seconds
                AuthScanGracePeriod 10
            """,
            default_site_config: ''
        ) do
            transaction do |t|
                t.request(
                    raw: req,
                    headers: {
                        'Host' => host,
                        'MyAuthHeader' => "#{hash};date=#{date}"
                    }
                )
                t.response(raw: 'HTTP/1.1 200 OK')
            end
        end

        assert_no_issues
        assert_log_match hash
        assert_log_match /authscan.cpp.*Allowing Transaction/
    end

    def test_authscan_badsecret
        secret = 'mysecret'
        req    = 'GET / HTTP/1.1'
        host   = 'www.myhost.com'
        date   = generate_date
        hash   = generate_hash "wrong", req, host, date
        clipp(
            modhtp: true,
            modules: %w{ authscan },
            log_level: 'debug',
            config: """
                AuthScanSharedSecret #{secret}

                AuthScanRequestHeader MyAuthHeader

                # 10 seconds
                AuthScanGracePeriod 10
            """,
            default_site_config: ''
        ) do
            transaction do |t|
                t.request(
                    raw: req,
                    headers: {
                        'Host' => host,
                        'MyAuthHeader' => "#{hash};date=#{date}"
                    }
                )
                t.response(raw: 'HTTP/1.1 200 OK')
            end
        end

        assert_no_issues
        assert_log_match hash
        assert_log_match /Submitted hash .* does not equal computed hash .*. No action taken./
    end

    def test_authscan_badclock
        secret = 'mysecret'
        req    = 'GET / HTTP/1.1'
        host   = 'www.myhost.com'
        date   = generate_date(Time.new-11)
        hash   = generate_hash secret, req, host, date
        clipp(
            modhtp: true,
            modules: %w{ authscan },
            log_level: 'debug',
            config: """
                AuthScanSharedSecret #{secret}

                AuthScanRequestHeader MyAuthHeader

                # 10 seconds
                AuthScanGracePeriod 10
            """,
            default_site_config: ''
        ) do
            transaction do |t|
                t.request(
                    raw: req,
                    headers: {
                        'Host' => host,
                        'MyAuthHeader' => "#{hash};date=#{date}"
                    }
                )
                t.response(raw: 'HTTP/1.1 200 OK')
            end
        end

        assert_no_issues
        assert_log_match hash
        assert_log_match /Date stamp is outside of the allowable clock skew: \d+ seconds./
    end

    def test_authscan_badhost
        secret = 'mysecret'
        req    = 'GET / HTTP/1.1'
        host   = 'www.myhost.com'
        date   = generate_date
        hash   = generate_hash host, "wrong", host, date
        clipp(
            modhtp: true,
            modules: %w{ authscan },
            log_level: 'debug',
            config: """
                AuthScanSharedSecret #{secret}

                AuthScanRequestHeader MyAuthHeader

                # 10 seconds
                AuthScanGracePeriod 10
            """,
            default_site_config: ''
        ) do
            transaction do |t|
                t.request(
                    raw: req,
                    headers: {
                        'Host' => host,
                        'MyAuthHeader' => "#{hash};date=#{date}"
                    }
                )
                t.response(raw: 'HTTP/1.1 200 OK')
            end
        end

        assert_no_issues
        assert_log_match hash
        assert_log_match /Submitted hash .* does not equal computed hash .*. No action taken./
    end

    def test_authscan_badreq
        secret = 'mysecret'
        req    = 'GET / HTTP/1.1'
        host   = 'www.myhost.com'
        date   = generate_date
        hash   = generate_hash host, req, "wrong", date
        clipp(
            modhtp: true,
            modules: %w{ authscan },
            log_level: 'debug',
            config: """
                AuthScanSharedSecret #{secret}

                AuthScanRequestHeader MyAuthHeader

                # 10 seconds
                AuthScanGracePeriod 10
            """,
            default_site_config: ''
        ) do
            transaction do |t|
                t.request(
                    raw: req,
                    headers: {
                        'Host' => host,
                        'MyAuthHeader' => "#{hash};date=#{date}"
                    }
                )
                t.response(raw: 'HTTP/1.1 200 OK')
            end
        end

        assert_no_issues
        assert_log_match hash
        assert_log_match /Submitted hash .* does not equal computed hash .*. No action taken./
    end

    private

    def generate_date(t=Time.new)
        t.gmtime.strftime("%a, %d %b %Y %H:%M:%S GMT")
    end

    def generate_hash(secret, req, host, date)
        hmac = OpenSSL::HMAC.new(secret, OpenSSL::Digest.new('sha256'))
        hmac << req
        hmac << host
        hmac << date
        hmac.hexdigest
    end
end