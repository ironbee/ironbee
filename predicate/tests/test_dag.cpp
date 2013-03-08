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

class TestDAG : public ::testing::Test, public IBPPTestFixture
{
};

static const ib_field_t c_field = ib_field_t();

class DummyNode : public DAG::Node
{
public:
    virtual std::string to_s() const
    {
        return "dummy";
    }

    virtual size_t hash() const
    {
        return 1234;
    }

protected:
    virtual void calculate(DAG::Context)
    {
        set_value(IronBee::ConstField(&c_field));
    }
};

class DummyCall : public DAG::Call<DummyCall>
{
public:
    static const std::string class_name;

    virtual size_t hash() const
    {
        return 1234;
    }

protected:
    virtual void calculate(DAG::Context)
    {
        set_value(IronBee::ConstField(&c_field));
    }
};

const std::string DummyCall::class_name("dummy_call");

class DummyOrderedCall : public DAG::OrderedCall<DummyOrderedCall>
{
public:
    static const std::string class_name;

protected:
    virtual void calculate(DAG::Context)
    {
        set_value(IronBee::ConstField(&c_field));
    }
};

const std::string DummyOrderedCall::class_name("dummy ordered call");

class DummyUnorderedCall : public DAG::UnorderedCall<DummyUnorderedCall>
{
public:
    static const std::string class_name;

protected:
    virtual void calculate(DAG::Context)
    {
        set_value(IronBee::ConstField(&c_field));
    }
};

const std::string DummyUnorderedCall::class_name("dummy unordered call");

TEST_F(TestDAG, Node)
{
    DummyNode n;

    EXPECT_EQ("dummy", n.to_s());
    EXPECT_EQ(1234, n.hash());
    EXPECT_TRUE(n.children().empty());
    EXPECT_TRUE(n.parents().empty());
    EXPECT_FALSE(n.has_value());
    EXPECT_THROW(n.value(), IronBee::einval);

    EXPECT_EQ(&c_field, n.eval(m_transaction).ib());
    EXPECT_TRUE(n.has_value());

    n.reset();
    EXPECT_FALSE(n.has_value());
}

TEST_F(TestDAG, Literal)
{
    EXPECT_EQ("'node'", DAG::Literal("node").to_s());
    EXPECT_NE(DAG::Literal("node").hash(), DAG::Literal("node2").hash());
    EXPECT_EQ(DAG::Literal("node").hash(), DAG::Literal("node").hash());

    EXPECT_EQ(
        "node",
        DAG::Literal("node").eval(m_transaction).value_as_byte_string().to_s()
    );
}

TEST_F(TestDAG, LiteralEscaping)
{
    EXPECT_EQ("'\\''", DAG::Literal("'").to_s());
    EXPECT_EQ("'foo\\'bar'", DAG::Literal("foo'bar").to_s());
    EXPECT_EQ("'foo\\\\bar'", DAG::Literal("foo\\bar").to_s());
    EXPECT_EQ("'foo\\\\'", DAG::Literal("foo\\").to_s());
}

TEST_F(TestDAG, Call)
{
    DummyCall n;

    EXPECT_EQ("(dummy_call)", n.to_s());
    EXPECT_EQ(1234, n.hash());
    EXPECT_EQ(&c_field, n.eval(m_transaction).ib());
    EXPECT_TRUE(n.has_value());

    DAG::node_p a1(new DummyCall());
    n.children().push_back(a1);
    DAG::node_p a2(new DAG::Literal("foo"));
    n.children().push_back(a2);

    EXPECT_EQ("(dummy_call (dummy_call) 'foo')", n.to_s());
}

TEST_F(TestDAG, OrderedCall)
{
    DAG::node_p n1(new DummyOrderedCall());
    DAG::node_p n2(new DummyOrderedCall());
    DAG::node_p a1(new DummyCall());
    DAG::node_p a2(new DAG::Literal("foo"));

    n1->children().push_back(a1);
    n1->children().push_back(a2);
    n2->children().push_back(a2);
    n2->children().push_back(a1);

    EXPECT_NE(n1->hash(), n2->hash());
}

TEST_F(TestDAG, UnorderedCall)
{
    DAG::node_p n1(new DummyUnorderedCall());
    DAG::node_p n2(new DummyUnorderedCall());
    DAG::node_p a1(new DummyCall());
    DAG::node_p a2(new DAG::Literal("foo"));

    n1->children().push_back(a1);
    n1->children().push_back(a2);
    n2->children().push_back(a2);
    n2->children().push_back(a1);

    // Note EQ vs. NE for OrderedCall.
    EXPECT_EQ(n1->hash(), n2->hash());
}

TEST_F(TestDAG, OutputOperator)
{
    std::stringstream s;

    s << DummyNode();

    EXPECT_EQ("dummy", s.str());
}
