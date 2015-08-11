class TestResponse < CLIPPTest::TestCase
  include CLIPPTest

  def test_response_load
    clipp(
      modules: %w[ response ]
    ) do
      transaction do |t|
        t.request(raw:"GET /foo?1=foobar&2=---abc--- HTTP/1.0")
      end
    end

    assert_no_issues

  end

  def test_response_status
    clipp(
      modules: %w[ response ],
      log_level:'debug',
      default_site_config: '''
        Action id:1 phase:REQUEST response:200
      '''
    ) do
      transaction do |t|
        t.request(raw:"GET /foo?1=foobar&2=---abc--- HTTP/1.0")
      end
    end

    assert_log_match('clipp_error: 200')
    assert_log_match('Setting status to 200')
  end

  def test_response_status_and_file
    clipp(
      modules: %w[ response ],
      log_level:'debug',
      default_site_config: '''
        Action id:1 phase:REQUEST response:200,/my_file.txt
      '''
    ) do
      transaction do |t|
        t.request(raw:"GET /foo?1=foobar&2=---abc--- HTTP/1.0")
      end
    end

    assert_log_match('clipp_error: 200')
    assert_log_match('Setting status to 200')
    assert_log_match('Setting response file to /my_file.txt')
  end

  def test_response_1_header
    clipp(
      modules: %w[ response ],
      log_level:'debug',
      default_site_config: '''
        Action id:1 phase:REQUEST response:200,X-Header1:Value1,/my_file.txt
      '''
    ) do
      transaction do |t|
        t.request(raw:"GET /foo?1=foobar&2=---abc--- HTTP/1.0")
      end
    end

    assert_log_match('clipp_error: 200')
    assert_log_match('Setting status to 200')
    assert_log_match('Adding header X-Header1=Value1')
    assert_log_match('Setting response file to /my_file.txt')
  end

  def test_response_2_headers
    clipp(
      modules: %w[ response ],
      log_level:'debug',
      default_site_config: '''
        Action id:1 phase:REQUEST response:200,X-Header1:Value1,X-Header2:Value2,/my_file.txt
      '''
    ) do
      transaction do |t|
        t.request(raw:"GET /foo?1=foobar&2=---abc--- HTTP/1.0")
      end
    end

    assert_log_match('Setting status to 200')
    assert_log_match('Setting status to 200')
    assert_log_match('Adding header X-Header1=Value1')
    assert_log_match('Adding header X-Header2=Value2')
    assert_log_match('Setting response file to /my_file.txt')
  end
end