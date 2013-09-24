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

#include <predicate/dag.hpp>
#include <ironbeepp/test_fixture.hpp>

#include "gtest/gtest.h"

#include <boost/lexical_cast.hpp>

using namespace IronBee::Predicate;
using namespace std;

class TestDAG : public ::testing::Test, public IronBee::TestFixture
{
};

static ib_field_t c_field = ib_field_t();

class DummyCall : public Call
{
public:
    virtual string name() const
    {
        return "dummy_call";
    }

protected:
    virtual void calculate(EvalContext)
    {
        add_value(Value(&c_field));
        finish();
    }
};

class DummyCall2 : public DummyCall
{
public:
    virtual string name() const
    {
        return "dummy_call2";
    }
};

TEST_F(TestDAG, Node)
{
    node_p n(new DummyCall);

    EXPECT_EQ("(dummy_call)", n->to_s());
    EXPECT_TRUE(n->children().empty());
    EXPECT_TRUE(n->parents().empty());
    EXPECT_FALSE(n->is_finished());
    EXPECT_TRUE(n->values().empty());

    EXPECT_EQ(&c_field, n->eval(m_transaction).front().ib());
    EXPECT_TRUE(n->is_finished());

    n->reset();
    EXPECT_FALSE(n->is_finished());

    node_p n2(new DummyCall);
    n->add_child(n2);
    EXPECT_EQ(1UL, n->children().size());
    EXPECT_EQ(n2, n->children().front());
    EXPECT_EQ(1UL, n2->parents().size());
    EXPECT_EQ(n, n2->parents().front().lock());
}

TEST_F(TestDAG, String)
{
    String n("node");
    EXPECT_EQ("'node'", n.to_s());
    EXPECT_EQ("node", n.value_as_s());
    EXPECT_EQ(
        "node",
        n.eval(EvalContext()).front().value_as_byte_string().to_s()
    );
    EXPECT_TRUE(n.is_literal());
}

TEST_F(TestDAG, StringEscaping)
{
    EXPECT_EQ("'\\''", String("'").to_s());
    EXPECT_EQ("'foo\\'bar'", String("foo'bar").to_s());
    EXPECT_EQ("'foo\\\\bar'", String("foo\\bar").to_s());
    EXPECT_EQ("'foo\\\\'", String("foo\\").to_s());
}

TEST_F(TestDAG, Integer)
{
    Integer n(0);
    EXPECT_EQ("0", n.to_s());
    EXPECT_EQ(0, n.value_as_i());
    EXPECT_EQ(
        0,
        n.eval(EvalContext()).front().value_as_number()
    );
    EXPECT_TRUE(n.is_literal());
}

TEST_F(TestDAG, Float)
{
    Float n(1.2);
    EXPECT_FLOAT_EQ(1.2, boost::lexical_cast<long double>(n.to_s()));
    EXPECT_FLOAT_EQ(1.2, n.value_as_f());
    EXPECT_FLOAT_EQ(
        1.2,
        n.eval(EvalContext()).front().value_as_float()
    );
    EXPECT_TRUE(n.is_literal());
}

TEST_F(TestDAG, Call)
{
    node_p n(new DummyCall);

    EXPECT_EQ("(dummy_call)", n->to_s());
    EXPECT_EQ(&c_field, n->eval(m_transaction).front().ib());
    EXPECT_TRUE(n->is_finished());

    node_p a1(new DummyCall);
    n->add_child(a1);
    node_p a2(new String("foo"));
    n->add_child(a2);

    EXPECT_EQ("(dummy_call (dummy_call) 'foo')", n->to_s());
    EXPECT_FALSE(n->is_literal());
}

TEST_F(TestDAG, OutputOperator)
{
    stringstream s;
    DummyCall c;

    s << c;

    EXPECT_EQ("(dummy_call)", s.str());
}

