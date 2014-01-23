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
#include <ironbee/rule_engine.h>
#include <ironbee/operator.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>
#include <ironbee/field.h>
#include <ironbee/capture.h>
#include <ironbee/bytestr.h>

// @todo Remove once ib_engine_operator_get() is available.
#include "engine_private.h"

#include <string>

class PcreModuleTest : public BaseTransactionFixture
{
public:

    ib_rule_exec_t rule_exec1;
    ib_rule_exec_t rule_exec2;
    ib_rule_t *rule1;
    ib_rule_t *rule2;
    ib_field_t* field1;
    ib_field_t* field2;

    virtual void SetUp()
    {
        ib_status_t rc;
        const char *s1 = "string 1";
        const char *s2 = "string 2";

        BaseTransactionFixture::SetUp();
        configureIronBee();
        performTx();

        ib_mpool_t *mp = ib_engine_pool_main_get(ib_engine);
        char* str1 = (char *)ib_mpool_alloc(mp, (strlen(s1)+1));
        if (str1 == NULL) {
            throw std::runtime_error("Could not allocate string 1.");
        }
        strcpy(str1, s1);
        char* str2 = (char *)ib_mpool_alloc(mp, (strlen(s2)+1));
        if (str1 == NULL) {
            throw std::runtime_error("Could not allocate string 2.");
        }
        strcpy(str2, s2);

        // Create field 1.
        rc = ib_field_create(&field1,
                             mp,
                             IB_FIELD_NAME("field1"),
                             IB_FTYPE_NULSTR,
                             ib_ftype_nulstr_in(str1));
        if (rc != IB_OK) {
            throw std::runtime_error("Could not initialize field1.");
        }

        // Create field 2.
        rc = ib_field_create(&field2,
                             mp,
                             IB_FIELD_NAME("field2"),
                             IB_FTYPE_NULSTR,
                             ib_ftype_nulstr_in(str2));
        if (rc != IB_OK) {
            throw std::runtime_error("Could not initialize field2.");
        }

        /* Create rule 1 */
        rc = ib_rule_create(ib_engine,
                            ib_context_engine(ib_engine),
                            __FILE__,
                            __LINE__,
                            true,
                            &rule1);
        if (rc != IB_OK) {
            throw std::runtime_error("Could not create rule1.");
        }
        rc = ib_rule_set_id(ib_engine, rule1, "rule1");
        if (rc != IB_OK) {
            throw std::runtime_error("Could not set ID for rule1.");
        }

        /* Create the rule execution object #1 */
        memset(&rule_exec1, 0, sizeof(rule_exec1));
        rule_exec1.ib = ib_engine;
        rule_exec1.tx = ib_tx;
        rule_exec1.rule = rule1;

        /* Create rule 2 */
        rc = (ib_rule_create(ib_engine,
                             ib_context_engine(ib_engine),
                             __FILE__,
                             __LINE__,
                             true,
                             &rule2));
        if (rc != IB_OK) {
            throw std::runtime_error("Could not create rule2.");
        }
        rc = ib_rule_set_id(ib_engine, rule2, "rule2");
        if (rc != IB_OK) {
            throw std::runtime_error("Could not set ID for rule2.");
        }
        rule2->flags |= IB_RULE_FLAG_CAPTURE;

        /* Create the rule execution object #1 */
        memset(&rule_exec2, 0, sizeof(rule_exec2));
        rule_exec2.ib = ib_engine;
        rule_exec2.tx = ib_tx;
        rule_exec2.rule = rule2;
    }

};

TEST_F(PcreModuleTest, test_load_module)
{
    const ib_operator_t *op;
    ASSERT_EQ(IB_OK, ib_operator_lookup(ib_engine, "pcre", &op));
}

TEST_F(PcreModuleTest, test_pcre_operator)
{
    ib_field_t *outfield;
    ib_num_t result;
    ib_field_t *capture;
    const ib_operator_t *op;
    void *instance_data = NULL;
    ASSERT_EQ(IB_OK, ib_operator_lookup(ib_engine, "pcre", &op));

    // Create the operator instance.
    ASSERT_EQ(
        IB_OK,
        ib_operator_inst_create(
              op,
              ib_context_main(ib_engine),
              IB_OP_CAPABILITY_NONE,
              "string\\s2",
              &instance_data
        )
    );

    // Attempt to match.
    ASSERT_EQ(
        IB_OK,
        ib_operator_inst_execute(
            op, instance_data,
            rule_exec1.tx,
            field1,
            NULL,
            &result
        )
    );

    // We should fail.
    ASSERT_FALSE(result);

    // Attempt to match again.
    ASSERT_EQ(
        IB_OK,
        ib_operator_inst_execute(
            op, instance_data,
            rule_exec1.tx,
            field2,
            NULL,
            &result
        )
    );

    // This time we should succeed.
    ASSERT_TRUE(result);

    // Should be no capture set */
    outfield = getTarget1(IB_TX_CAPTURE":0");
    ASSERT_FALSE(outfield);

    // Create the operator instance.
    ASSERT_EQ(
        IB_OK,
        ib_operator_inst_create(
            op,
            ib_context_main(ib_engine),
            IB_OP_CAPABILITY_NONE,
            "(string 2)",
            &instance_data
        )
    );

    // Attempt to match.
    ASSERT_EQ(
        IB_OK,
        ib_operator_inst_execute(
            op, instance_data,
            rule_exec1.tx,
            field1,
            NULL,
            &result
        )
    );

    // We should fail.
    ASSERT_FALSE(result);

    // Attempt to match again.
    ASSERT_EQ(
        IB_OK,
        ib_operator_inst_execute(
            op, instance_data,
            rule_exec1.tx,
            field2,
            NULL,
            &result
        )
    );

    // This time we should succeed.
    ASSERT_TRUE(result);

    // Should be no capture (CAPTURE flag not set for rule 1)
    outfield = getTarget1(IB_TX_CAPTURE":0");
    ASSERT_FALSE(outfield);

    // Create the operator instance.
    ASSERT_EQ(
        IB_OK,
        ib_operator_inst_create(
            op,
            ib_context_main(ib_engine),
            IB_OP_CAPABILITY_NONE,
            "(string 2)",
            &instance_data
        )
    );

    ASSERT_EQ(IB_OK,
              ib_capture_acquire(
                  rule_exec2.tx,
                  NULL,
                  &capture));

    // Attempt to match again.
    ASSERT_EQ(
        IB_OK,
        ib_operator_inst_execute(
            op, instance_data,
            rule_exec1.tx,
            field2,
            capture,
            &result
        )
    );

    // This time we should succeed.
    ASSERT_TRUE(result);

    // Should be a capture (CAPTURE flag is set for rule 2)
    outfield = getTarget1(IB_TX_CAPTURE":0");
    ASSERT_TRUE(outfield);
}

