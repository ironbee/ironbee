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
 * @brief Predicate --- Standard Development Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbee/predicate/standard_development.hpp>

#include "standard_test.hpp"

using namespace IronBee::Predicate;
using namespace std;

class TestStandardDevelopment :
    public StandardTest
{
protected:
    void SetUp()
    {
        Standard::load_development(factory());
    }
};

TEST_F(TestStandardDevelopment, p)
{
    EXPECT_EQ("'foo'", eval("(p 'a' 5 'foo')"));
    EXPECT_EQ("c:[d:'foo' e:5]", eval("(p a:'foo' b:5 c:[d:'foo' e:5])"));

    EXPECT_THROW(eval("(p)"), IronBee::einval);
}

TEST_F(TestStandardDevelopment, identity)
{
    EXPECT_EQ("'foo'", eval("(identity 'foo')"));

    EXPECT_EQ("(identity 'foo')", transform("(identity 'foo')"));

    EXPECT_THROW(eval("(identity)"), IronBee::einval);
    EXPECT_THROW(eval("(identity 'a' 'b')"), IronBee::einval);
}

TEST_F(TestStandardDevelopment, sequence)
{
    typedef IronBee::ConstList<Value> ValueList;

    {
        node_p n = parse("(sequence 1 3)");
        ValueList v;
        size_t index_limit;
        vector<node_cp> traversal;
        bfs_down(n, make_indexer(index_limit, traversal));
        GraphEvalState ges(index_limit);
        bfs_down(n, make_initializer(ges, m_transaction));

        ges.eval(n.get(), m_transaction);
        v = ges.value(n->index()).as_list();
        ASSERT_EQ(1UL, v.size());
        ASSERT_FALSE(ges.is_finished(n->index()));
        ges.eval(n.get(), m_transaction);
        ASSERT_EQ(2UL, v.size());
        ASSERT_FALSE(ges.is_finished(n->index()));
        ges.eval(n.get(), m_transaction);
        ASSERT_EQ(3UL, v.size());
        ASSERT_TRUE(ges.is_finished(n->index()));

        ValueList::const_iterator i = v.begin();
        EXPECT_EQ(1, i->as_number());
        ++i;
        EXPECT_EQ(2, i->as_number());
        ++i;
        EXPECT_EQ(3, i->as_number());
    }

    {
        node_p n = parse("(sequence 3 1 -1)");
        ValueList v;
        size_t index_limit;
        vector<node_cp> traversal;
        bfs_down(n, make_indexer(index_limit, traversal));
        GraphEvalState ges(index_limit);
        bfs_down(n, make_initializer(ges, m_transaction));

        ges.eval(n.get(), m_transaction);
        v = ges.value(n->index()).as_list();
        ASSERT_EQ(1UL, v.size());
        ASSERT_FALSE(ges.is_finished(n->index()));
        ges.eval(n.get(), m_transaction);
        ASSERT_EQ(2UL, v.size());
        ASSERT_FALSE(ges.is_finished(n->index()));
        ges.eval(n.get(), m_transaction);
        ASSERT_EQ(3UL, v.size());
        ASSERT_TRUE(ges.is_finished(n->index()));

        ValueList::const_iterator i = v.begin();
        EXPECT_EQ(3, i->as_number());
        ++i;
        EXPECT_EQ(2, i->as_number());
        ++i;
        EXPECT_EQ(1, i->as_number());
    }

    {
        node_p n = parse("(sequence 1 5 2)");
        ValueList v;
        size_t index_limit;
        vector<node_cp> traversal;
        bfs_down(n, make_indexer(index_limit, traversal));
        GraphEvalState ges(index_limit);
        bfs_down(n, make_initializer(ges, m_transaction));

        ges.eval(n.get(), m_transaction);
        v = ges.value(n->index()).as_list();
        ASSERT_EQ(1UL, v.size());
        ASSERT_FALSE(ges.is_finished(n->index()));
        ges.eval(n.get(), m_transaction);
        ASSERT_EQ(2UL, v.size());
        ASSERT_FALSE(ges.is_finished(n->index()));
        ges.eval(n.get(), m_transaction);
        ASSERT_EQ(3UL, v.size());
        ASSERT_TRUE(ges.is_finished(n->index()));

        ValueList::const_iterator i = v.begin();
        EXPECT_EQ(1, i->as_number());
        ++i;
        EXPECT_EQ(3, i->as_number());
        ++i;
        EXPECT_EQ(5, i->as_number());
    }

    {
        node_p n = parse("(sequence 1)");
        ValueList v;
        size_t index_limit;
        vector<node_cp> traversal;
        bfs_down(n, make_indexer(index_limit, traversal));
        GraphEvalState ges(index_limit);
        bfs_down(n, make_initializer(ges, m_transaction));

        ges.eval(n.get(), m_transaction);
        v = ges.value(n->index()).as_list();
        ASSERT_EQ(1UL, v.size());
        ASSERT_FALSE(ges.is_finished(n->index()));
        ges.eval(n.get(), m_transaction);
        ASSERT_EQ(2UL, v.size());
        ASSERT_FALSE(ges.is_finished(n->index()));
        ges.eval(n.get(), m_transaction);
        ASSERT_EQ(3UL, v.size());
        ASSERT_FALSE(ges.is_finished(n->index()));

        ValueList::const_iterator i = v.begin();
        EXPECT_EQ(1, i->as_number());
        ++i;
        EXPECT_EQ(2, i->as_number());
        ++i;
        EXPECT_EQ(3, i->as_number());
    }

    EXPECT_THROW(eval("(sequence)"), IronBee::einval);
    EXPECT_THROW(eval("(sequence 1 2 3 4)"), IronBee::einval);
    EXPECT_THROW(eval("(sequence 'a')"), IronBee::einval);
    EXPECT_THROW(eval("(sequence 1 'a')"), IronBee::einval);
    EXPECT_THROW(eval("(sequence 1 1 'a')"), IronBee::einval);
}
