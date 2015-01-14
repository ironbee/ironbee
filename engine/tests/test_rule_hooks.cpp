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
 * @brief IronBee --- Rule Engine Hook Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "gtest/gtest.h"
#include "base_fixture.h"

#include <ironbee/rule_engine.h>

class RuleHooksTest : public BaseTransactionFixture
{
};

struct result_t
{
    result_t() : next_at(0), rule_exec(NULL), at(0) {}

    void called(const ib_rule_exec_t* rule_exec_)
    {
        rule_exec = rule_exec_;
        at = next_at;
        ++next_at;
    }

    static int next_at;
    const ib_rule_exec_t* rule_exec;
    int at;
};
int result_t::next_at = 0;

struct pre_operator_result_t : result_t
{
    pre_operator_result_t() :
        opinst(NULL), invert(false), value(NULL) {}

    void called(
        const ib_rule_exec_t*     rule_exec_,
        const ib_operator_inst_t* opinst_,
        bool                      invert_,
        const ib_field_t*         value_
    )
    {
        result_t::called(rule_exec_);
        opinst = opinst_;
        invert = invert_;
        value = value_;
    }

    const ib_operator_inst_t* opinst;
    bool invert;
    const ib_field_t* value;
};

struct post_operator_result_t : pre_operator_result_t
{
    post_operator_result_t() :
        op_rc(IB_ENOTIMPL), result(2), capture(NULL) {}

    void called(
        const ib_rule_exec_t*     rule_exec_,
        const ib_operator_inst_t* opinst_,
        bool                      invert_,
        const ib_field_t*         value_,
        ib_status_t               op_rc_,
        ib_num_t                  result_,
        ib_field_t*               capture_
    )
    {
        pre_operator_result_t::called(
            rule_exec_, opinst_, invert_, value_
        );
        op_rc = op_rc_;
        result = result_;
        capture = capture_;
    }

    ib_status_t op_rc;
    ib_num_t result;
    ib_field_t* capture;
};

struct pre_action_result_t : result_t
{
    pre_action_result_t() :
        action(NULL), result(2) {}

    void called(
        const ib_rule_exec_t*   rule_exec_,
        const ib_action_inst_t* action_,
        ib_num_t                result_
    )
    {
        result_t::called(rule_exec_);
        action = action_;
        result = result_;
    }

    const ib_action_inst_t* action;
    ib_num_t result;
};

struct post_action_result_t : pre_action_result_t
{
    post_action_result_t() :
        act_rc(IB_ENOTIMPL) {}

    void called(
        const ib_rule_exec_t*   rule_exec_,
        const ib_action_inst_t* action_,
        ib_num_t                result_,
        ib_status_t             act_rc_
    )
    {
        pre_action_result_t::called(rule_exec_, action_, result_);
        act_rc = act_rc_;
    }

    ib_status_t act_rc;
};

extern "C" {

static
void pre_post_rule(
    const ib_rule_exec_t* rule_exec,
    void*                 cbdata
)
{
    result_t* hook_result =
        reinterpret_cast<result_t*>(cbdata);
    hook_result->called(rule_exec);
}

static
void pre_operator(
    const ib_rule_exec_t*     rule_exec,
    const ib_operator_inst_t* opinst,
    bool                      invert,
    const ib_field_t*         value,
    void*                     cbdata
)
{
    pre_operator_result_t* hook_result =
        reinterpret_cast<pre_operator_result_t*>(cbdata);
    hook_result->called(rule_exec, opinst, invert, value);
}

static
void post_operator(
    const ib_rule_exec_t*     rule_exec,
    const ib_operator_inst_t* opinst,
    bool                      invert,
    const ib_field_t*         value,
    ib_status_t               op_rc,
    ib_num_t                  result,
    ib_field_t*               capture,
    void*                     cbdata
)
{
    post_operator_result_t* hook_result =
        reinterpret_cast<post_operator_result_t*>(cbdata);
    hook_result->called(
        rule_exec,
        opinst,
        invert,
        value,
        op_rc,
        result,
        capture
    );
}

static
void pre_action(
    const ib_rule_exec_t*   rule_exec,
    const ib_action_inst_t* action,
    ib_num_t                result,
    void*                   cbdata
)
{
    pre_action_result_t* hook_result =
        reinterpret_cast<pre_action_result_t*>(cbdata);
    hook_result->called(rule_exec, action, result);
}

static
void post_action(
    const ib_rule_exec_t*   rule_exec,
    const ib_action_inst_t* action,
    ib_num_t                result,
    ib_status_t             act_rc,
    void*                   cbdata
)
{
    post_action_result_t* hook_result =
        reinterpret_cast<post_action_result_t*>(cbdata);
    hook_result->called(rule_exec, action, result, act_rc);
}

} // extern "C"

TEST_F(RuleHooksTest, test_basic)
{
    result_t pre_rule_result;
    result_t post_rule_result;
    pre_operator_result_t pre_operator_result;
    post_operator_result_t post_operator_result;
    pre_action_result_t pre_action_result;
    post_action_result_t post_action_result;

    result_t::next_at = 1;

    ASSERT_EQ(IB_OK, ib_rule_register_pre_rule_fn(
        ib_engine,
        pre_post_rule, &pre_rule_result
    ));
    ASSERT_EQ(IB_OK, ib_rule_register_post_rule_fn(
        ib_engine,
        pre_post_rule, &post_rule_result
    ));
    ASSERT_EQ(IB_OK, ib_rule_register_pre_operator_fn(
        ib_engine,
        pre_operator, &pre_operator_result
    ));
    ASSERT_EQ(IB_OK, ib_rule_register_post_operator_fn(
        ib_engine,
        post_operator, &post_operator_result
    ));
    ASSERT_EQ(IB_OK, ib_rule_register_pre_action_fn(
        ib_engine,
        pre_action, &pre_action_result
    ));
    ASSERT_EQ(IB_OK, ib_rule_register_post_action_fn(
        ib_engine,
        post_action, &post_action_result
    ));

    configureIronBee();
    performTx();

    EXPECT_EQ(1, pre_rule_result.at);
    EXPECT_TRUE(pre_rule_result.rule_exec);
    EXPECT_EQ(2, pre_operator_result.at);
    EXPECT_TRUE(pre_operator_result.rule_exec);
    EXPECT_TRUE(pre_operator_result.opinst);
    EXPECT_FALSE(pre_operator_result.invert);
    EXPECT_TRUE(pre_operator_result.value);
    EXPECT_EQ(3, post_operator_result.at);
    EXPECT_TRUE(post_operator_result.rule_exec);
    EXPECT_TRUE(post_operator_result.opinst);
    EXPECT_FALSE(post_operator_result.invert);
    EXPECT_TRUE(post_operator_result.value);
    EXPECT_EQ(IB_OK, post_operator_result.op_rc);
    EXPECT_EQ(1, post_operator_result.result);
    EXPECT_FALSE(post_operator_result.capture);
    EXPECT_EQ(4, pre_action_result.at);
    EXPECT_TRUE(pre_action_result.rule_exec);
    EXPECT_TRUE(pre_action_result.action);
    EXPECT_EQ(1, pre_action_result.result);
    EXPECT_EQ(5, post_action_result.at);
    EXPECT_TRUE(post_action_result.rule_exec);
    EXPECT_TRUE(post_action_result.action);
    EXPECT_EQ(1, post_action_result.result);
    EXPECT_EQ(IB_OK, post_action_result.act_rc);
    EXPECT_EQ(6, post_rule_result.at);
    EXPECT_TRUE(post_rule_result.rule_exec);
}