TEST_F(PcreModuleTest, test_match_basic)
{
    ib_field_t *outfield;

    // Should be no capture set */
    outfield = getTarget1(IB_TX_CAPTURE":0");
    ASSERT_FALSE(outfield);
}

TEST_F(PcreModuleTest, test_match_capture)
{
    ib_field_t *ib_field;

    const ib_bytestr_t *ib_bytestr;
    ib_status_t rc;

    /* Check :0 */
    ib_field = getTarget1(IB_TX_CAPTURE":0");
    ASSERT_NE(static_cast<ib_field_t*>(NULL), ib_field);
    ASSERT_EQ(static_cast<ib_ftype_t>(IB_FTYPE_BYTESTR), ib_field->type);

    /* Check :1 */
    ib_field = getTarget1(IB_TX_CAPTURE":1");
    ASSERT_NE(static_cast<ib_field_t*>(NULL), ib_field);
    ASSERT_EQ(static_cast<ib_ftype_t>(IB_FTYPE_BYTESTR), ib_field->type);

    /* Check :2 */
    ib_field = getTarget1(IB_TX_CAPTURE":2");
    ASSERT_NE(static_cast<ib_field_t*>(NULL), ib_field);
    ASSERT_EQ(static_cast<ib_ftype_t>(IB_FTYPE_BYTESTR), ib_field->type);

    rc = ib_field_value(ib_field, ib_ftype_bytestr_out(&ib_bytestr));
    ASSERT_EQ(IB_OK, rc);

    /* Check that a value is over written correctly. */
    ASSERT_EQ("4", std::string(
        reinterpret_cast<const char*>(ib_bytestr_const_ptr(ib_bytestr)),
        ib_bytestr_length(ib_bytestr)
    ));

    ib_field = getTarget1(IB_TX_CAPTURE":3");
    ASSERT_FALSE(ib_field);
}

TEST_F(PcreModuleTest, test_match_capture_named)
{
    ib_field_t *ib_field;
    const char *capname;

    const ib_bytestr_t *ib_bytestr;
    ib_status_t rc;

    /* Check :0 */
    capname = ib_capture_fullname(ib_tx, "captest", 0);
    ASSERT_STREQ(capname, "captest:0");
    ib_field = getTarget1(capname);
    ASSERT_NE(static_cast<ib_field_t*>(NULL), ib_field);
    ASSERT_EQ(static_cast<ib_ftype_t>(IB_FTYPE_BYTESTR), ib_field->type);

    /* Check :1 */
    capname = ib_capture_fullname(ib_tx, "captest", 1);
    ASSERT_STREQ(capname, "captest:1");
    ib_field = getTarget1(capname);
    ASSERT_NE(static_cast<ib_field_t*>(NULL), ib_field);
    ASSERT_EQ(static_cast<ib_ftype_t>(IB_FTYPE_BYTESTR), ib_field->type);

    /* Check :2 */
    capname = ib_capture_fullname(ib_tx, "captest", 2);
    ASSERT_STREQ(capname, "captest:2");
    ib_field = getTarget1(capname);
    ASSERT_NE(static_cast<ib_field_t*>(NULL), ib_field);
    ASSERT_EQ(static_cast<ib_ftype_t>(IB_FTYPE_BYTESTR), ib_field->type);

    rc = ib_field_value(ib_field, ib_ftype_bytestr_out(&ib_bytestr));
    ASSERT_EQ(IB_OK, rc);

    /* Check that a value is over written correctly. */
    ASSERT_EQ("4", std::string(
        reinterpret_cast<const char*>(ib_bytestr_const_ptr(ib_bytestr)),
        ib_bytestr_length(ib_bytestr)
    ));

    capname = ib_capture_fullname(ib_tx, "captest", 3);
    ASSERT_STREQ(capname, "captest:3");
    ib_field = getTarget1(capname);
    ASSERT_FALSE(ib_field);
}
