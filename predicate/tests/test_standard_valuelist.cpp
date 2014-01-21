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
 * @brief Predicate --- Standard ValueList Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/reporter.hpp>
#include <predicate/standard_valuelist.hpp>
#include "standard_test.hpp"

using namespace IronBee::Predicate;
using namespace std;

class TestStandardValueList :
    public StandardTest
{
};

TEST_F(TestStandardValueList, Name)
{
    EXPECT_TRUE(eval_bool(parse("(setName 'a' 'b')")));
    EXPECT_EQ("b", eval_s(parse("(setName 'a' 'b')")));
    EXPECT_THROW(eval_bool(parse("(setName)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(setName null 'a')")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(setName 'a')")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(setName 'a' 'b' 'c')")), IronBee::einval);
}

TEST_F(TestStandardValueList, CatFirstRest)
{
    EXPECT_EQ("a", eval_s(parse("(first 'a')")));
    EXPECT_EQ("a", eval_s(parse("(first (cat 'a'))")));
    EXPECT_EQ("a", eval_s(parse("(first (cat 'a' 'b'))")));
    EXPECT_EQ("b", eval_s(parse("(first (rest (cat 'a' 'b')))")));
    EXPECT_EQ("b", eval_s(parse("(first (rest (cat 'a' 'b' 'c')))")));
    EXPECT_EQ("b", eval_s(parse("(first (rest (cat 'a' (cat 'b' 'c'))))")));

    EXPECT_THROW(eval_s(parse("(first 'a' 'b')")), IronBee::einval);
    EXPECT_THROW(eval_s(parse("(first)")), IronBee::einval);
    EXPECT_THROW(eval_s(parse("(rest 'a' 'b')")), IronBee::einval);
    EXPECT_THROW(eval_s(parse("(rest)")), IronBee::einval);
}

TEST_F(TestStandardValueList, CatTransform)
{
    EXPECT_EQ("'a'", transform("(cat 'a')"));
    EXPECT_EQ("null", transform("(cat)"));
    EXPECT_EQ("(cat 'a' 'b' 'c')", transform("(cat 'a' null 'b' null 'c')"));
}

TEST_F(TestStandardValueList, CatIncremental)
{
    // This test is unfortunately fragile as sequence depends on the number
    // of times its evaluated at that number is dependent on the
    // implementation of Cat.  What we really need is something like sequence
    // but that is externally incremented.
    node_p n = parse("(cat (sequence 0 1) (sequence 0 3))");
    Reporter r;

    size_t index_limit;
    bfs_down(n, make_indexer(index_limit));
    GraphEvalState ges(index_limit);
    bfs_down(n, make_initializer(ges, m_transaction));

    ges.eval(n, m_transaction);
    EXPECT_EQ(1UL, ges.values(n->index()).size());
    EXPECT_FALSE(ges.is_finished(n->index()));
    ges.eval(n, m_transaction);
    EXPECT_EQ(4UL, ges.values(n->index()).size());
    EXPECT_FALSE(ges.is_finished(n->index()));
    ges.eval(n, m_transaction);
    EXPECT_EQ(5UL, ges.values(n->index()).size());
    EXPECT_FALSE(ges.is_finished(n->index()));
    ges.eval(n, m_transaction);
    EXPECT_EQ(6UL, ges.values(n->index()).size());
    EXPECT_TRUE(ges.is_finished(n->index()));
    ges.eval(n, m_transaction);
}

TEST_F(TestStandardValueList, Nth)
{
    EXPECT_EQ("a", eval_s(parse("(nth 1 'a')")));
    EXPECT_EQ("a", eval_s(parse("(nth 1 (cat 'a' 'b' 'c'))")));
    EXPECT_EQ("b", eval_s(parse("(nth 2 (cat 'a' 'b' 'c'))")));
    EXPECT_EQ("c", eval_s(parse("(nth 3 (cat 'a' 'b' 'c'))")));
    EXPECT_FALSE(eval_bool(parse("(nth 0 (cat 'a' 'b' 'c'))")));

    EXPECT_THROW(eval_bool(parse("(nth -3 (cat 'a' 'b' 'c'))")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(nth)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(nth 1)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(nth 'a' 'b')")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(nth 1 'a' 'b')")), IronBee::einval);
}

TEST_F(TestStandardValueList, ScatterGather)
{
    EXPECT_EQ("a", eval_s(parse("(first (scatter (gather (cat 'a' 'b'))))")));
    EXPECT_EQ("b", eval_s(parse("(rest (scatter (gather (cat 'a' 'b'))))")));

    EXPECT_THROW(eval_bool(parse("(scatter)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(scatter 'a' 'b')")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(gather)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(gather 'a' 'b')")), IronBee::einval);
}

TEST_F(TestStandardValueList, Flatten)
{
    EXPECT_EQ("[a b]", eval_l(parse("(flatten (cat 'a' 'b'))")));
    EXPECT_EQ("[a b c d]", eval_l(parse("(flatten (cat (cat 'a' 'b') (cat 'c' 'd')))")));
    EXPECT_EQ("[a b c]", eval_l(parse("(flatten (cat (cat 'a' 'b') 'c'))")));
    EXPECT_EQ("[a]", eval_l(parse("(flatten 'a')")));
    EXPECT_EQ("[]", eval_l(parse("(flatten null)")));

    EXPECT_THROW(eval_l(parse("(flatten)")), IronBee::einval);
    EXPECT_THROW(eval_l(parse("(flatten 'a' 'b')")), IronBee::einval);

    EXPECT_EQ("[1:a 1:b]", eval_l(parse("(namedi '1' (flatten (cat (cat (setName '1' 'a') (setName '2' 'foo')) (cat (setName '1' 'b') (setName '2' 'bar')))))")));
}

TEST_F(TestStandardValueList, Focus)
{
    EXPECT_EQ("[foo:1 bar:4]", eval_l(parse("(focus 'x' (cat (setName 'foo' (gather (cat (setName 'x' 1) (setName 'y' 2)))) (setName 'bar' (gather (cat (setName 'y' 3) (setName 'x' 4))))))")));

    EXPECT_THROW(eval_bool(parse("(setName)")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(setName null 'a')")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(setName 'a')")), IronBee::einval);
    EXPECT_THROW(eval_bool(parse("(setName 'a' 'b' 'c')")), IronBee::einval);
}
