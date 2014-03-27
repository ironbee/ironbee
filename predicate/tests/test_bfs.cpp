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
 * @brief Predicate --- BFS Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <predicate/bfs.hpp>
#include <predicate/parse.hpp>
#include "parse_fixture.hpp"

#include "gtest/gtest.h"

using namespace IronBee::Predicate;
using namespace std;

class TestBFS :
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

//! Vector of nodes.
typedef vector<node_p> node_vec_t;
//! Vector of const nodes.
typedef vector<node_cp> node_cvec_t;

TEST_F(TestBFS, DownEasy)
{
    node_p n = parse("(A)");

    {
        node_vec_t r;
        ASSERT_NO_THROW(bfs_down(n, back_inserter(r)));
        ASSERT_EQ(1UL, r.size());
        EXPECT_EQ("(A)", r[0]->to_s());
    }

    {
        node_cvec_t r;
        ASSERT_NO_THROW(bfs_down(node_cp(n), back_inserter(r)));
        ASSERT_EQ(1UL, r.size());
        EXPECT_EQ("(A)", r[0]->to_s());
    }
}

TEST_F(TestBFS, Down)
{
    node_p n = parse("(A (B (C) (C)) (C (B) (B)))");
    node_vec_t r;
    ASSERT_NO_THROW(bfs_down(n, back_inserter(r)));
    ASSERT_EQ(7UL, r.size());
    EXPECT_EQ("(A (B (C) (C)) (C (B) (B)))", r[0]->to_s());
    EXPECT_EQ("(B (C) (C))",                 r[1]->to_s());
    EXPECT_EQ("(C (B) (B))",                 r[2]->to_s());
    EXPECT_EQ("(C)",                         r[3]->to_s());
    EXPECT_EQ("(C)",                         r[4]->to_s());
    EXPECT_EQ("(B)",                         r[5]->to_s());
    EXPECT_EQ("(B)",                         r[6]->to_s());
}

TEST_F(TestBFS, DownWithDups)
{
    node_p n = parse("(A (B (C)) (C (B)))");
    node_p a_b = n->children().front();
    a_b->add_child(a_b->children().front());
    n->add_child(a_b);

    // (A (B (C) (C)) (C (B)) (B (C) (C)))
    //       ^   ^  are same
    //    ^                   ^ are same

    node_vec_t r;
    ASSERT_NO_THROW(bfs_down(n, back_inserter(r)));
    ASSERT_EQ(5UL, r.size());
    EXPECT_EQ("(A (B (C) (C)) (C (B)) (B (C) (C)))", r[0]->to_s());
    EXPECT_EQ("(B (C) (C))",                         r[1]->to_s());
    EXPECT_EQ("(C (B))",                             r[2]->to_s());
    EXPECT_EQ("(C)",                                 r[3]->to_s());
    EXPECT_EQ("(B)",                                 r[4]->to_s());
}

TEST_F(TestBFS, DownError)
{
    node_cvec_t r;
    EXPECT_THROW(bfs_down(node_cp(), back_inserter(r)), IronBee::einval);
}

TEST_F(TestBFS, UpEasy)
{
    node_p n = parse("(A)");

    {
        node_vec_t r;
        ASSERT_NO_THROW(bfs_up(n, back_inserter(r)));
        ASSERT_EQ(1UL, r.size());
        EXPECT_EQ("(A)", r[0]->to_s());
    }

    {
        node_cvec_t r;
        ASSERT_NO_THROW(bfs_up(node_cp(n), back_inserter(r)));
        ASSERT_EQ(1UL, r.size());
        EXPECT_EQ("(A)", r[0]->to_s());
    }
}

TEST_F(TestBFS, Up)
{
    node_p a = parse("(A (B (C)))");
    node_p a_b_c = a->children().front()->children().front();
    node_p n = parse("(C)");
    n->add_child(a_b_c);

    EXPECT_EQ("(C (C))", n->to_s());
    node_vec_t r;
    ASSERT_NO_THROW(bfs_up(a_b_c, back_inserter(r)));
    EXPECT_EQ(4UL, r.size());
    EXPECT_EQ("(C)",         r[0]->to_s());
    EXPECT_EQ("(B (C))",     r[1]->to_s());
    EXPECT_EQ("(C (C))",     r[2]->to_s());
    EXPECT_EQ("(A (B (C)))", r[3]->to_s());
}

TEST_F(TestBFS, UpWithDups)
{
    node_p a = parse("(A (B (C)))");
    node_p a_b_c = a->children().front()->children().front();
    a->add_child(a->children().front());
    EXPECT_EQ("(A (B (C)) (B (C)))", a->to_s());
    // (A (B (C)) (B (C)))
    //    ^       ^ are same
    //       ^       ^ are same

    node_vec_t r;
    ASSERT_NO_THROW(bfs_up(a_b_c, back_inserter(r)));
    EXPECT_EQ(3UL, r.size());
    EXPECT_EQ("(C)",         r[0]->to_s());
    EXPECT_EQ("(B (C))",     r[1]->to_s());
    EXPECT_EQ("(A (B (C)) (B (C)))", r[2]->to_s());
}
