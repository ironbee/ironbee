/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

#include <gtest/gtest.h>
#include <config-parser.h>
#include <ironbee/config.h>
#include "base_fixture.h"

#include <string>

using std::string;

class TestConfig : public BaseFixture
{
    private:

    ib_cfgparser_t *cfgparser;

    public:

    virtual void SetUp()
    {
        ib_status_t rc;
        BaseFixture::SetUp();
        rc = ib_cfgparser_create(&cfgparser, ib_engine);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to create parser");
        }
        rc = ib_engine_config_started(ib_engine, cfgparser);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to start configuration");
        }
    }

    virtual void TearDown()
    {
        ib_status_t rc;

        rc = ib_engine_config_finished(ib_engine);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to finish configuration");
        }

        rc = ib_cfgparser_destroy(cfgparser);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to destroy parser");
        }
        BaseFixture::TearDown();
    }

    virtual ib_status_t config(const string& configString, int isEnd=0)
    {
        return config(configString, "test.conf", 1, isEnd);
    }

    virtual ib_status_t config(const string& configString,
                               const char *file, ib_num_t lineno,
                               int isEnd=0)
    {
        string s = configString + "\n";
        ib_status_t rc;
        rc = ib_cfgparser_ragel_parse_chunk(cfgparser,
                                            s.c_str(),
                                            s.length(),
                                            isEnd);
        if (rc != IB_OK) {
            return rc;
        }

        if (isEnd) {
            rc = ib_cfgparser_apply(cfgparser, cfgparser->ib);
        }

        return rc;
    }
};

TEST_F(TestConfig, simpleparse)
{
    ASSERT_IB_OK(config("LogLevel 9"));
}

TEST_F(TestConfig, valid_module)
{
    ASSERT_IB_OK(config("ModuleBasePath "IB_XSTRINGIFY(MODULE_BASE_PATH)));
    ASSERT_IB_OK(config("LoadModule ibmod_htp.so"));
}

TEST_F(TestConfig, false_directive)
{
    ASSERT_NE(IB_OK, config("blah blah", 1));
}

TEST_F(TestConfig, incomplete_site_block)
{
    ASSERT_NE(IB_OK, config("<Site default>\n"
                            "Hostname *\n"
                            "SiteId AAAABBBB-1111-2222-3333-000000000000\n"
                            "</Site", 1));
    ASSERT_NE(IB_OK, config("<Site defau",  1));
    ASSERT_NE(IB_OK, config("<Site default>\n", 1));
    ASSERT_NE(IB_OK, config("<Site default>\n"
                            "Hostname *\n"
                            "SiteId AAAABBBB-1111-2222-3333-000000000000\n",
                            1));
}

TEST_F(TestConfig, unloadable_module)
{
    ASSERT_NE(IB_OK, config("LoadModule doesnt_exist.so", 1));
}
