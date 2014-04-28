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
 * @brief IronBee++ Internals --- Action Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/action.hpp>

#include <ironbee/rule_engine.h>

#include <ironbeepp/test_fixture.hpp>

#include "../engine/engine_private.h"

#include "gtest/gtest.h"

using namespace IronBee;
using namespace std;

class TestAction : public ::testing::Test, public TestFixture
{
};

void test_action(
    const ib_rule_exec_t** out_rule_exec,
    const ib_rule_exec_t*  rule_exec,
    void*                  instance_data
)
{
    *out_rule_exec = rule_exec;
}

TEST_F(TestAction, basic)
{
    MemoryManager mm = m_engine.main_memory_mm();
    const ib_rule_exec_t* result_rule_exec = NULL;
    ib_rule_exec_t rule_exec;
    Action action = Action::create<void>(
        mm,
        "test",
        NULL,
        NULL,
        boost::bind(test_action, &result_rule_exec, _1, _2)
    );

    ASSERT_NO_THROW(action.register_with(m_engine));

    ConstAction other_action =
        ConstAction::lookup(m_engine, "test");
    EXPECT_EQ(action, other_action);

    ActionInstance::create(mm, m_engine, action, "").execute(&rule_exec);
    EXPECT_EQ(result_rule_exec, &rule_exec);
}
