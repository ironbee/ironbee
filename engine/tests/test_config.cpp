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

/**
 * @file
 * @brief IronBee --- Test configuration code.
 */

#include <gtest/gtest.h>
#include <config-parser.h>
#include <ironbee/config.h>
#include <ironbee/mm_mpool_lite.h>
#include "base_fixture.h"
#include "mock_module.h"

#include <string>
#include <utility>

using std::string;

/////////////////////////////// Fixture ///////////////////////////////

/**
 * Base for configuration tests.
 *
 * It provides a ib_cfgparser_t and a way to pass strings into it.
 *
 * The ib_cfgparser_t requires a properly setup ib_engine_t so,
 * we require the services of BaseFixture.
 */
class TestConfig : public BaseFixture
{
    private:

    ib_cfgparser_t *cfgparser;

    public:

    /**
     * Return the member parser pointer.
     */
    ib_cfgparser_t *GetParser() const
    {
        return cfgparser;
    }

    /**
     * Return the root node of the parser.
     *
     * This is a convenience function to void calling GetParser()->root a lot.
     */
    const ib_cfgparser_node_t *GetParseTree() const
    {
        return cfgparser->root;
    }

    /**
     * - Call BaseFixture::SetUp()
     * - Create ib_cfgparser_t.
     * - Signal the engine that the configuration is started.
     */
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

    /**
     * - Call ib_engine_config_finished.
     * - Destroy the parser.
     * - Call BaseFixture::TearDown()
     */
    virtual void TearDown()
    {
        ib_status_t rc;

        rc = ib_cfgparser_destroy(cfgparser);
        if (rc != IB_OK) {
            throw std::runtime_error("Failed to destroy parser");
        }
        BaseFixture::TearDown();
    }

    /**
     * Push a configuration string through the parser.
     *
     * @param[in] configString The string to pass in.
     * @param[in] isEnd True if we are done configuring for this test.
     *            This is forwarded to Ragel so it knows how to parse.
     *            When this is true (non-zero) ib_cfgparser_apply()
     *            is called.
     */
    virtual ib_status_t config(const string& configString, int isEnd=0)
    {
        ib_status_t rc;

        rc = ib_cfgparser_ragel_parse_chunk(cfgparser,
                                            configString.c_str(),
                                            configString.length(),
                                            isEnd);
        ib_log_info(
            ib_engine,
            "Done with configuration: %s",
            ib_status_to_string(rc));
        if (rc != IB_OK) {
            return rc;
        }

        if (isEnd) {
            ib_log_info(ib_engine, "Applying configuration.");

            rc = ib_cfgparser_apply(cfgparser, cfgparser->ib);

            ib_log_info(
                ib_engine,
                "Done with configuration application: %s",
                ib_status_to_string(rc));
        }

        return rc;
    }

    /**
     * Parse the given file and apply the configuration.
     *
     * @param[in] file The file name.
     *
     * @returns return code of parse function.
     */
    virtual ib_status_t configFile(const string& file) {
        ib_status_t rc;

        rc = ib_cfgparser_parse(cfgparser, file.c_str());

        ib_log_info(
            ib_engine,
            "Done with configuration: %s",
            ib_status_to_string(rc));

        return rc;
    }
};

/////////////////////////////// Passing Parses ///////////////////////////////

class PassingParseTest :
   public TestConfig,
   public ::testing::WithParamInterface<const char*>
{ };

TEST_P(PassingParseTest, SuccessConfig) {
    ASSERT_EQ(IB_OK, config(GetParam(), 1));
}

INSTANTIATE_TEST_CASE_P(
    SimpleConfig,
    PassingParseTest,
    ::testing::Values(
        "",
        "\n",
        "\r\n",
        "LogLevel 9",
        "LogLevel 9\n",
        "LogLevel 9\r\n",

        "ModuleBasePath " IB_XSTRINGIFY(MODULE_BASE_PATH) "\n"
        "LoadModule ibmod_htp.so",

        "IncludeIfExists Missing.conf"
    ));

///////////////////////////// Passing File Parses /////////////////////////////
class PassingFileParseTest :
   public TestConfig,
   public ::testing::WithParamInterface<const char*>
{ };