TEST_F(TestDAG, Null)
{
    Null n;
    EXPECT_EQ("null", n.to_s());
    EXPECT_TRUE(n.eval(EvalContext()).empty());
    EXPECT_TRUE(n.is_finished());
    EXPECT_TRUE(n.is_literal());
}

TEST_F(TestDAG, DeepCall)
{
    node_p n(new DummyCall);
    node_p n2(new DummyCall);
    node_p n3(new DummyCall);
    node_p n4(new DummyCall);
    n->add_child(n2);
    n2->add_child(n3);
    EXPECT_EQ("(dummy_call (dummy_call (dummy_call)))", n->to_s());
    n3->add_child(n4);
    // Note distance between n and n4.
    EXPECT_EQ("(dummy_call (dummy_call (dummy_call (dummy_call))))", n->to_s());
}

TEST_F(TestDAG, ModifyChildren)
{
    node_p p(new DummyCall);
    node_p c1(new DummyCall);
    node_p c2(new DummyCall2);

    EXPECT_THROW(p->remove_child(c1), IronBee::enoent);
    EXPECT_THROW(p->remove_child(node_p()), IronBee::einval);
    EXPECT_THROW(p->add_child(node_p()), IronBee::einval);
    ASSERT_NO_THROW(p->add_child(c1));
    EXPECT_EQ("(dummy_call (dummy_call))", p->to_s());
    ASSERT_NO_THROW(p->add_child(c2));
    EXPECT_EQ("(dummy_call (dummy_call) (dummy_call2))", p->to_s());
    ASSERT_NO_THROW(p->remove_child(c1));
    EXPECT_EQ("(dummy_call (dummy_call2))", p->to_s());
    EXPECT_THROW(p->replace_child(c1, c2), IronBee::enoent);
    EXPECT_THROW(p->replace_child(c2, node_p()), IronBee::einval);
    EXPECT_THROW(p->replace_child(node_p(), c2), IronBee::einval);
    ASSERT_NO_THROW(p->add_child(c1));
    EXPECT_EQ("(dummy_call (dummy_call2) (dummy_call))", p->to_s());
    ASSERT_NO_THROW(p->replace_child(c2, c1));
    EXPECT_EQ("(dummy_call (dummy_call) (dummy_call))", p->to_s());
    EXPECT_EQ(2UL, c1->parents().size());
    EXPECT_EQ(p, c1->parents().front().lock());
    EXPECT_EQ(p, boost::next(c1->parents().begin())->lock());
    EXPECT_TRUE(c2->parents().empty());
}

namespace {

class test_thread_worker
{
public:
    test_thread_worker(Value v, EvalContext c, node_p n) :
        m_v(v), m_c(c), m_n(n)
    {
        // nop
    }

    void operator()()
    {
        for (int i = 0; i < 10000; ++i) {
            if (m_n->eval(m_c).front() != m_v) {
                throw runtime_error("FAIL");
            }
            usleep(i % 100);
        }
    }

private:
    Value m_v;
    EvalContext m_c;
    node_p m_n;
};

class ConstantCall : public Call
{
public:
    ConstantCall(Value a, Value b) : m_a(a), m_b(b) {}

    virtual string name() const
    {
        return "constant_call";
    }

protected:
    virtual void calculate(EvalContext context)
    {
        add_value(context ? m_a : m_b);
        finish();
    }

private:
    Value m_a;
    Value m_b;
};

}

TEST_F(TestDAG, Threaded)
{
    ib_tx_t dummy_tx;
    ib_field_t dummy_field;
    EvalContext nonnull(&dummy_tx);
    Value a(&dummy_field); // non-null
    Value b; // null

    node_p n(new ConstantCall(a, b));

    // Failure will be a thread throwing an exception.
    boost::thread_group g;
    g.create_thread(test_thread_worker(a, nonnull, n));
    g.create_thread(test_thread_worker(b, EvalContext(), n));
    g.join_all();
}
