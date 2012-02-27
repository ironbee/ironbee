#include <gtest/gtest.h>
#include <config-parser.h>
#include <ironbee/config.h>
#include "base_fixture.h"

#include <string>

using std::string;

class TestConfig : public BaseFixture {
    private:

    ib_cfgparser_t *cfgparser;

    public:

    virtual void SetUp() {
        BaseFixture::SetUp();
        ib_cfgparser_create(&cfgparser, ib_engine);
    }

    virtual void TearDown() {
        BaseFixture::TearDown();
    }

    virtual ib_status_t config(const string& configString, int isEnd=0) {
        string s = configString + "\n";
        return ib_cfgparser_ragel_parse_chunk(cfgparser,
                                              s.c_str(),
                                              s.length(),
                                              isEnd);
    }
};

TEST_F(TestConfig, simpleparse) {
    ASSERT_IB_OK(config("DebugLogLevel 9"));
}

TEST_F(TestConfig, valid_module) {
    ASSERT_IB_OK(config("ModuleBasePath "IB_XSTRINGIFY(MODULE_BASE_PATH)));
    ASSERT_IB_OK(config("LoadModule ibmod_htp.so"));
    ASSERT_IB_OK(config("Set parser htp", 1));
}

TEST_F(TestConfig, false_directive) {
    ASSERT_NE(IB_OK, config("blah blah"));
}

TEST_F(TestConfig, incomplete_site_block) {
    ASSERT_NE(IB_OK, config("<Site default>\n"
                            "Hostname *\n"
                            "SiteId AAAABBBB-1111-2222-3333-000000000000\n"
                            "</Site", 1));
    ASSERT_NE(IB_OK, config("<Site defau", 1));
    ASSERT_NE(IB_OK, config("<Site default>\n", 1));
    ASSERT_NE(IB_OK, config("<Site default>\n"
                            "Hostname *\n"
                            "SiteId AAAABBBB-1111-2222-3333-000000000000\n",
                            1));
}

TEST_F(TestConfig, unloadable_module) {
    ASSERT_NE(IB_OK, config("LoadModule doesnt_exist.so", 1));
}
