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
 * @brief Predicate --- Eval Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/eval.hpp>
#include <predicate/value.hpp>
#include <ironbeepp/test_fixture.hpp>

#include "gtest/gtest.h"

#include <boost/lexical_cast.hpp>

using namespace IronBee::Predicate;
using namespace std;

class TestEval : public ::testing::Test, public IronBee::TestFixture
{
};

TEST_F(TestEval, NodeEvalState_Trivial)
{
    NodeEvalState nes;

    EXPECT_FALSE(nes.is_finished());
    EXPECT_FALSE(nes.is_forwarding());
    EXPECT_FALSE(nes.is_aliased());
    EXPECT_FALSE(nes.forwarded_to());
    EXPECT_EQ(IB_PHASE_NONE, nes.phase());
    EXPECT_FALSE(nes.value());
    EXPECT_TRUE(nes.state().empty());
}

TEST_F(TestEval, NodeEvalState_Finish)
{
    {
        NodeEvalState nes;

        EXPECT_FALSE(nes.is_finished());
        EXPECT_NO_THROW(nes.finish());
        EXPECT_TRUE(nes.is_finished());
        EXPECT_THROW(nes.finish(), IronBee::einval);
    }

    {
        NodeEvalState nes;

        EXPECT_FALSE(nes.is_finished());
        EXPECT_NO_THROW(nes.finish());
        EXPECT_TRUE(nes.is_finished());
        EXPECT_THROW(nes.finish(), IronBee::einval);
        EXPECT_FALSE(nes.value());
    }

    {
        NodeEvalState nes;

        EXPECT_FALSE(nes.is_finished());
        EXPECT_NO_THROW(nes.finish_true(m_transaction));
        EXPECT_TRUE(nes.is_finished());
        EXPECT_THROW(nes.finish(), IronBee::einval);
        EXPECT_TRUE(nes.value());
    }
}

TEST_F(TestEval, NodeEvalState_Local)
{
    NodeEvalState nes;

    nes.setup_local_list(m_transaction);
    ASSERT_TRUE(nes.value());
    EXPECT_TRUE(nes.value().as_list().empty());
    EXPECT_FALSE(nes.is_forwarding());
    EXPECT_FALSE(nes.is_aliased());
    EXPECT_FALSE(nes.forwarded_to());

    nes.append_to_list(Value());
    EXPECT_EQ(1UL, nes.value().as_list().size());

    EXPECT_THROW(nes.forward(node_p()), IronBee::einval);
    EXPECT_THROW(nes.alias(Value()), IronBee::einval);
    EXPECT_NO_THROW(nes.setup_local_list(m_transaction));

    EXPECT_NO_THROW(nes.finish());
    EXPECT_TRUE(nes.is_finished());
}

TEST_F(TestEval, NodeEvalState_Forwarded)
{
    node_p n(new Literal());

    NodeEvalState nes;

    nes.forward(n);
    EXPECT_TRUE(nes.is_forwarding());
    EXPECT_EQ(n, nes.forwarded_to());

    EXPECT_THROW(nes.setup_local_list(m_transaction), IronBee::einval);
    EXPECT_THROW(nes.forward(node_p()), IronBee::einval);
    EXPECT_THROW(nes.alias(Value()), IronBee::einval);
    EXPECT_THROW(nes.finish(), IronBee::einval);
    EXPECT_THROW(nes.append_to_list(Value()), IronBee::einval);
}

TEST_F(TestEval, NodeEvalState_Aliased)
{
	ib_field_t f;
	Value v(&f);
    NodeEvalState nes;

    nes.alias(v);
    EXPECT_TRUE(nes.is_aliased());
    EXPECT_EQ(v, nes.value());

    EXPECT_THROW(nes.setup_local_list(m_transaction), IronBee::einval);
    EXPECT_THROW(nes.forward(node_p()), IronBee::einval);
    EXPECT_THROW(nes.alias(Value()), IronBee::einval);
    EXPECT_THROW(nes.append_to_list(Value()), IronBee::einval);

    EXPECT_NO_THROW(nes.finish());
    EXPECT_TRUE(nes.is_finished());
}

TEST_F(TestEval, NodeEvalState_Phase)
{
    NodeEvalState nes;

    EXPECT_EQ(IB_PHASE_NONE, nes.phase());
    nes.set_phase(IB_PHASE_REQUEST_HEADER);
    EXPECT_EQ(IB_PHASE_REQUEST_HEADER, nes.phase());
}

TEST_F(TestEval, NodeEvalState_State)
{
    NodeEvalState nes;
    int i = 5;

    EXPECT_TRUE(nes.state().empty());
    nes.state() = i;
    EXPECT_FALSE(nes.state().empty());
    EXPECT_EQ(i, boost::any_cast<int>(nes.state()));
}

TEST_F(TestEval, GraphEvalState)
{
    GraphEvalState ges(5);
    NodeEvalState& local = ges[0];
    NodeEvalState& alias = ges[1];
    NodeEvalState& forwarded = ges[2];
    NodeEvalState& forwarded2 = ges[3];

    node_p n0(new Literal);
    node_p n1(new Literal);
    node_p n2(new Literal);
    node_p n3(new Literal);
    node_p n4(new Literal("Hello World"));

    n0->set_index(0);
    n1->set_index(1);
    n2->set_index(2);
    n3->set_index(3);
    n4->set_index(4);

    forwarded2.forward(n2);
    forwarded.forward(n4);

	ib_field_t f;
	Value v(&f);

    alias.alias(v);
    alias.finish();

    local.setup_local_list(m_transaction);

    EXPECT_EQ(&ges[0], &ges.final(0));
    EXPECT_EQ(&ges[1], &ges.final(1));
    EXPECT_EQ(&ges[4], &ges.final(2));
    EXPECT_EQ(&ges[4], &ges.final(3));
    EXPECT_EQ(&ges[4], &ges.final(4));

    ges.initialize(n4, m_transaction);
    Value result = ges.eval(n3, m_transaction);

    EXPECT_TRUE(result);
    EXPECT_EQ("Hello World", result.to_s());

    EXPECT_TRUE(ges.value(0));
    EXPECT_TRUE(ges.value(1));
    EXPECT_TRUE(ges.value(2));
    EXPECT_TRUE(ges.value(3));
    EXPECT_TRUE(ges.value(4));

    EXPECT_FALSE(ges.is_finished(0));
    EXPECT_TRUE(ges.is_finished(1));
    EXPECT_TRUE(ges.is_finished(2));
    EXPECT_TRUE(ges.is_finished(3));
    EXPECT_TRUE(ges.is_finished(4));
}
