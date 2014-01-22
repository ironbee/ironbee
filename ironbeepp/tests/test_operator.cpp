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
 * @brief IronBee++ Internals --- Operator Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/operator.hpp>

#include <ironbeepp/test_fixture.hpp>

#include "../engine/engine_private.h"

#include "gtest/gtest.h"

using namespace IronBee;
using namespace std;

class TestOperator : public ::testing::Test, public TestFixture
{
};

class operator_instance
{
public:
    operator_instance(const string& param, string& result) :
        m_param(param),
        m_result(result)
    {
        // nop
    }

    int operator()(Transaction, ConstField, Field)
    {
        m_result = m_param;
        return 42;
    }

private:
    string m_param;
    string& m_result;
};

class operator_generator
{
public:
    explicit
    operator_generator(string& result) :
        m_result(result)
    {
        // nop
    }

    Operator::operator_instance_t operator()(Context, const char* param)
    {
        return operator_instance(param, m_result);
    }

private:
    string& m_result;
};

TEST_F(TestOperator, advanced)
{
    // This will also test the normal interface as it makes heavy use of it.

    ib_context_t ctx;
    ctx.ib = m_engine.ib();

    string result;
    Operator op = Operator::create(
        m_engine.main_memory_pool(),
        "advanced",
        0,
        operator_generator(result)
    );

    ASSERT_NO_THROW(op.register_with(m_engine));

    ConstOperator other_op = ConstOperator::lookup(m_engine, "advanced");
    EXPECT_EQ(op, other_op);

    void* instance_data = op.create_instance(Context(&ctx), 0, "abc");
    ASSERT_TRUE(instance_data);
    ASSERT_EQ(42, op.execute_instance(instance_data, Transaction(), Field()));
    ASSERT_EQ("abc", result);

    ASSERT_NO_THROW(op.destroy_instance(instance_data));
}

TEST_F(TestOperator, existing)
{
    ScopedMemoryPool smp;
    ConstOperator op = ConstOperator::lookup(m_engine, "match");

    // 0 means no required capabilities.
    void* instance_data =
        op.create_instance(m_engine.main_context(), 0, "foo");
    ASSERT_EQ(1,
        op.execute_instance(instance_data, m_transaction,
            Field::create_byte_string(
                smp, "", 0,
                ByteString::create(smp, "foo")
            )
        )
    );

    ASSERT_NO_THROW(op.destroy_instance(instance_data));
}
