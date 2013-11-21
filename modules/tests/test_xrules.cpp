//////////////////////////////////////////////////////////////////////////////
// Licensed to Qualys, Inc. (QUALYS) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// QUALYS licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee --- PCRE module tests
///
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "base_fixture.h"
#include <ironbee/operator.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>
#include <ironbee/field.h>
#include <ironbee/bytestr.h>

#include <boost/filesystem.hpp>

class XRulesTest :
    public BaseTransactionFixture
{
public:
    virtual void sendRequestLine()
    {
        BaseTransactionFixture::sendRequestLine("GET", "/foo/bar", "HTTP/1.1");
    }
};

TEST_F(XRulesTest, Load) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
}

TEST_F(XRulesTest, IPv4) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleIpv4 \"1.0.0.2/32\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, IPv6) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleIpv6 \"::1/128\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_FALSE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, Path) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_htp.so\"\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRulePath \"/foo/bar\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, PathPrefix) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_htp.so\"\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRulePath \"/fo\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, Time1) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleTime \"00:00-23:59+0000\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, Time2) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleTime \"!00:00-23:59+0000\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_FALSE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, Time3) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleTime \"0,1,2,3,4,5,6,7@00:00-23:59+0000\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, Time4) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleTime \"!0,1,2,3,4,5,6,7@00:00-23:59+0000\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_FALSE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, Time5) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleTime \"0@15:00-17:45-0800\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
}

TEST_F(XRulesTest, Time6) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleTime \"4@02:00-02:10-0800\" Block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
}

TEST_F(XRulesTest, ReqContentType1) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleRequestContentType \"*\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, ReqContentType2) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleRequestContentType \"text/html\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, ReqContentType3) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleRequestContentType \"text/bob\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_FALSE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, RespContentType) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleResponseContentType \"text/html\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, ScaleThreat) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleResponseContentType \"text/html\" ScaleThreat=1 priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
}

TEST_F(XRulesTest, RunGeoIP) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_persistence_framework.so\"\n"
            "LoadModule \"ibmod_init_collection.so\"\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "InitCollection GeoIP vars: country_code=US\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleGeo \"US\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, SetFlag) {
    ib_field_t       *field;
    const ib_list_t  *list;
    ib_num_t          num;

    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_persistence_framework.so\"\n"
            "LoadModule \"ibmod_init_collection.so\"\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "InitCollection GeoIP vars: country_code=US\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            /* Note that both rules should fire and result in a single entry. */
            "XRulePath /  EnableRequestParamInspection priority=1\n"
            "XRuleGeo US EnableRequestParamInspection priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_FINSPECT_REQPARAMS);

    ASSERT_EQ(
        IB_OK,
        ib_data_get(ib_tx->data, "FLAGS:inspectRequestParams", &field)
    );
    ASSERT_EQ(IB_FTYPE_LIST, field->type);
    ASSERT_EQ(
        IB_OK,
        ib_field_value(field, ib_ftype_list_out(&list))
    );
    ASSERT_EQ(1U, ib_list_elements(list));
    field = (ib_field_t *)ib_list_node_data_const(ib_list_first_const(list));
    ASSERT_EQ(IB_FTYPE_NUM, field->type);
    ASSERT_EQ(
        IB_OK,
        ib_field_value(field, ib_ftype_num_out(&num))
    );
    ASSERT_EQ(1, num);
}

TEST_F(XRulesTest, RespBlockAny) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleResponseContentType \"*\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

class XRulesTest2 :
    public BaseTransactionFixture
{
    public:
    virtual void generateResponseHeader()
    {
        addResponseHeader("X-MyHeader", "header3");
        addResponseHeader("X-MyHeader", "header4");
        addResponseHeader("Transport-Encoding", "somebits");
    }
};

TEST_F(XRulesTest2, RespBlockNone) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleResponseContentType \"\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

