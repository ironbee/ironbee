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

class PersistenceLoadModuleTest :
    public BaseTransactionFixture,
    public ::testing::WithParamInterface<const char *> { };

class PersistenceImplTest :
    public BaseTransactionFixture,
    public ::testing::WithParamInterface<
        std::pair<
            const char *, const char * > > { };

TEST_P(PersistenceLoadModuleTest, LoadModule) {
    std::string config =
        std::string(
            "# A basic ironbee configuration\n"
            "# for getting an engine up-and-running.\n"
            "LogLevel 9\n"

            "LoadModule \"ibmod_htp.so\"\n"
            "LoadModule \"ibmod_rules.so\"\n"
        )
        + GetParam()
        + std::string(
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

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
}

INSTANTIATE_TEST_CASE_P(
    WithConfiguration,
    PersistenceLoadModuleTest,
    ::testing::Values(
        "LoadModule \"ibmod_persistence_framework.so\"\n",

        "LoadModule \"ibmod_persistence_framework.so\"\n"
        "LoadModule \"ibmod_init_collection.so\"\n"
    )
);

TEST_P(PersistenceImplTest, InitCollection) {
    ib_field_t           *field = NULL;
    const ib_list_t      *field_list;
    const ib_list_node_t *node;
    const ib_field_t     *val;
    const ib_bytestr_t   *bs;
    const char           *str;

    std::string config =
        std::string(
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
        )
        + GetParam().first
        + std::string(
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname *\n"
        )
        + GetParam().second
        + std::string(
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();

    ASSERT_TRUE(ib_tx);

    ASSERT_EQ(IB_OK, ib_data_get(ib_tx->data, "COL1", &field));
    ASSERT_TRUE(field);
    ASSERT_EQ(IB_FTYPE_LIST, field->type);
    ASSERT_EQ(IB_OK, ib_field_value(field, ib_ftype_list_out(&field_list)));

    /* Check A=a1. */
    node = ib_list_first_const(field_list);
    ASSERT_TRUE(node);
    val = reinterpret_cast<const ib_field_t *>(ib_list_node_data_const(node));
    ASSERT_TRUE(val);
    ASSERT_EQ(IB_FTYPE_BYTESTR, val->type);
    ASSERT_EQ(1U, val->nlen);
    ASSERT_EQ(0, strncmp("A", val->name, val->nlen));
    ASSERT_EQ(IB_OK, ib_field_value(val, ib_ftype_bytestr_out(&bs)));
    str = reinterpret_cast<const char *>(ib_bytestr_const_ptr(bs));
    ASSERT_EQ(0, strncmp("a1", str, 2));

    /* Check B=b1. */
    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    val = reinterpret_cast<const ib_field_t *>(ib_list_node_data_const(node));
    ASSERT_TRUE(val);
    ASSERT_EQ(IB_FTYPE_BYTESTR, val->type);
    ASSERT_EQ(1U, val->nlen);
    ASSERT_EQ(0, strncmp("B", val->name, val->nlen));
    ASSERT_EQ(IB_OK, ib_field_value(val, ib_ftype_bytestr_out(&bs)));
    str = reinterpret_cast<const char *>(ib_bytestr_const_ptr(bs));
    ASSERT_EQ(0, strncmp("b1", str, 2));

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
    val = reinterpret_cast<const ib_field_t *>(ib_list_node_data_const(node));
    ASSERT_TRUE(val);
    ASSERT_EQ(IB_FTYPE_BYTESTR, val->type);
    ASSERT_EQ(1U, val->nlen);
    ASSERT_EQ(0, strncmp("A", val->name, val->nlen));
    ASSERT_EQ(IB_OK, ib_field_value(val, ib_ftype_bytestr_out(&bs)));
    str = reinterpret_cast<const char *>(ib_bytestr_const_ptr(bs));
    ASSERT_EQ(0, strncmp("a2", str, 2));

    /* Check B=b2. */
    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    val = reinterpret_cast<const ib_field_t *>(ib_list_node_data_const(node));
    ASSERT_TRUE(val);
    ASSERT_EQ(IB_FTYPE_BYTESTR, val->type);
    ASSERT_EQ(1U, val->nlen);
    ASSERT_EQ(0, strncmp("B", val->name, val->nlen));
    ASSERT_EQ(IB_OK, ib_field_value(val, ib_ftype_bytestr_out(&bs)));
    str = reinterpret_cast<const char *>(ib_bytestr_const_ptr(bs));
    ASSERT_EQ(0, strncmp("b2", str, 2));

    /* Check end of list. */
    node = ib_list_node_next_const(node);
    ASSERT_FALSE(node);
}

INSTANTIATE_TEST_CASE_P(
    WithConfiguration,
    PersistenceImplTest,
    ::testing::Values(
        std::make_pair(
            "InitCollection COL1 vars: A=a1 B=b1\n",
            "InitCollection COL2 vars: A=a2 B=b2\n"),
        std::make_pair(
            "InitCollection COL1 json-file://init_collection_1.json\n",
            "InitCollection COL2 json-file://init_collection_2.json\n")
    )
);
