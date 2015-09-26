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
 * @brief Predicate --- Standard List Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbee/predicate/reporter.hpp>
#include <ironbee/predicate/standard_development.hpp>
#include <ironbee/predicate/standard_list.hpp>

#include "standard_test.hpp"

using namespace IronBee::Predicate;
using namespace std;

class TestStandardList :
    public StandardTest
{
protected:
    void SetUp()
    {
        Standard::load_list(factory());
        Standard::load_development(factory());
        factory().add("A", &create);
    }
};

TEST_F(TestStandardList, Name)
{
    EXPECT_EQ("a:'b'", eval("(setName 'a' 'b')"));
    EXPECT_EQ("[a:1 a:2 a:3]", eval("(setName 'a' [1 2 3])"));

    EXPECT_EQ("a:'b'", transform("(setName 'a' 'b')"));
    EXPECT_EQ("[a:1 a:2 a:3]", transform("(setName 'a' [1 2 3])"));

    EXPECT_THROW(eval("(setName)"), IronBee::einval);
    EXPECT_THROW(eval("(setName [] 'a')"), IronBee::einval);
    EXPECT_THROW(eval("(setName 'a')"), IronBee::einval);
    EXPECT_THROW(eval("(setName 'a' 'b' 'c')"), IronBee::einval);
}

TEST_F(TestStandardList, PushName)
{
    EXPECT_EQ("[1 2 3]", eval("(pushName [1 2 3])"));
    EXPECT_EQ("foo:'bar'", eval("(pushName foo:'bar')"));
    EXPECT_EQ(
        "foo:[a:[a:1 a:2] b:[b:3 b:4] c:5]",
        eval("(pushName foo:[a:[x:1 y:2] b:[z:3 w:4] c:5])")
    );
    EXPECT_EQ("[]", eval("(pushName [])"));

    EXPECT_EQ("[1 2 3]", transform("(pushName [1 2 3])"));
    EXPECT_EQ("foo:'bar'", transform("(pushName foo:'bar')"));
    EXPECT_EQ(
        "foo:[a:[a:1 a:2] b:[b:3 b:4] c:5]",
        transform("(pushName foo:[a:[x:1 y:2] b:[z:3 w:4] c:5])")
    );
    EXPECT_EQ("[]", transform("(pushName [])"));

    EXPECT_THROW(eval("(pushName)"), IronBee::einval);
    EXPECT_THROW(eval("(pushName 'a' 'a')"), IronBee::einval);
}

TEST_F(TestStandardList, Cat)
{
    EXPECT_EQ("[1]", eval("(cat 1)"));
    EXPECT_EQ("[1]", eval("(cat [1])"));
    EXPECT_EQ("[1 2 3]", eval("(cat [1 2 3])"));
    EXPECT_EQ("[1 2 3]", eval("(cat 1 2 3)"));
    EXPECT_EQ("[1 2 3 4 5]", eval("(cat 1 2 3 [4 5])"));

    EXPECT_EQ("[1]", transform("(cat 1)"));
    EXPECT_EQ("[1]", transform("(cat [1])"));
    EXPECT_EQ("[1 2 3]", transform("(cat [1 2 3])"));
    EXPECT_EQ("[1 2 3]", transform("(cat 1 2 3)"));
    EXPECT_EQ("[1 2 3 4 5]", transform("(cat 1 2 3 [4 5])"));

    EXPECT_EQ("[]", transform("(cat)"));
    EXPECT_EQ("(cat 1 2 (A))", transform("(cat 1 [] 2 : (A))"));
}

TEST_F(TestStandardList, CatIncremental)
{
    // This test is unfortunately fragile as sequence depends on the number
    // of times its evaluated at that number is dependent on the
    // implementation of Cat.  What we really need is something like sequence
    // but that is externally incremented.
    node_p n = parse("(cat (sequence 0 1) (sequence 0 3))");
    Reporter r;

    size_t index_limit;
    vector<const Node*> traversal;
    bfs_down(n, make_indexer(index_limit, traversal));
    GraphEvalState ges(traversal, index_limit);
    bfs_down(n, make_initializer(ges, m_transaction));

    ges.eval(n.get(), m_transaction);
    EXPECT_EQ("[0]", ges.value(n.get(), m_transaction).to_s());
    EXPECT_FALSE(ges.is_finished(n.get(), m_transaction));
    ges.eval(n.get(), m_transaction);
    EXPECT_EQ("[0 1 0 1]", ges.value(n.get(), m_transaction).to_s());
    EXPECT_FALSE(ges.is_finished(n.get(), m_transaction));
    ges.eval(n.get(), m_transaction);
    EXPECT_EQ("[0 1 0 1 2]", ges.value(n.get(), m_transaction).to_s());
    EXPECT_FALSE(ges.is_finished(n.get(), m_transaction));
    ges.eval(n.get(), m_transaction);
    EXPECT_EQ("[0 1 0 1 2 3]", ges.value(n.get(), m_transaction).to_s());
    EXPECT_TRUE(ges.is_finished(n.get(), m_transaction));
}

