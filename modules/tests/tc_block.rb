class TestBlock < CLIPPTest::TestCase
  include CLIPPTest

  def test_load
    clipp(
      modules: ['block']
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end
    assert_no_issues
  end

  def test_default
    clipp(
      modules: ['block'],
      default_site_config: "Action id:1 phase:REQUEST_HEADER setflag:blockingMode block:phase"
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end
    assert_log_match(/clipp_error: 403/)
  end

  def test_block_status
    clipp(
      modules: ['block'],
      config: 'BlockStatus 123',
      default_site_config: "Action id:1 phase:REQUEST_HEADER setflag:blockingMode block:phase"
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end
    assert_log_match(/clipp_error: 123/)
  end

  def test_block_method_status
    clipp(
      modules: ['block'],
      config: 'BlockMethod status',
      default_site_config: "Action id:1 phase:REQUEST_HEADER setflag:blockingMode block:phase"
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end
    assert_log_match(/clipp_error: 403/)
  end

  def test_block_method_close
    clipp(
      modules: ['block'],
      config: 'BlockMethod close',
      default_site_config: "Action id:1 phase:REQUEST_HEADER setflag:blockingMode block:phase"
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end
    assert_log_match(/clipp_close/)
  end

  def test_invalid_block_status
    clipp(
      modules: ['block'],
      config: 'BlockStatus helloworld',
      default_site_config: "Action id:1 phase:REQUEST_HEADER setflag:blockingMode block:phase"
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end
    assert_log_match(/Could not convert helloworld to integer./)
  end

  def test_invalid_block_method
    clipp(
      modules: ['block'],
      config: 'BlockMethod foobar',
      default_site_config: "Action id:1 phase:REQUEST_HEADER setflag:blockingMode block:phase"
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
      end
    end
    assert_log_match(/Invalid block method: foobar/)
  end

  def test_fallback
    clipp(
      modules: ['block'],
      default_site_config: "Action id:1 phase:RESPONSE setflag:blockingMode block:phase"
    ) do
      transaction do |t|
        t.request(raw: "GET /foo HTTP/1.1")
        t.response(raw: "100 OK")
      end
    end
    assert_log_match(/clipp_close/)
  end
end
