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
/// @brief IronBee --- Rule inject tests
///
/// @author Nick LeRoy <nleroy@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "base_fixture.h"

#include "ibtest_util.hpp"
#include "engine_private.h"
#include <ironbee/hash.h>
#include <ironbee/mm.h>
#include <ironbee/field.h>
#include <ironbee/rule_defs.h>
#include <ironbee/rule_engine.h>

#include <string>
#include <list>

/**
 * The config creates 4 rules with id "inject-{1,2,3,4}", in that order.
 *
 * The operator for all is identical, and should always return 1, so the
 * actions will execute.  "inject-{3,4}" use the inject action, defined below,
 * which has not execute function.
 *
 * All rules use the store action, which adds the rule to the m_actions list.
 * The ownership function ownership_fn checks each rule to see if the "inject"
 * action is registered for it.
 *
 *   If yes, the ownership function adds the rule to the m_injection list, and
 *   returns IB_OK. This should be the case for rules "inject-{3,4}".
 *
 *   If no, the ownership function returns IB_DECLINED. This should be the
 *   case for rules "inject-{1,2}".
 *
 * The injection function injection_fn injects the rules in m_injection list
 * by adding them to the rule list. This should inject rules "inject-{3,4}".
 *
 * Because rules "inject-{3,4}" were injected, they will run at the start of
 * the phase, before rules "inject-{1,2}".
 *
 * The store action adds the list to the m_action list, thus recording the
 * order of rule execution.
 *
 * The test then verifies that the rules executed in the proper order, namely
 * (inject-{3,4,1,2).
*/

/**
 * Test rules.
 */
class RuleInjectTest : public BaseTransactionFixture
{
public:
    ib_list_t *m_injections;
    ib_list_t *m_actions;

public:

    RuleInjectTest() :
        BaseTransactionFixture(),
        m_injections(NULL),
        m_actions(NULL)
    {
    }

    virtual void SetUp()
    {
        BaseTransactionFixture::SetUp();

        ASSERT_IB_OK(ib_list_create(&m_injections, ib_engine_mm_main_get(ib_engine)));
        ASSERT_IB_OK(ib_list_create(&m_actions, ib_engine_mm_main_get(ib_engine)));
    }

};

static const char *name = "inject";

/* "inject" action creation function */
static ib_status_t create_fn(
    ib_mm_t       mm,
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    *(void **)instance_data = cbdata;
    return IB_OK;
}

/* "store" action execute function, adds rule to m_actions list */
static
ib_status_t store_fn(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    void                 *cbdata
)
{
    RuleInjectTest *p = static_cast<RuleInjectTest *>(cbdata);
    ib_status_t rc;

    rc = ib_list_push(p->m_actions, rule_exec->rule);

    return rc;
}

/* Ownership function, adds inject rules to the m_injections list */
static ib_status_t ownership_fn(
    const ib_engine_t  *ib,
    const ib_rule_t    *rule,
    const ib_context_t *ctx,
    void               *cbdata)
{
    ib_status_t rc;
    size_t count = 0;

    RuleInjectTest *p = static_cast<RuleInjectTest *>(cbdata);

    rc = ib_rule_search_action(ib, rule, IB_RULE_ACTION_TRUE,
                               name, NULL, &count);
    if (rc != IB_OK) {
        return rc;
    }
    if (count == 0) {
        return IB_DECLINED;
    }

    rc = ib_list_push(p->m_injections, (ib_rule_t *)rule);
    return rc;
}

/* Injection function, injects rules rules from the m_injections list */
static ib_status_t injection_fn(
    const ib_engine_t    *ib,
    const ib_rule_exec_t *rule_exec,
    ib_list_t            *rule_list,
    void                 *cbdata)
{
    const ib_list_node_t *node;
    RuleInjectTest *p = static_cast<RuleInjectTest *>(cbdata);

    IB_LIST_LOOP_CONST(p->m_injections, node) {
        const ib_rule_t *rule = (const ib_rule_t *)node->data;
        if (rule->meta.phase == rule_exec->phase) {
            ib_status_t rc = ib_list_push(rule_list, (ib_rule_t *)rule);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }
    return IB_OK;
}

TEST_F(RuleInjectTest, test_inject)
{
    ib_status_t rc;
    const ib_list_node_t *node;
    const ib_rule_t *rule;

    // Register the inject action and related rule engine callbacks
    rc = ib_action_create_and_register(
        NULL,
        ib_engine, name,
        create_fn, this,
        NULL, NULL,
        NULL, NULL
    );
    ASSERT_EQ(IB_OK, rc);

    rc = ib_action_create_and_register(
        NULL, ib_engine, "store",
        NULL, NULL,
        NULL, NULL,
        store_fn, this
    );
    ASSERT_EQ(IB_OK, rc);

    // Register the ownership function
    rc = ib_rule_register_ownership_fn(ib_engine, name, ownership_fn, this);
    ASSERT_EQ(IB_OK, rc);

    // Register the injection function
    rc = ib_rule_register_injection_fn(ib_engine, name, IB_PHASE_REQUEST_HEADER,
                                       injection_fn, this);
    ASSERT_EQ(IB_OK, rc);

    // Setup IronBee after the ownership function is registered
    configureIronBee();

    // Verify that the correct rules were added to the injection list
    ASSERT_EQ(2U, ib_list_elements(m_injections));

    node = ib_list_first_const(m_injections);
    ASSERT_TRUE(node);
    ASSERT_TRUE(node->data);
    rule = (const ib_rule_t *)node->data;
    ASSERT_TRUE(strstr(ib_rule_id(rule), "inject-3"));

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    ASSERT_TRUE(node->data);
    rule = (const ib_rule_t *)node->data;
    ASSERT_TRUE(strstr(ib_rule_id(rule), "inject-4"));

    // Now, run the transaction
    performTx();

    // Verify that the correct number of rules were executed
    ASSERT_EQ(4U, ib_list_elements(m_actions));

    // Verify that the rules were executed in the expected order
    node = ib_list_first_const(m_actions);
    ASSERT_TRUE(node);
    ASSERT_TRUE(node->data);
    rule = (const ib_rule_t *)node->data;
    ASSERT_TRUE(strstr(ib_rule_id(rule), "inject-3"));

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    ASSERT_TRUE(node->data);
    rule = (const ib_rule_t *)node->data;
    ASSERT_TRUE(strstr(ib_rule_id(rule), "inject-4"));

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    ASSERT_TRUE(node->data);
    rule = (const ib_rule_t *)node->data;
    ASSERT_TRUE(strstr(ib_rule_id(rule), "inject-1"));

    node = ib_list_node_next_const(node);
    ASSERT_TRUE(node);
    ASSERT_TRUE(node->data);
    rule = (const ib_rule_t *)node->data;
    ASSERT_TRUE(strstr(ib_rule_id(rule), "inject-2"));
}
