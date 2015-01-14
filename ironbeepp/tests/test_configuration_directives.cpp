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
 * @brief IronBee++ Internals --- Configuration Directives Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/configuration_directives.hpp>
#include <ironbeepp/configuration_parser.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/site.hpp>
#include <ironbeepp/test_fixture.hpp>

#include "gtest/gtest.h"

#include <ironbee/engine.h>
#include <ironbee/config.h>

#include <boost/foreach.hpp>

using namespace std;
using namespace IronBee;

class TestConfigurationDirectives :
    public ::testing::Test, public TestFixture
{
};

struct Info
{
    Info() : which(0), on(false), mask(0), value(0) {}

    int                 which;
    ConfigurationParser parser;
    string              name;
    string              param1;
    string              param2;
    bool                on;
    vector<string>      nparam;
    ib_flags_t          mask;
    ib_flags_t          value;
};

class Handler
{
public:
    explicit Handler(
        Info& out_info
    ) :
        m_out_info(out_info)
    {
        // nop
    }

    void operator()(
        ConfigurationParser parser,
        const char*         name,
        const char*         param1
    )
    {
        m_out_info.which  = 1;
        m_out_info.parser = parser;
        m_out_info.name   = name;
        m_out_info.param1 = param1;
    }

    void operator()(
        ConfigurationParser parser,
        const char*         name,
        const char*         param1,
        const char*         param2
    )
    {
        m_out_info.which  = 2;
        m_out_info.parser = parser;
        m_out_info.name   = name;
        m_out_info.param1 = param1;
        m_out_info.param2 = param2;
    }

    void operator()(
        ConfigurationParser parser,
        const char*         name
    )
    {
        m_out_info.which  = 3;
        m_out_info.parser = parser;
        m_out_info.name   = name;
    }

    void operator()(
        ConfigurationParser parser,
        const char*         name,
        bool                on
    )
    {
        m_out_info.which  = 4;
        m_out_info.parser = parser;
        m_out_info.name   = name;
        m_out_info.on     = on;
    }

    void operator()(
        ConfigurationParser parser,
        const char*         name,
        List<const char*>   args
    )
    {
        m_out_info.which  = 5;
        m_out_info.parser = parser;
        m_out_info.name   = name;

        BOOST_FOREACH(const char* a, args) {
            m_out_info.nparam.push_back(a);
        }
    }
    void operator()(
        ConfigurationParser parser,
        const char*         name,
        ib_flags_t          value,
        ib_flags_t          mask
    )
    {
        m_out_info.which  = 6;
        m_out_info.parser = parser;
        m_out_info.name   = name;
        m_out_info.value  = value;
        m_out_info.mask   = mask;
    }

private:
    Info& m_out_info;
};

TEST_F(TestConfigurationDirectives, Registrar)
{
    ib_cfgparser_t* parser = NULL;
    ib_status_t rc;

    rc = ib_cfgparser_create(&parser, m_engine.ib());
    ASSERT_EQ(IB_OK, rc);
    ASSERT_TRUE(parser);
    rc = ib_engine_config_started(m_engine.ib(), parser);
    ASSERT_EQ(IB_OK, rc);
    ConfigurationParser P(parser);

    Info info;
    Handler handler(info);
    Info info2;
    Handler handler2(info2);

    ConfigurationDirectivesRegistrar R(m_engine);
    R.param1("Param1", handler);
    R.param2("Param2", handler);
    R.block("Block", handler, handler2);
    R.on_off("OnOff", handler);
    R.list("List", handler);
    map<string,ib_flags_t> value_map;
    value_map["a"] = (1 << 1) & (1 << 3);
    value_map["b"] = (1 << 7);
    R.op_flags("OpFlags", handler, value_map);

    info = Info();
    P.parse_buffer("Param1 HelloWorld\n", false);
    ASSERT_EQ(IB_OK, ib_cfgparser_apply(P.ib(), m_engine.ib()));
    EXPECT_EQ(1,            info.which);
    EXPECT_EQ(P,            info.parser);
    EXPECT_EQ("Param1",     info.name);
    EXPECT_EQ("HelloWorld", info.param1);

    info = Info();
    P.parse_buffer("Param2 Foo Bar\n", false);
    ASSERT_EQ(IB_OK, ib_cfgparser_apply(P.ib(), m_engine.ib()));
    EXPECT_EQ(2,        info.which);
    EXPECT_EQ(P,        info.parser);
    EXPECT_EQ("Param2", info.name);
    EXPECT_EQ("Foo",    info.param1);
    EXPECT_EQ("Bar",    info.param2);

    info = Info();
    info2 = Info();
    P.parse_buffer("<Block Foo>\n</Block>\n", false);
    ASSERT_EQ(IB_OK, ib_cfgparser_apply(P.ib(), m_engine.ib()));
    EXPECT_EQ(1,       info.which);
    EXPECT_EQ(P,       info.parser);
    EXPECT_EQ("Block", info.name);
    EXPECT_EQ("Foo",   info.param1);
    EXPECT_EQ(3,       info2.which);
    EXPECT_EQ(P,       info2.parser);
    EXPECT_EQ("Block", info2.name);

    info = Info();
    P.parse_buffer("OnOff true\n", false);
    ASSERT_EQ(IB_OK, ib_cfgparser_apply(P.ib(), m_engine.ib()));
    EXPECT_EQ(4,       info.which);
    EXPECT_EQ(P,       info.parser);
    EXPECT_EQ("OnOff", info.name);
    EXPECT_TRUE(info.on);

    info = Info();
    P.parse_buffer("OnOff false\n", false);
    ASSERT_EQ(IB_OK, ib_cfgparser_apply(P.ib(), m_engine.ib()));
    EXPECT_EQ(4,       info.which);
    EXPECT_EQ(P,       info.parser);
    EXPECT_EQ("OnOff", info.name);
    EXPECT_FALSE(info.on);    info = Info();

    info = Info();
    P.parse_buffer("List a b c d\n", false);
    ASSERT_EQ(IB_OK, ib_cfgparser_apply(P.ib(), m_engine.ib()));
    EXPECT_EQ(5,      info.which);
    EXPECT_EQ(P,      info.parser);
    EXPECT_EQ("List", info.name);
    EXPECT_EQ(4UL,    info.nparam.size());
    EXPECT_EQ("a",    info.nparam[0]);
    EXPECT_EQ("b",    info.nparam[1]);
    EXPECT_EQ("c",    info.nparam[2]);
    EXPECT_EQ("d",    info.nparam[3]);

    info = Info();
    P.parse_buffer("OpFlags +a -b\n", false);
    ASSERT_EQ(IB_OK, ib_cfgparser_apply(P.ib(), m_engine.ib()));
    EXPECT_EQ(6,         info.which);
    EXPECT_EQ(P,         info.parser);
    EXPECT_EQ("OpFlags", info.name);
    EXPECT_EQ(value_map["a"] | value_map["b"], info.mask);
    EXPECT_EQ(value_map["a"], info.value & info.mask);
    EXPECT_EQ(value_map["b"], ~info.value & info.mask);

    rc = ib_engine_config_finished(m_engine.ib());
    ASSERT_EQ(IB_OK, rc);
    rc = ib_cfgparser_destroy(parser);
    ASSERT_EQ(IB_OK, rc);
}
