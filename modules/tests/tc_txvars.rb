class TxVars < CLIPPTest::TestCase
    include CLIPPTest

    def test_txvars
        clipp(
            modules: %w/ txvars txdump /,
            config: '''
                TxVars On
                TxDump TxFinished stderr +All
            ''',
            default_site_config: ''
        ) do
            transaction do |t|
                t.request(raw: 'GET / HTTP/1.1', headers: {'Host'=>'www.myhost.com'})
                t.response(raw: 'HTTP/1.1 200 OK')
            end
        end

        assert_no_issues
        assert_log_match "sensor_id"
        assert_log_match "site_name"
        assert_log_match "tx_id"
        assert_log_match "tx_start"
        assert_log_match "conn_id"
        assert_log_match "conn_start"
        assert_log_match "conn_tx_count"
        assert_log_match "site_id"
        assert_log_match "engine_id"
    end
end