TEST_P(PassingFileParseTest, SuccessConfig) {
    ASSERT_EQ(IB_OK, configFile(GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    SimpleConfig,
    PassingFileParseTest,
    ::testing::Values(
        "Huge.config"
    ));

/////////////////////////////// Failing Parses ///////////////////////////////

class FailingParseTest :
   public TestConfig,
   public ::testing::WithParamInterface<const char*>
{ };

TEST_P(FailingParseTest, FailConfig) {
    ASSERT_NE(IB_OK, config(GetParam(), 1));
}

INSTANTIATE_TEST_CASE_P(
    SimpleConfigErrors,
    FailingParseTest,
    ::testing::Values(
    "blah blah",
    "blah blah\n",
    "blah blah\r\n",
    "LoadModule doesnt_exist.so",
    "LoadModule doesnt_exist.so\n",
    "LoadModule doesnt_exist.so\r\n",
    "Include Missing.config",
    "Include Missing.config\n",
    "Include Missing.config\r\n",
    "LogLevel TRACE\nLSProfile foo site=*@request_line_length=>numeric_int\n"
    ));

INSTANTIATE_TEST_CASE_P(
    IncompleteSiteBlock,
    FailingParseTest,
    ::testing::Values(
        "<Site default>\n"
        "   Hostname *\n"
        "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
        "</Site",

        "<Site defau",

        "<Site default>\n",

        "<Site default>\n"
        "   Hostname *\n"
        "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
));

/////////////////////////////// ParseTreeTests ///////////////////////////////

class ParseTreeTest :
   public TestConfig,
   public ::testing::WithParamInterface<const char*>
{
    public:
    ib_status_t setup_rc;

    virtual void SetUp()
    {
        TestConfig::SetUp();

        setup_rc = mock_module_register(ib_engine);
        if (setup_rc != IB_OK) {
            return;
        }

        setup_rc = config(GetParam(), 1);
    }

};

TEST_P(ParseTreeTest, IB_OK) {
    ASSERT_EQ(IB_OK, setup_rc);
}

TEST_P(ParseTreeTest, RootNodeIsRoot) {
    ASSERT_EQ(IB_CFGPARSER_NODE_ROOT, GetParseTree()->type);
    ASSERT_EQ(NULL, GetParseTree()->parent);
}

/* Did the configuration get applied to our mock module? */
TEST_P(ParseTreeTest, ConfigurationApplied) {
    ib_module_t *module;
    ib_context_t *ctx = ib_context_main(ib_engine);
    mock_module_conf_t *conf;

    const ib_list_node_t *node;

    ASSERT_EQ(
        IB_OK,
        ib_engine_module_get(ib_engine, mock_module_name(), &module));

    ASSERT_EQ(IB_OK, ib_context_module_config(ctx, module, &conf));

    ASSERT_FALSE(NULL == module->dm_init);
    ASSERT_FALSE(NULL == conf);

    ASSERT_STREQ("value1", conf->param1_p1);
    ASSERT_STREQ("value1", conf->param2_p1);
    ASSERT_STREQ("value2", conf->param2_p2);
    ASSERT_TRUE(0 == ~(conf->opflags_mask));
    ASSERT_TRUE(1 == conf->opflags_val);
    ASSERT_STREQ("MyParam1", conf->sblk1_p1);
    ASSERT_TRUE(conf->onoff_onoff);
    ASSERT_TRUE(conf->blkend_called);


    node = ib_list_first_const(conf->list_params);
    ASSERT_STREQ(
        "value1",
        reinterpret_cast<const char *>(ib_list_node_data_const(node)));
    node = ib_list_node_next_const(node);
    ASSERT_STREQ(
        "value2",
        reinterpret_cast<const char *>(ib_list_node_data_const(node)));
    node = ib_list_node_next_const(node);
    ASSERT_STREQ(
        "value3",
        reinterpret_cast<const char *>(ib_list_node_data_const(node)));
    node = ib_list_node_next_const(node);
    ASSERT_TRUE(NULL == node);
}

INSTANTIATE_TEST_CASE_P(
    SimpleConfig,
    ParseTreeTest,
    ::testing::Values(
    "LogLevel TRACE\n"
    "Include ParseTreeTest.config\n"
    //"IncludeIfExists ParseTreeTest.config\n"
    //"IncludeIfExists Missing.config\n"
    "<Site site1>\n"
    "   Param1 wrong_value1\n"
    "</Site>\n"

    "Param1  value1\n"
    "OnOff   on\n"
    "Param2  value1 value2\n"
    "List    value1 value2 value3\n"
    "OpFlags Flag1\n"
    "<Sblk1 MyParam1>\n"
    "</Sblk1>\n",

    "LogLevel TRACE\n"
    "Param1  value1\n"
    "OnOff   on\n"
    "Param2  value1 value2\n"
    "List    value1 \\\nvalue2 \\\nvalue3\n"
    "OpFlags Flag1\n"
    "<Sblk1 MyParam1>\n"
    "</Sblk1>\n"
));

/////////////////////////////// Buffer Span ///////////////////////////////

class BufferSpanTest :
   public TestConfig,
   public ::testing::WithParamInterface<std::pair<const char*, const char *> >
{
    public:
    ib_status_t setup_rc;

    virtual void SetUp()
    {
        TestConfig::SetUp();

        setup_rc = mock_module_register(ib_engine);
        if (setup_rc != IB_OK) {
            return;
        }

        setup_rc = config("LogLevel TRACE\n", 0);
        if (setup_rc != IB_OK) {
            return;
        }

        setup_rc = config(GetParam().first, 0);
        if (setup_rc != IB_OK) {
            return;
        }

        setup_rc = config(GetParam().second, 1);
    }

};

TEST_P(BufferSpanTest, IB_OK) {
    ASSERT_EQ(IB_OK, setup_rc);
}

/* Did the configuration get applied to our mock module? */
TEST_P(BufferSpanTest, ConfigurationApplied) {
    ib_module_t *module;
    ib_context_t *ctx = ib_context_main(ib_engine);
    mock_module_conf_t *conf;

    const ib_list_node_t *node;

    ASSERT_EQ(
        IB_OK,
        ib_engine_module_get(ib_engine, mock_module_name(), &module));

    ASSERT_EQ(IB_OK, ib_context_module_config(ctx, module, &conf));

    ASSERT_FALSE(NULL == module->dm_init);
    ASSERT_FALSE(NULL == conf);

    ASSERT_STREQ("value1", conf->param2_p1);
    ASSERT_STREQ("value2", conf->param2_p2);

    node = ib_list_first_const(conf->list_params);
    ASSERT_STREQ(
        "value1",
        reinterpret_cast<const char *>(ib_list_node_data_const(node)));
    node = ib_list_node_next_const(node);
    ASSERT_STREQ(
        "value2",
        reinterpret_cast<const char *>(ib_list_node_data_const(node)));
    node = ib_list_node_next_const(node);
    ASSERT_STREQ(
        "value3",
        reinterpret_cast<const char *>(ib_list_node_data_const(node)));
    node = ib_list_node_next_const(node);
    ASSERT_TRUE(NULL == node);
}

INSTANTIATE_TEST_CASE_P(
    SpanTests,
    BufferSpanTest,
    ::testing::Values(
    std::make_pair("Param2 value1 value2\n", "List value1 value2 value3\n"),
    std::make_pair("Param2 val", "ue1 value2\nList value1 value2 value3\n"),
    std::make_pair("Param2 value1 valu", "e2\nList value1 value2 value3\n"),
    std::make_pair("Param2 value1 value2\nL", "ist value1 value2 value3\n"),
    std::make_pair("Param2 value1 value2\nList ", "value1 value2 value3\n"),
    std::make_pair("Param2 value1 value2\nList v", "alue1 value2 value3\n"),
    std::make_pair("Param2 value1 value2\nList value1 valu", "e2 value3\n"),
    std::make_pair("Param2 value1 value2\nList value1 value2", " value3\n"),
    std::make_pair("Param2 value1 value2\nList value1 value2 ", "value3\n"),
    std::make_pair("Param2 value1 value2\nList value1 value2 val", "ue3\n"),
    std::make_pair("Param2 value1 value2\nList value1 value2 value3", "\n")
));

/////////////////////////////// Quoted String /////////////////////////////

class QuotedStrTest :
   public TestConfig,
   public ::testing::WithParamInterface<std::pair<const char*, const char *> >
{
    public:
    ib_status_t setup_rc;

    virtual void SetUp()
    {
        TestConfig::SetUp();

        setup_rc = mock_module_register(ib_engine);
        if (setup_rc != IB_OK) {
            return;
        }

        setup_rc = config("LogLevel TRACE\n", 0);
        if (setup_rc != IB_OK) {
            return;
        }

        setup_rc = config(GetParam().first, 0);
        if (setup_rc != IB_OK) {
            return;
        }

        setup_rc = config(GetParam().second, 1);
    }

};

TEST_P(QuotedStrTest, IB_OK) {
    ASSERT_EQ(IB_OK, setup_rc);
}

/* Did the configuration get applied to our mock module? */
TEST_P(QuotedStrTest, ConfigurationApplied) {
    ib_module_t *module;
    ib_context_t *ctx = ib_context_main(ib_engine);
    mock_module_conf_t *conf;

    const ib_list_node_t *node;

    ASSERT_EQ(
        IB_OK,
        ib_engine_module_get(ib_engine, mock_module_name(), &module));

    ASSERT_EQ(IB_OK, ib_context_module_config(ctx, module, &conf));

    ASSERT_FALSE(NULL == module->dm_init);
    ASSERT_FALSE(NULL == conf);

    ASSERT_STREQ("value1\\b", conf->param2_p1);
    ASSERT_STREQ("value2\\b", conf->param2_p2);

    node = ib_list_first_const(conf->list_params);
    ASSERT_STREQ(
        "value1\\b",
        reinterpret_cast<const char *>(ib_list_node_data_const(node)));
    node = ib_list_node_next_const(node);
    ASSERT_STREQ(
        "value2\\b",
        reinterpret_cast<const char *>(ib_list_node_data_const(node)));
    node = ib_list_node_next_const(node);
    ASSERT_STREQ(
        "value3\\b",
        reinterpret_cast<const char *>(ib_list_node_data_const(node)));
    node = ib_list_node_next_const(node);
    ASSERT_TRUE(NULL == node);
}

INSTANTIATE_TEST_CASE_P(
    SpanTests,
    QuotedStrTest,
    ::testing::Values(
    std::make_pair("Param2 value1\\b value2\\b\n", "List value1\\b value2\\b value3\\b\n"),
    std::make_pair("Param2 \"value1\\b\" \"value2\\b\"\n", "List \"value1\\b\" \"value2\\b\" \"value3\\b\"\n"),
    std::make_pair("Param2 value1\\b \"value2\\b\"\n", "List value1\\b \"value2\\b\" \"value3\\b\"\n"),
    std::make_pair("Param2 \"value1\\b\" value2\\b\n", "List \"value1\\b\" value2\\b \"value3\\b\"\n"),
    std::make_pair("Param2 \"value1\\b\" \"value2\\b\"\n", "List \"value1\\b\" \"value2\\b\" value3\\b\n"),
    std::make_pair("Param2 \"value1\\b\" \"value2\\b\"\n", "List \"value1\\b\" \"value2\\b\" \"value3\\b\"\n")
));

TEST(TestTfnParsing, emptyarg) {
    ib_mpool_lite_t *mpool_lite;
    ib_mm_t mm;
    const char *target;
    ib_list_t *tfns;

    ASSERT_EQ(IB_OK, ib_mpool_lite_create(&mpool_lite));

    mm = ib_mm_mpool_lite(mpool_lite);

    ASSERT_EQ(IB_OK, ib_list_create(&tfns, mm));
    ASSERT_EQ(
        IB_OK,
        ib_cfg_parse_target_string(
            mm,
            "list.first()",
            &target,
            tfns
        )
    );
    ASSERT_STREQ("list", target);
    ASSERT_EQ(1U, ib_list_elements(tfns));
}

TEST(TestTfnParsing, 2emptyarg) {
    ib_mpool_lite_t *mpool_lite;
    ib_mm_t mm;
    const char *target;
    ib_list_t *tfns;

    ASSERT_EQ(IB_OK, ib_mpool_lite_create(&mpool_lite));

    mm = ib_mm_mpool_lite(mpool_lite);

    ASSERT_EQ(IB_OK, ib_list_create(&tfns, mm));
    ASSERT_EQ(
        IB_OK,
        ib_cfg_parse_target_string(
            mm,
            "list.first().first()",
            &target,
            tfns
        )
    );
    ASSERT_STREQ("list", target);
    ASSERT_EQ(2U, ib_list_elements(tfns));
}