TEST_F(TestStandardList, List)
{
    EXPECT_EQ("[1]", eval("(list 1)"));
    EXPECT_EQ("[[1]]", eval("(list [1])"));
    EXPECT_EQ("[[1 2 3]]", eval("(list [1 2 3])"));
    EXPECT_EQ("[1 2 3]", eval("(list 1 2 3)"));
    EXPECT_EQ("[1 2 3 [4 5]]", eval("(list 1 2 3 [4 5])"));

    EXPECT_EQ("[1]", transform("(list 1)"));
    EXPECT_EQ("[[1]]", transform("(list [1])"));
    EXPECT_EQ("[[1 2 3]]", transform("(list [1 2 3])"));
    EXPECT_EQ("[1 2 3]", transform("(list 1 2 3)"));
    EXPECT_EQ("[1 2 3 [4 5]]", transform("(list 1 2 3 [4 5])"));

    EXPECT_EQ("[]", transform("(list)"));
}

TEST_F(TestStandardList, ListIncremental)
{
    node_p n = parse("(list (sequence 0 1) (sequence 0 3))");
    Reporter r;

    size_t index_limit;
    vector<const Node*> traversal;
    bfs_down(n, make_indexer(index_limit, traversal));
    GraphEvalState ges(traversal, index_limit);
    bfs_down(n, make_initializer(ges, m_transaction));

    ges.eval(n.get(), m_transaction);
    EXPECT_EQ("[]", ges.value(n.get(), m_transaction).to_s());
    EXPECT_FALSE(ges.is_finished(n.get(), m_transaction));
    ges.eval(n.get(), m_transaction);
    EXPECT_EQ("[[0 1]]", ges.value(n.get(), m_transaction).to_s());
    EXPECT_FALSE(ges.is_finished(n.get(), m_transaction));
    ges.eval(n.get(), m_transaction);
    EXPECT_EQ("[[0 1]]", ges.value(n.get(), m_transaction).to_s());
    EXPECT_FALSE(ges.is_finished(n.get(), m_transaction));
    ges.eval(n.get(), m_transaction);
    EXPECT_EQ("[[0 1]]", ges.value(n.get(), m_transaction).to_s());
    EXPECT_FALSE(ges.is_finished(n.get(), m_transaction));
    ges.eval(n.get(), m_transaction);
    EXPECT_EQ("[[0 1] [0 1 2 3]]", ges.value(n.get(), m_transaction).to_s());
    EXPECT_TRUE(ges.is_finished(n.get(), m_transaction));
}

TEST_F(TestStandardList, First)
{
    EXPECT_EQ("'a'", eval("(first ['a' 'b' 'c'])"));
    EXPECT_EQ("'a'", eval("(first 'a')"));
    EXPECT_EQ(":", eval("(first :)"));
    EXPECT_EQ(":", eval("(first [])"));

    EXPECT_EQ("'a'", transform("(first ['a' 'b' 'c'])"));
    EXPECT_EQ("'a'", transform("(first 'a')"));
    EXPECT_EQ(":", transform("(first :)"));
    EXPECT_EQ(":", transform("(first [])"));

    EXPECT_THROW(eval("(first)"), IronBee::einval);
    EXPECT_THROW(eval("(first 1 2)"), IronBee::einval);
}

