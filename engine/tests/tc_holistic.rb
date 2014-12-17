CONFIG_AUTO = File.expand_path(
  ENV['abs_top_builddir'] + "/ironbee_config_auto_gen.h"
)
$thread_sanitizer_workaround = false
File.open(CONFIG_AUTO, 'r') do |fp|
  if fp.find {|x| x =~ /define IB_THREAD_SANITIZER_WORKAROUND 1/}
    $thread_sanitizer_workaround = true
  end
end

# Very dumb random HTTP traffic generator.
module CLIPPTestHolistic

  def self.r(min, max)
    rand(max - min + 1) + min
  end

  def self.ra(array)
    array[r(0, array.length - 1)]
  end

  def self.generate_body(min_length = 0, max_length = 1024)
    length = r(min_length, max_length)
    (1..length).collect {"%c" % [r(32, 126)]}.join('')
  end

  def self.generate_id
    (1..20).collect {"%c" % [r(65, 90)]}.join('')
  end

  def self.generate_uri
    '/' + (1..r(0, 10)).collect do
      (1..r(1, 10)).collect {"%c" % [r(65, 90)]}.join('')
    end.join('/')
  end

  METHODS = ['OPTIONS', 'GET', 'HEAD', 'POST', 'PUT', 'DELETE', 'TRACE', 'CONNECT']
  VERSIONS = ['HTTP/1.0', 'HTTP/1.1']

  def self.generate_tx_request
    [ra(METHODS), generate_uri(), ra(VERSIONS)]
  end

  def self.hash_to_headers(which, h)
    {
      'which' => which,
      'header_event' => {
        'header' => h.collect do |k,v|
          {
            'name'  => k,
            'value' => v
          }
        end
      }
    }
  end

  RESPONSES = [
    ['100', 'Continue'],
    ['101', 'Switching Protocols'],
    ['200', 'OK'],
    ['201', 'Created'],
    ['202', 'Accepted'],
    ['203', 'Non-Authoritative Information'],
    ['204', 'No Content'],
    ['205', 'Reset Content'],
    ['206', 'Partial Content'],
    ['300', 'Multiple Choices'],
    ['301', 'Moved Permanently'],
    ['302', 'Found'],
    ['303', 'See Other'],
    ['304', 'Not Modified'],
    ['305', 'Use Proxy'],
    ['306', '(Unused)'],
    ['307', 'Temporary Redirect'],
    ['400', 'Bad Request'],
    ['401', 'Unauthorized'],
    ['402', 'Payment Required'],
    ['403', 'Forbidden'],
    ['404', 'Not Found'],
    ['405', 'Method Not Allowed'],
    ['406', 'Not Acceptable'],
    ['407', 'Proxy Authentication Required'],
    ['408', 'Request Timeout'],
    ['409', 'Conflict'],
    ['410', 'Gone'],
    ['411', 'Length Required'],
    ['412', 'Precondition Failed'],
    ['413', 'Request Entity Too Large'],
    ['414', 'Request-URI Too Long'],
    ['415', 'Unsupported Media Type'],
    ['416', 'Requested Range Not Satisfiable'],
    ['417', 'Expectation Failed'],
    ['500', 'Internal Server Error'],
    ['501', 'Not Implemented'],
    ['502', 'Bad Gateway'],
    ['503', 'Service Unavailable'],
    ['504', 'Gateway Timeout'],
    ['505', 'HTTP Version Not Supported']
  ]

  def self.generate_tx_response
    ra(RESPONSES)
  end

  def self.generate_tx_events
    result = []

    # REQUEST
    method, uri, protocol = generate_tx_request()
    body = generate_body()
    result << {
      'which'    => 5, # REQUEST_STARTED
      'request_event' => {
        'raw'      => [method, uri, protocol].join(' '),
        'method'   => method,
        'uri'      => uri,
        'protocol' => protocol
      }
    }
    result << hash_to_headers(6,
      'Content-Length'   => body.length.to_s,
      'Host'             => 'nobody',
      'Content-Encoding' => 'none',
      'Content-Type'     => 'random',
      'Referer'          => 'clipp random',
      'Cookie'           => generate_body(1, 100),
      'User-Agent'       => 'clipp random data'
    )
    result << {
      'which' => 7 # REQUEST_HEADER_FINISHED
    }
    result << {
      'which' => 8, # REQUEST_BODY
      'data_event' => {
        'data'  => body
      }
    }
    result << {
      'which' => 9 # REQUEST_FINISHED
    }

    # RESPONSE
    status, message = generate_tx_response()
    result << {
      'which'    => 10, # RESPONSE_STARTED
      'response_event' => {
        'raw'      => [protocol, status, message].join(' '),
        'protocol' => protocol,
        'status'   => status,
        'message'  => message
      }
    }
    body = generate_body()
    result << hash_to_headers(11,
      'Content-Length' => body.length.to_s,
      'Content-Type'   => 'text/random'
    )
    result << {
      'which' => 12 # RESPONSE_HEADER_FINISHED
    }
    result << {
      'which' => 13, # RESPONSE_BODY
      'data_event' => {
        'data'  => body
      }
    }
    result << {
      'which' => 14 # RESPONSE_FINISHED
    }
    result
  end

  def self.generate_ip
    (1..4).collect {r(1, 254).to_s}.join('.')
  end

  def self.generate_port
    r(1, 32767)
  end

  def self.generate_http_connection
    {
      'id' => generate_id(),
      'connection' => {
        'pre_transaction_event' => [
          {
            'which' => 1,
            'connection_event' => {
              'local_ip'    => generate_ip(),
              'local_port'  => generate_port(),
              'remote_ip'   => generate_ip(),
              'remote_port' => 80
            }
          }
        ],
        'transaction' => [
          {
            'event' => generate_tx_events()
          }
        ],
        'post_transaction_event' => [
           {
             'which' => 4
           }
        ]
      }
    }
  end

  def self.generate_http_data(n = 1000)
    (1..n).collect {generate_http_connection()}
  end

end

class TestHolistic < CLIPPTest::TestCase

  parallelize_me!

  include CLIPPTest

  def self.generate_input
    n = 1000
    puts
    puts "Generating #{n} samples request/response pairs."
    now = Time.now
    input_hashes = CLIPPTestHolistic::generate_http_data()
    to = File.join(BUILDDIR, "clipp_test_#{rand(10000)}.pb")
    File.open(to, 'w') do |fp|
      input_hashes.each do |h|
        fp.print IronBee::CLIPP::HashToPB::hash_to_pb(h)
      end
    end
    puts "Finished in #{Time.now-now} seconds"
    "pb:#{to} @aggregate:poisson:5"
  end
  INPUT = generate_input()

  def test_single_threaded
    clipp(
      :input => INPUT
    )
    assert_no_issues
  end

  def test_multi_threaded
    if $thread_sanitizer_workaround
      puts "Skipping due to thread sanitizer."
    else
      clipp(
        :input => INPUT,
        :consumer => 'ironbee_threaded:IRONBEE_CONFIG:4'
      )
      assert_no_issues
    end
  end
end