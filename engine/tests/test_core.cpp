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
 * @brief IronBee --- Core tests that don't fit elsewhere.
 *
 */

#include <gtest/gtest.h>
#include "base_fixture.h"

#include "core_private.h"

class CoreTest : public BaseTransactionFixture {
};

TEST_F(CoreTest, BlockingMode) {
    ib_core_cfg_t *corecfg;
    ib_module_t   *module;

    std::string config =
        std::string(
            "LogLevel INFO\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "BlockingMethod status=200\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname UnitTest\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());

    ASSERT_EQ(IB_OK, ib_engine_module_get(ib_engine, "core", &module));

    ASSERT_EQ(
        IB_OK,
        ib_context_module_config(ib_context_main(ib_engine), module, &corecfg));

    ASSERT_EQ(200, corecfg->block_status);
    ASSERT_EQ(IB_BLOCK_METHOD_STATUS, corecfg->block_method);
}

TEST_F(CoreTest, BlockingMode2) {
    ib_core_cfg_t *corecfg;
    ib_module_t   *module;

    std::string config =
        std::string(
            "LogLevel INFO\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "<Site test-site>\n"
            "   BlockingMethod status=403\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname UnitTest\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());

    ASSERT_EQ(IB_OK, ib_engine_module_get(ib_engine, "core", &module));

    ASSERT_EQ(
        IB_OK,
        ib_context_module_config(ib_context_main(ib_engine), module, &corecfg));

    ASSERT_EQ(403, corecfg->block_status);
    ASSERT_EQ(IB_BLOCK_METHOD_STATUS, corecfg->block_method);
}

TEST_F(CoreTest, TfnFirst) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "LoadModule ibmod_rules.so\n"
            "Action id:1 rev:1 phase:REQUEST_HEADER "
            "    setvar:list:element1=1\n"
            "Action id:2 rev:1 phase:REQUEST_HEADER "
            "    setvar:list:element2=2\n"
            "Rule list.first() @eq 1 id:3 rev:1 phase:REQUEST_HEADER "
            "    setvar:results:result1=1\n"
            "Rule list.last() @eq 2 id:4 rev:1 phase:REQUEST_HEADER "
            "    setvar:results:result2=2\n"
            "<Site default>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname *\n"
            "   Service *:*\n"
            "   RuleEnable all\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();

    /* Validation code */
    ib_var_target_t      *target;
    const ib_list_t      *list;
    const ib_list_node_t *node;
    const ib_field_t     *field;
    ib_num_t              num;

    ASSERT_EQ(
        IB_OK,
        ib_var_target_acquire_from_string(
            &target,
            ib_tx->mp,
            ib_engine_var_config_get(ib_engine),
            IB_S2SL("results:result1"),
            NULL,
            NULL));
    ASSERT_EQ(
        IB_OK,
        ib_var_target_get_const(
            target,
            &list,
            ib_tx->mp,
            ib_tx->var_store));
    ASSERT_EQ(1UL, ib_list_elements(list));

    node = ib_list_first_const(list);
    ASSERT_TRUE(node);
    field = (const ib_field_t *)ib_list_node_data_const(node);
    ASSERT_TRUE(field);
    ASSERT_EQ(
        IB_OK,
        ib_field_value_type(field, ib_ftype_num_out(&num), IB_FTYPE_NUM));
    ASSERT_EQ(1L, num);
}

TEST_F(CoreTest, TfnLast) {
    std::string config =
        std::string(
            "LogLevel INFO\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "LoadModule ibmod_rules.so\n"
            "Action id:1 rev:1 phase:REQUEST_HEADER "
            "    setvar:list:element1=1\n"
            "Action id:2 rev:1 phase:REQUEST_HEADER "
            "    setvar:list:element2=2\n"
            "Rule list.first() @eq 1 id:3 rev:1 phase:REQUEST_HEADER "
            "    setvar:results:result1=1\n"
            "Rule list.last() @eq 2 id:4 rev:1 phase:REQUEST_HEADER "
            "    setvar:results:result2=2\n"
            "<Site default>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname *\n"
            "   Service *:*\n"
            "   RuleEnable all\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();

    /* Validation code */
    ib_var_target_t      *target;
    const ib_list_t      *list;
    const ib_list_node_t *node;
    const ib_field_t     *field;
    ib_num_t              num;

    ASSERT_EQ(
        IB_OK,
        ib_var_target_acquire_from_string(
            &target,
            ib_tx->mp,
            ib_engine_var_config_get(ib_engine),
            IB_S2SL("results:result2"),
            NULL,
            NULL));
    ASSERT_EQ(
        IB_OK,
        ib_var_target_get_const(
            target,
            &list,
            ib_tx->mp,
            ib_tx->var_store));
    ASSERT_EQ(1UL, ib_list_elements(list));

    node = ib_list_first_const(list);
    ASSERT_TRUE(node);
    field = (const ib_field_t *)ib_list_node_data_const(node);
    ASSERT_TRUE(field);
    ASSERT_EQ(
        IB_OK,
        ib_field_value_type(field, ib_ftype_num_out(&num), IB_FTYPE_NUM));
    ASSERT_EQ(2L, num);
}