TEST_F(TestStandardList, Rest)
{
    EXPECT_EQ("['b' 'c']", eval("(rest ['a' 'b' 'c'])"));
    EXPECT_EQ("[]", eval("(rest ['a'])"));
    EXPECT_EQ(":", eval("(rest 'a')"));
    EXPECT_EQ(":", eval("(rest :)"));

    EXPECT_EQ("['b' 'c']", transform("(rest ['a' 'b' 'c'])"));
    EXPECT_EQ("[]", transform("(rest ['a'])"));
    EXPECT_EQ(":", transform("(rest 'a')"));
    EXPECT_EQ(":", transform("(rest :)"));

    EXPECT_THROW(eval("(rest)"), IronBee::einval);
    EXPECT_THROW(eval("(rest 1 2)"), IronBee::einval);
}

TEST_F(TestStandardList, Nth)
{
    EXPECT_EQ("'b'", eval("(nth 2 ['a' 'b' 'c'])"));
    EXPECT_EQ(":", eval("(nth 2 ['b'])"));
    EXPECT_EQ(":", eval("(nth 2 'b')"));
    EXPECT_EQ(":", eval("(nth 2 :)"));
    EXPECT_EQ("'b'", eval("(nth 1 'b')"));

    EXPECT_EQ("'b'", transform("(nth 2 ['a' 'b' 'c'])"));
    EXPECT_EQ(":", transform("(nth 2 ['b'])"));
    EXPECT_EQ(":", transform("(nth 2 'b')"));
    EXPECT_EQ(":", transform("(nth 2 :)"));
    EXPECT_EQ("'b'", transform("(nth 1 'b')"));

    EXPECT_THROW(eval("(nth)"), IronBee::einval);
    EXPECT_THROW(eval("(nth 1)"), IronBee::einval);
    EXPECT_THROW(eval("(nth 1 2 3)"), IronBee::einval);
}

TEST_F(TestStandardList, Flatten)
{
    EXPECT_EQ("['a' 'b']", eval("(flatten ['a' 'b'])"));
    EXPECT_EQ("['a' 'b' 'c' 'd']", eval("(flatten [['a' 'b'] ['c' 'd']])"));
    EXPECT_EQ("['a' 'b' 'c']", eval("(flatten [['a' 'b'] 'c'])"));
    EXPECT_EQ("'a'", eval("(flatten 'a')"));
    EXPECT_EQ("[]", eval("(flatten [])"));
    EXPECT_EQ(":", eval("(flatten :)"));

    EXPECT_EQ("['a' 'b']", transform("(flatten ['a' 'b'])"));
    EXPECT_EQ("['a' 'b' 'c' 'd']", transform("(flatten [['a' 'b'] ['c' 'd']])"));
    EXPECT_EQ("['a' 'b' 'c']", transform("(flatten [['a' 'b'] 'c'])"));
    EXPECT_EQ("'a'", transform("(flatten 'a')"));
    EXPECT_EQ("[]", transform("(flatten [])"));
    EXPECT_EQ(":", transform("(flatten :)"));

    EXPECT_THROW(eval("(flatten)"), IronBee::einval);
    EXPECT_THROW(eval("(flatten 1 2)"), IronBee::einval);
}

TEST_F(TestStandardList, Focus)
{
    EXPECT_EQ("[foo:1 bar:4]", eval("(focus 'x' [foo:[x:1 y:2] bar:[y:3 x:4]])"));
    EXPECT_EQ("[foo:1 bar:4]", eval("(focus 'x' [1 foo:[x:1 y:2] 2 bar:[y:3 x:4] 3 baz:[a:1 b:2]])"));
    EXPECT_EQ("[]", eval("(focus 'x' 'y')"));
    EXPECT_EQ("[]", eval("(focus 'x' [1 2 3])"));

    EXPECT_EQ("[foo:1 bar:4]", transform("(focus 'x' [foo:[x:1 y:2] bar:[y:3 x:4]])"));
    EXPECT_EQ("[foo:1 bar:4]", transform("(focus 'x' [1 foo:[x:1 y:2] 2 bar:[y:3 x:4] 3 baz:[a:1 b:2]])"));
    EXPECT_EQ("[]", transform("(focus 'x' 'y')"));
    EXPECT_EQ("[]", transform("(focus 'x' [1 2 3])"));

    EXPECT_THROW(eval("(focus)"), IronBee::einval);
    EXPECT_THROW(eval("(focus 'x')"), IronBee::einval);
    EXPECT_THROW(eval("(focus 1 2)"), IronBee::einval);
    EXPECT_THROW(eval("(focus 'x' 2 3)"), IronBee::einval);
}
