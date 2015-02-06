
class TestTesting < CLIPPTest::TestCase
  include CLIPPTest
  def test_response_header_add
    clipp(
      modules: %w{ lua },
      lua_module: '''
        local m = ...

        m:request_started_state(function(tx)

          tx:addResponseHeader("A", "B")

          return 0
        end)

        return 0
      ''',
      config: '',
      default_site_config: ''
    ) do
      transaction do |t|
        t.request(raw: "GET /")
        t.response(raw: "HTTP/1.0 200 OK")
      end
    end

    assert_log_match /ALERT\s+.*dir=response\s+action=add\s+hdr=A\s+value=B/
  end

  def test_response_header_set
    clipp(
      modules: %w{ lua },
      lua_module: '''
        local m = ...

        m:request_started_state(function(tx)

          tx:setResponseHeader("A", "B")

          return 0
        end)

        return 0
      ''',
      config: '',
      default_site_config: ''
    ) do
      transaction do |t|
        t.request(raw: "GET /")
        t.response(raw: "HTTP/1.0 200 OK")
      end
    end

    assert_log_match /ALERT\s+.*dir=response\s+action=set\s+hdr=A\s+value=B/
  end

  def test_response_header_del
    clipp(
      modules: %w{ lua },
      lua_module: '''
        local m = ...

        m:request_started_state(function(tx)

          tx:delResponseHeader("A", "B")

          return 0
        end)

        return 0
      ''',
      config: '',
      default_site_config: ''
    ) do
      transaction do |t|
        t.request(raw: "GET /")
        t.response(raw: "HTTP/1.0 200 OK")
      end
    end

    assert_log_match /ALERT\s+.*dir=response\s+action=unset\s+hdr=A\s+value=B/
  end


  def test_request_header_add
    clipp(
      modules: %w{ lua },
      lua_module: '''
        local m = ...

        m:request_started_state(function(tx)

          tx:addRequestHeader("A", "B")

          return 0
        end)

        return 0
      ''',
      config: '',
      default_site_config: ''
    ) do
      transaction do |t|
        t.request(raw: "GET /")
        t.response(raw: "HTTP/1.0 200 OK")
      end
    end

    assert_log_match /ALERT\s+.*dir=request\s+action=add\s+hdr=A\s+value=B/
  end

  def test_request_header_set
    clipp(
      modules: %w{ lua },
      lua_module: '''
        local m = ...

        m:request_started_state(function(tx)

          tx:setRequestHeader("A", "B")

          return 0
        end)

        return 0
      ''',
      config: '',
      default_site_config: ''
    ) do
      transaction do |t|
        t.request(raw: "GET /")
        t.response(raw: "HTTP/1.0 200 OK")
      end
    end

    assert_log_match /ALERT\s+.*dir=request\s+action=set\s+hdr=A\s+value=B/
  end

  def test_request_header_del
    clipp(
      modules: %w{ lua },
      lua_module: '''
        local m = ...

        m:request_started_state(function(tx)

          tx:delRequestHeader("A", "B")

          return 0
        end)

        return 0
      ''',
      config: '',
      default_site_config: ''
    ) do
      transaction do |t|
        t.request(raw: "GET /")
        t.response(raw: "HTTP/1.0 200 OK")
      end
    end

    assert_log_match /ALERT\s+.*dir=request\s+action=unset\s+hdr=A\s+value=B/
  end
end