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
 * @brief Predicate --- DAG Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include "../dag.hpp"
#include "../../ironbeepp/tests/fixture.hpp"

#include "gtest/gtest.h"

using namespace IronBee::Predicate;
using namespace std;

class TestDAG : public ::testing::Test, public IBPPTestFixture
{
};

static const ib_field_t c_field = ib_field_t();

class DummyCall : public DAG::Call
{
public:
    virtual string name() const
    {
        return "dummy_call";
    }

protected:
    virtual DAG::Value calculate(DAG::Context)
    {
        return IronBee::ConstField(&c_field);
    }
};

TEST_F(TestDAG, Node)
{
    DAG::node_p n(new DummyCall);

    EXPECT_EQ("(dummy_call)", n->to_s());
    EXPECT_TRUE(n->children().empty());
    EXPECT_TRUE(n->parents().empty());
    EXPECT_FALSE(n->has_value());
    EXPECT_THROW(n->value(), IronBee::einval);

    EXPECT_EQ(&c_field, n->eval(m_transaction).ib());
    EXPECT_TRUE(n->has_value());

    n->reset();
    EXPECT_FALSE(n->has_value());

    DAG::node_p n2(new DummyCall);
    n->add_child(n2);
    EXPECT_EQ(1, n->children().size());
    EXPECT_EQ(n2, n->children().front());
    EXPECT_EQ(1, n2->parents().size());
    EXPECT_EQ(n, n2->parents().front().lock());
}

TEST_F(TestDAG, String)
{
    DAG::String n("node");
    EXPECT_EQ("'node'", n.to_s());
    EXPECT_EQ("node", n.value_as_s());
    EXPECT_TRUE(n.is_static());
    EXPECT_EQ(
        "node",
        n.eval(DAG::Context()).value_as_byte_string().to_s()
    );
}

TEST_F(TestDAG, StringEscaping)
{
    EXPECT_EQ("'\\''", DAG::String("'").to_s());
    EXPECT_EQ("'foo\\'bar'", DAG::String("foo'bar").to_s());
    EXPECT_EQ("'foo\\\\bar'", DAG::String("foo\\bar").to_s());
    EXPECT_EQ("'foo\\\\'", DAG::String("foo\\").to_s());
}

TEST_F(TestDAG, Call)
{
    DAG::node_p n(new DummyCall);

    EXPECT_EQ("(dummy_call)", n->to_s());
    EXPECT_EQ(&c_field, n->eval(m_transaction).ib());
    EXPECT_TRUE(n->has_value());

    DAG::node_p a1(new DummyCall);
    n->add_child(a1);
    DAG::node_p a2(new DAG::String("foo"));
    n->add_child(a2);

    EXPECT_EQ("(dummy_call (dummy_call) 'foo')", n->to_s());
}

TEST_F(TestDAG, OutputOperator)
{
    stringstream s;

    s << DummyCall();

    EXPECT_EQ("(dummy_call)", s.str());
}

TEST_F(TestDAG, Null)
{
    DAG::Null n;
    EXPECT_EQ("null", n.to_s());
    EXPECT_TRUE(n.is_static());
    EXPECT_FALSE(n.eval(DAG::Context()));
}

TEST_F(TestDAG, DeepCall)
{
    DAG::node_p n(new DummyCall);
    DAG::node_p n2(new DummyCall);
    DAG::node_p n3(new DummyCall);
    DAG::node_p n4(new DummyCall);
    n->add_child(n2);
    n2->add_child(n3);
    EXPECT_EQ("(dummy_call (dummy_call (dummy_call)))", n->to_s());
    n3->add_child(n4);
    // Note distance between n and n4.
    EXPECT_EQ("(dummy_call (dummy_call (dummy_call (dummy_call))))", n->to_s());
}
