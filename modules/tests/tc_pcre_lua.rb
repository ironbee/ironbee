#!/usr/bin/env ruby

# This test is from one of our developers using Predicate and Lua.
class TestPcreLua < Test::Unit::TestCase
  include CLIPPTest

  def test_rule_writer_dfa_bug_01
    clipp(
      predicate: true,
      modhtp: true,
      modules: %w{ lua pcre },
      default_site_config: "RuleEnable All",
      config: <<-EOS
        RuleEngineLogData event audit
        RuleEngineLogLevel info
        LuaInclude #{File.join(SRCDIR, 'test_rule_writer_dfa_bug_01.waggle')}
      EOS
    ) do
      transaction do |t|
        t.request(
          raw: "GET /zeusbox/?record&key=783074335349716c3161424b37363237483433314a466f6b6b64&referer=http%3A//www.bing.com/images/search%3Fq%3Dgemma+arterton%26view%3Ddetail%26id%3DC7D4B98212649CB15A71F9E830CE07BAE1757643%26first%3D1&resource=http%3A//www.zeusbox.com/view/gemma_arterton_1280_x_800_widescreen-1280x800.html&resource_title=Gemma%20Arterton%201280%20x%20800%20widescreen&resource_title_encoded=0&1347266315924&serve_js HTTP/1.1",
          headers: {
            'Host' => '127.0.0.1' ,
            'Connection' => 'close'
          }
        )
        t.response(
          raw: "HTTP/1.1 200 OK",
          headers: {
            'Host' => '127.0.0.1',
            'Content-Length' => 2 ,
            'Connection' => 'close'
          },
          body: 'az'
        )
      end
    end
    assert_log_no_match /Predicate operator failure/ms
  end
end
