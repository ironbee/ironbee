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
/// @brief IronBee --- Aho Corasick Module Tests
///
/// @author Sam Baskinger <sbaskinger@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

#include "base_fixture.h"
#include <ironbee/rule_engine.h>
#include <ironbee/operator.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>
#include <ironbee/field.h>

// @todo Remove this once there is something like ib_engine_operator_get()
#include "engine_private.h"

class AhoCorasickModuleTest : public BaseFixture {
public:
    ib_module_t *rules_mod;
    ib_module_t *ac_mod;

    AhoCorasickModuleTest() : BaseFixture()
    {
    }

    virtual void SetUp() {
        BaseFixture::SetUp();
        loadModule(&rules_mod, "ibmod_rules.so");
        loadModule(&ac_mod, "ibmod_ac.so");
        configureIronBee("AhoCorasickModuleTest.config");
    }
};

TEST_F(AhoCorasickModuleTest, test_pm_rule)
{
    ib_tx_t *tx; /**< We do need a transaction for the memory pool. */

    ib_rule_t *rule;
    ib_conn_t *conn;
    ib_operator_t op;
    ib_operator_inst_t *op_inst = NULL;
    ib_num_t result;

    ib_field_t* field1;
    ib_field_t* field2;

    char* str1 = (char *) ib_mpool_alloc(ib_engine_pool_main_get(ib_engine), (strlen("string1")+1));
    char* str2 = (char *) ib_mpool_alloc(ib_engine_pool_main_get(ib_engine), (strlen("string2")+1));
    strcpy(str1, "string1");
    strcpy(str2, "string2");

    conn = buildIronBeeConnection();

    ib_tx_create(&tx, conn, ib_context_engine(ib_engine));

    // Create field 1.
    ASSERT_EQ(IB_OK,
        ib_field_create(
            &field1,
            ib_engine_pool_main_get(ib_engine),
            IB_FIELD_NAME("field1"),
            IB_FTYPE_NULSTR,
            ib_ftype_nulstr_in(str1)
        )
    );

    // Create field 2.
    ASSERT_EQ(IB_OK,
        ib_field_create(
            &field2,
            ib_engine_pool_main_get(ib_engine),
            IB_FIELD_NAME("field2"),
            IB_FTYPE_NULSTR,
            ib_ftype_nulstr_in(str2)
        )
    );

    // Ensure that the operator exists.
    ASSERT_EQ(IB_OK, ib_hash_get(ib_engine->operators, &op, "pm"));

    ASSERT_IB_OK(ib_rule_create(ib_engine,
                                ib_context_engine(ib_engine),
                                __FILE__,
                                __LINE__,
                                true,
                                &rule));
    rule->meta.id = "fake-id";

    // Get the operator.
    ASSERT_EQ(IB_OK,
              ib_operator_inst_create(ib_engine,
                                      NULL,
                                      rule,
                                      IB_OP_FLAG_PHASE,
                                      "pm",
                                      "string2",
                                      IB_OPINST_FLAG_NONE,
                                      &op_inst));

    // Attempt to match.
    ASSERT_EQ(IB_OK, op_inst->op->fn_execute(
        ib_engine, tx, rule, op_inst->data, op_inst->flags, field1, &result));

    // We should fail.
    ASSERT_FALSE(result);

    // Attempt to match again.
    ASSERT_EQ(IB_OK, op_inst->op->fn_execute(
        ib_engine, tx, rule, op_inst->data, op_inst->flags, field2, &result));

    // This time we should succeed.
    ASSERT_TRUE(result);
}

TEST_F(AhoCorasickModuleTest, test_pmf_rule)
{
    ib_operator_t op;
    ib_operator_inst_t *op_inst = NULL;
    ib_num_t result;
    ib_rule_t *rule;
    ib_tx_t *tx;
    ib_conn_t *conn;

    ib_field_t* field1;
    ib_field_t* field2;

    char* str1 = (char *) ib_mpool_alloc(ib_engine_pool_main_get(ib_engine), (strlen("string1")+1));
    char* str2 = (char *) ib_mpool_alloc(ib_engine_pool_main_get(ib_engine), (strlen("string2")+1));
    strcpy(str1, "string1");
    strcpy(str2, "string2");

    conn = buildIronBeeConnection();

    ib_tx_create(&tx, conn, ib_context_engine(ib_engine));

    // Create field 1.
    ASSERT_EQ(IB_OK,
        ib_field_create(
            &field1,
            ib_engine_pool_main_get(ib_engine),
            IB_FIELD_NAME("field1"),
            IB_FTYPE_NULSTR,
            ib_ftype_nulstr_in(str1)
        )
    );

    // Create field 2.
    ASSERT_EQ(IB_OK,
        ib_field_create(
            &field2,
            ib_engine_pool_main_get(ib_engine),
            IB_FIELD_NAME("field2"),
            IB_FTYPE_NULSTR,
            ib_ftype_nulstr_in(str2)
        )
    );

    // Ensure that the operator exists.
    ASSERT_EQ(IB_OK, ib_hash_get(ib_engine->operators, &op, "pmf"));

    ASSERT_IB_OK(ib_rule_create(ib_engine,
                                ib_context_engine(ib_engine),
                                __FILE__,
                                __LINE__,
                                true,
                                &rule));
    rule->meta.id = "fake-id";

    // Get the operator.
    ASSERT_EQ(IB_OK,
              ib_operator_inst_create(ib_engine,
                                      NULL,
                                      rule,
                                      IB_OP_FLAG_PHASE,
                                      "pmf",
                                      "ahocorasick.patterns",
                                      IB_OPINST_FLAG_NONE,
                                      &op_inst));

    // Attempt to match.
    ASSERT_EQ(IB_OK, op_inst->op->fn_execute(
        ib_engine, tx, rule, op_inst->data, op_inst->flags, field1, &result));

    // We should fail.
    ASSERT_TRUE(result);

    // Attempt to match again.
    ASSERT_EQ(IB_OK, op_inst->op->fn_execute(
        ib_engine, tx, rule, op_inst->data, op_inst->flags, field2, &result));

    // This time we should succeed.
    ASSERT_TRUE(result);
}
