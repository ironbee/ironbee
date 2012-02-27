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

    virtual ib_status_t config(const string& configString) {
        string s = configString + "\n";
        return ib_cfgparser_ragel_parse_chunk(cfgparser,
                                              s.c_str(),
                                              s.length(),
                                              0);
    }
};

TEST_F(TestConfig, simpleparse) {
    ASSERT_IB_OK(config("DebugLogLevel 9"));
}

TEST_F(TestConfig, fail) {
    ASSERT_NE(IB_OK, config("blah blah"));
}
