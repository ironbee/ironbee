class TestPredicate < CLIPPTest::TestCase
  include CLIPPTest

  def test_predicate_constant
    clipp(
      predicate: true,
      modules: ['constant'],
      config: "LoadModule \"#{TOP_BUILDDIR}/predicate/.libs/ibmod_predicate_constant.so\"",
      default_site_config: <<-EOS
        ConstantSet Foo Bar
        Action id:1 phase:REQUEST_HEADER clipp_announce:YES "predicate:(eq 'Bar' (constant 'Foo'))"
        Action id:2 phase:REQUEST_HEADER clipp_announce:NO "predicate:(eq 'Bar' (constant 'Baz'))"
      EOS
    ) do
      transaction {|t| t.request(raw: 'GET /')}
    end
    assert_no_issues
    assert_log_match /CLIPP ANNOUNCE: YES/
    assert_log_no_match /CLIPP ANNOUNCE: NO/
  end
end
