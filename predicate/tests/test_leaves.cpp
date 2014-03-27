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
 * @brief Predicate --- Leave Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/leaves.hpp>
#include <predicate/merge_graph.hpp>
#include "parse_fixture.hpp"

#include "gtest/gtest.h"

using namespace IronBee::Predicate;
using namespace std;

class TestLeaves :
    public ::testing::Test,
    public ParseFixture
{
protected:
    virtual void SetUp()
    {
        factory()
            .add("A", &create)
            .add("B", &create)
            .add("C", &create)
            ;
    }
};

TEST_F(TestLeaves, Tree)
{
    node_p tree1 = parse("(A (A 'B') (A 'C') (A (B (A) (B))))");
    node_p tree2 = parse("(A 'D')");
    vector<node_p> roots;
    vector<node_p> leaves;

    roots.push_back(tree1);
    roots.push_back(tree2);
    find_leaves(roots.begin(), roots.end(), back_inserter(leaves));

    ASSERT_EQ(5UL, leaves.size());
    EXPECT_EQ("'D'", leaves[0]->to_s());
    EXPECT_EQ("'B'", leaves[1]->to_s());
    EXPECT_EQ("'C'", leaves[2]->to_s());
    EXPECT_EQ("(A)", leaves[3]->to_s());
    EXPECT_EQ("(B)", leaves[4]->to_s());
}

TEST_F(TestLeaves, Graph)
{
    node_p tree1 = parse("(A (A 'B') (A 'C') (A (B 'B' 'D')))");
    node_p tree2 = parse("(A 'B')");
    MergeGraph g;
    vector<node_p> leaves;

    g.add_root(tree1);
    g.add_root(tree2);
    find_leaves(g.roots().first, g.roots().second, back_inserter(leaves));

    ASSERT_EQ(3UL, leaves.size());
    EXPECT_EQ("'B'", leaves[0]->to_s());
    EXPECT_EQ("'C'", leaves[1]->to_s());
    EXPECT_EQ("'D'", leaves[2]->to_s());
}
