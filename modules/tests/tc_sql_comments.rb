class TestSql < CLIPPTest::TestCase
  include CLIPPTest

  [
    # Test name, Transform, Input, Expected
    [ "replace_pg_comments()" , "a/* HI! */b"        , "ab"             ],
    [ "replace_pg_comments()" , "a/* /* HI! */b"     , "a/* /* HI! */b" ],
    [ "replace_pg_comments()" , "a/* HI! */ */b"     , "a/* HI! */ */b" ],
    [ "replace_pg_comments()" , "a/* /* HI! */ */b"  , "ab"             ],
    [ "replace_pg_comments(-)", "a/* HI! */b"        , "a-b"            ],
    [ "replace_pg_comments(-)", "a/* /* HI! */b"     , "a/* /* HI! */b" ],
    [ "replace_pg_comments(-)", "a/* HI! */ */b"     , "a/* HI! */ */b" ],
    [ "replace_pg_comments(-)", "a/* /* HI! */ */b"  , "a-b"            ],
  ].each_with_index do |test_case, i|
    transform, input, expected = test_case

    define_method "test_sql_pgsql_clean_comments_#{i}".to_sym do
      clipp(
        modules: %w[ sql_comments ],
        config: """
          InitVar SQL \"#{input}\"
        """,
        default_site_config: """
          Rule SQL.#{transform} @clipp_print expected id:act1 rev:1 phase:REQUEST
        """
      ) do
        transaction do |t|
          t.request(raw: 'GET / HTTP/1.0')
          t.response(raw:"HTTP/1.0 200 OK")
        end
      end

      assert_no_issues
      assert_log_match "clipp_print [expected]: #{expected}"
    end
  end

  [
    [ "replace_mysql_comments()" , "a-- hi\nb"            , "a\nb"              ],
    [ "replace_mysql_comments()" , "a# hi\nb"            , "a\nb"              ],
    [ "replace_mysql_comments()" , "a /* /* */ b"         , "a  b"              ],
    [ "replace_mysql_comments()" , "a /*! c */ b"         , "a /*! c */ b"      ],
  ].each_with_index do |test_case, i|
    transform, input, expected = test_case

    define_method "test_sql_mysql_clean_comments_#{i}".to_sym do
      clipp(
        modules: %w[ sql_comments ],
        config: """
          InitVar SQL \"#{input}\"
        """,
        default_site_config: """
          Rule SQL.#{transform} @clipp_print expected id:act1 rev:1 phase:REQUEST
        """
      ) do
        transaction do |t|
          t.request(raw: 'GET / HTTP/1.0')
          t.response(raw:"HTTP/1.0 200 OK")
        end
      end

      assert_no_issues
      assert_log_match "clipp_print [expected]: #{expected}"
    end
  end

  [
    [ "replace_oracle_comments()" , "a-- hi\nb"            , "a\nb"              ],
    [ "replace_oracle_comments()" , "a# hi\nb"            , "a\nb"              ],
    [ "replace_oracle_comments()" , "a /* /* */ b"         , "a  b"              ],
  ].each_with_index do |test_case, i|
    transform, input, expected = test_case

    define_method "test_sql_oracle_clean_comments_#{i}".to_sym do
      clipp(
        modules: %w[ sql_comments ],
        config: """
          InitVar SQL \"#{input}\"
        """,
        default_site_config: """
          Rule SQL.#{transform} @clipp_print expected id:act1 rev:1 phase:REQUEST
        """
      ) do
        transaction do |t|
          t.request(raw: 'GET / HTTP/1.0')
          t.response(raw:"HTTP/1.0 200 OK")
        end
      end

      assert_no_issues
      assert_log_match "clipp_print [expected]: #{expected}"
    end
  end

  [
    [ "normalize_sql_comments()" , "a-- hi\nb"    , "a-- hi\nb"],
    [ "normalize_sql_comments()" , "a# hi\nb"     , "a# hi\nb"],
    [ "normalize_sql_comments()" , "a/* /* */b" , "ab"],
    [ "normalize_sql_comments()" , "a/*! hi */b" , "a hi b"],
  ].each_with_index do |test_case, i|
    transform, input, expected = test_case

    define_method "test_sql_normalize_comments_#{i}".to_sym do
      clipp(
        modules: %w[ sql_comments ],
        config: """
          InitVar SQL \"#{input}\"
        """,
        default_site_config: """
          Rule SQL.#{transform} @clipp_print expected id:act1 rev:1 phase:REQUEST
        """
      ) do
        transaction do |t|
          t.request(raw: 'GET / HTTP/1.0')
          t.response(raw:"HTTP/1.0 200 OK")
        end
      end

      assert_no_issues
      assert_log_match "clipp_print [expected]: #{expected}"
    end
  end
end
