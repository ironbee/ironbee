class TestAbort < CLIPPTest::TestCase
    include CLIPPTest

    def test_load
        clipp(
            modules: %w{ testops abort },
            default_site_config: ''
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n")
            end
        end

        assert_no_issues
    end

    def test_no_abort
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                Rule x @false "" id:1 phase:REQUEST_HEADER
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n")
            end
        end

        assert_no_issues
    end

    def test_abort
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                Rule x @true "" id:1 phase:REQUEST_HEADER "abort:FalseAbort"
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n")
            end
        end

        assert_no_clean_exit
    end

    def test_abortif_optrue_true
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                Rule x @true "" id:1 phase:REQUEST_HEADER "abortIf:optrue:Operator was true."
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n")
            end
        end

        assert_no_clean_exit
    end

    def test_abortif_optrue_false
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                Rule x @false "" id:1 phase:REQUEST_HEADER "abortIf:optrue:Operator was true."
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n")
            end
        end

        assert_no_issues
    end

    def test_abortif_opfalse_true
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                Rule x @true "" id:1 phase:REQUEST_HEADER "abortIf:opfalse:Operator was false."
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n")
            end
        end

        assert_no_issues
    end

    def test_abortif_opfalse_false
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                Rule x @false "" id:1 phase:REQUEST_HEADER "abortIf:opfalse:Operator was false."
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n")
            end
        end

        assert_no_clean_exit
    end

    # Abort when an operator returns IB_OK. That is, does not fail.
    def test_abortif_opok_abort
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                Rule x @true "" id:1 phase:REQUEST_HEADER "abortIf:opok:Operator returned IB_OK."
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n")
            end
        end

        assert_no_clean_exit
    end

    # Do not abort if the operator returns an error of some sort.
    def test_abortif_opok_noabort
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                InitVar someText "This is a string, not an integer."
                Rule someText @gt 0 id:1 phase:REQUEST_HEADER "abortIf:opok:Operator returned IB_OK."
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n\n")
            end
        end

        assert_clean_exit
    end

    def test_abortif_opfail_abort
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                InitVar someText "This is a string, not an integer."
                Rule someText @gt 0 id:1 phase:REQUEST_HEADER "abortIf:opfail:Operator returned IB_EINVAL."
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n\n")
            end
        end

        assert_no_clean_exit
    end

    def test_abortif_opfail_noabort
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                Rule x @true "" id:1 phase:REQUEST_HEADER "abortIf:opfail:Operator returned IB_EINVAL."
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n\n")
            end
        end

        assert_no_issues
    end

    def test_abortif_actionok_abort
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                Rule x @true "" id:1 phase:REQUEST_HEADER "block:hi" "abortIf:actok:Action ok."
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n\n")
            end
        end

        assert_no_clean_exit
    end

    def test_abortif_actionok_noabort
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                Rule x @true "" id:1 phase:REQUEST_HEADER "setvar:x=%{y}" "abortIf:actok:Action ok."
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n\n")
            end
        end

        assert_clean_exit
    end

    def test_abortif_actionfail_abort
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                Rule x @true "" id:1 phase:REQUEST_HEADER "setvar:x=%{y}" "abortIf:actfail:Action not ok."
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n\n")
            end
        end

        assert_no_clean_exit
    end

    def test_abortif_actionfail_noabort
        clipp(
            modules: %w{ testops abort },
            default_site_config: <<-EOS
                Rule x @true "" id:1 phase:REQUEST_HEADER "block" "abortIf:actfail:Action not ok."
            EOS
        ) do
            transaction do |t|
                t.request(raw: "GET / HTTP/1.1\n\n")
            end
        end

        assert_clean_exit
    end

end