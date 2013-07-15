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

class PersistenceTest : public BaseTransactionFixture {
public:
    virtual void SetUp() {
        BaseTransactionFixture::SetUp();
    }

    virtual void TearDown() {
        BaseTransactionFixture::TearDown();
    }

    virtual ~PersistenceTest() { }
};

TEST_F(PersistenceTest, Load) {
    configureIronBeeByString(
        "# A basic ironbee configuration\n"
        "# for getting an engine up-and-running.\n"
        "LogLevel 9\n"

        "LoadModule \"ibmod_htp.so\"\n"
        "LoadModule \"ibmod_rules.so\"\n"
        "LoadModule \"ibmod_persistence_framework.so\"\n"

        "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
        "SensorName UnitTesting\n"
        "SensorHostname unit-testing.sensor.tld\n"

        "# Disable audit logs\n"
        "AuditEngine Off\n"

        "<Site test-site>\n"
        "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
        "   Hostname somesite.com\n"
        "</Site>\n"
    );

    performTx();
    ASSERT_TRUE(ib_tx);
}

TEST_F(PersistenceTest, LoadInitColl) {
    configureIronBeeByString(
        "# A basic ironbee configuration\n"
        "# for getting an engine up-and-running.\n"
        "LogLevel 9\n"

        "LoadModule \"ibmod_htp.so\"\n"
        "LoadModule \"ibmod_rules.so\"\n"
        "LoadModule \"ibmod_persistence_framework.so\"\n"
        "LoadModule \"ibmod_init_collection.so\"\n"

        "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
        "SensorName UnitTesting\n"
        "SensorHostname unit-testing.sensor.tld\n"

        "# Disable audit logs\n"
        "AuditEngine Off\n"

        "<Site test-site>\n"
        "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
        "   Hostname somesite.com\n"
        "</Site>\n"
    );

    performTx();
    ASSERT_TRUE(ib_tx);
}

TEST_F(PersistenceTest, InitVars) {
    ib_field_t           *field = NULL;
    const ib_list_t      *field_list;
    const ib_list_node_t *node;
    const ib_field_t     *val;
    const char           *str;
    configureIronBeeByString(
        "# A basic ironbee configuration\n"
        "# for getting an engine up-and-running.\n"
        "LogLevel 9\n"

        "LoadModule \"ibmod_htp.so\"\n"
        "LoadModule \"ibmod_rules.so\"\n"
        "LoadModule \"ibmod_persistence_framework.so\"\n"
        "LoadModule \"ibmod_init_collection.so\"\n"

        "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
        "SensorName UnitTesting\n"
        "SensorHostname unit-testing.sensor.tld\n"

        "# Disable audit logs\n"
        "AuditEngine Off\n"
        "InitCollection COL1 vars: A=a1 B=b1\n"

        "<Site test-site>\n"
        "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
        "   Hostname *\n"
        "   InitCollection COL2 vars: A=a2 B=b2\n"
        "</Site>\n"
    );

    performTx();
    ASSERT_TRUE(ib_tx);

    ASSERT_EQ(IB_OK, ib_data_get(ib_tx->data, "COL1", &field));
    ASSERT_TRUE(field);
    ASSERT_EQ(IB_FTYPE_LIST, field->type);
    ASSERT_EQ(IB_OK, ib_field_value(field, ib_ftype_list_out(&field_list)));

    /* Check A=a1. */
    node = ib_list_first_const(field_list);
    ASSERT_TRUE(node);
    val = (const ib_field_t *)ib_list_node_data_const(node);
    ASSERT_TRUE(val);
    ASSERT_EQ(IB_FTYPE_NULSTR, val->type);
    ASSERT_EQ(1U, val->nlen);
    ASSERT_EQ(0, strncmp("A", val->name, val->nlen));
    ASSERT_EQ(IB_OK, ib_field_value(val, ib_ftype_nulstr_out(&str)));
    ASSERT_STREQ("a1", str);

    /* Check B=b1. */
    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    val = (const ib_field_t *)ib_list_node_data_const(node);
    ASSERT_TRUE(val);
    ASSERT_EQ(IB_FTYPE_NULSTR, val->type);
    ASSERT_EQ(1U, val->nlen);
    ASSERT_EQ(0, strncmp("B", val->name, val->nlen));
    ASSERT_EQ(IB_OK, ib_field_value(val, ib_ftype_nulstr_out(&str)));
    ASSERT_STREQ("b1", str);

    /* Check end of list. */
    node = ib_list_node_next_const(node);
    ASSERT_FALSE(node);

    field = NULL;
    ASSERT_EQ(IB_OK, ib_data_get(ib_tx->data, "COL2", &field));
    ASSERT_TRUE(field);
    ASSERT_EQ(IB_FTYPE_LIST, field->type);
    ASSERT_EQ(IB_OK, ib_field_value(field, ib_ftype_list_out(&field_list)));

    /* Check A=a2. */
    node = ib_list_first_const(field_list);
    ASSERT_TRUE(node);
    val = (const ib_field_t *)ib_list_node_data_const(node);
    ASSERT_TRUE(val);
    ASSERT_EQ(IB_FTYPE_NULSTR, val->type);
    ASSERT_EQ(1U, val->nlen);
    ASSERT_EQ(0, strncmp("A", val->name, val->nlen));
    ASSERT_EQ(IB_OK, ib_field_value(val, ib_ftype_nulstr_out(&str)));
    ASSERT_STREQ("a2", str);

    /* Check B=b2. */
    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    val = (const ib_field_t *)ib_list_node_data_const(node);
    ASSERT_TRUE(val);
    ASSERT_EQ(IB_FTYPE_NULSTR, val->type);
    ASSERT_EQ(1U, val->nlen);
    ASSERT_EQ(0, strncmp("B", val->name, val->nlen));
    ASSERT_EQ(IB_OK, ib_field_value(val, ib_ftype_nulstr_out(&str)));
    ASSERT_STREQ("b2", str);

    /* Check end of list. */
    node = ib_list_node_next_const(node);
    ASSERT_FALSE(node);

}
