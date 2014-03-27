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

#include <predicate/merge_graph.hpp>
#include "parse_fixture.hpp"

#include "gtest/gtest.h"

using namespace IronBee::Predicate;
using namespace std;

class TestMergeGraph :
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

    size_t num_descendants(const node_cp& node) const
    {
        vector<node_cp> r;
        bfs_down(node, back_inserter(r));
        return r.size();
    }
};

TEST_F(TestMergeGraph, Easy)
{
    node_p n = parse("(A (B (C)))");
    MergeGraph g;
    size_t n_i = 0;

    EXPECT_TRUE(g.empty());
    EXPECT_NO_THROW(n_i = g.add_root(n));
    EXPECT_EQ(n, g.root(n_i));
    EXPECT_EQ(1UL, g.root_indices(n).size());
    EXPECT_EQ(n_i, *g.root_indices(n).begin());
    EXPECT_FALSE(g.empty());

    EXPECT_TRUE(g.write_validation_report(cerr));
}

TEST_F(TestMergeGraph, Basic)
{
    node_p n = parse("(A (B (C)) (B (C)))");
    MergeGraph g;
    size_t n_i = 0;

    EXPECT_NO_THROW(n_i = g.add_root(n));
    EXPECT_EQ(n, g.root(n_i));
    EXPECT_EQ(1UL, g.root_indices(n).size());
    EXPECT_EQ(n_i, *g.root_indices(n).begin());

    EXPECT_EQ(3UL, num_descendants(n));

    EXPECT_TRUE(g.write_validation_report(cerr));
}

TEST_F(TestMergeGraph, MultipleRoots)
{
    MergeGraph g;
    node_p n = parse("(A (B (C)) (B (C)))");
    size_t n_i = 0;
    node_p m = parse("(C (B (C)))");
    size_t m_i = 0;

    EXPECT_NO_THROW(n_i = g.add_root(n));
    EXPECT_NO_THROW(m_i = g.add_root(m));
    EXPECT_EQ(n, g.root(n_i));
    EXPECT_EQ(1UL, g.root_indices(n).size());
    EXPECT_EQ(n_i, *g.root_indices(n).begin());
    EXPECT_EQ(m, g.root(m_i));
    EXPECT_EQ(1UL, g.root_indices(m).size());
    EXPECT_EQ(m_i, *g.root_indices(m).begin());

    EXPECT_EQ(2UL, g.size());
    EXPECT_EQ(n, *g.roots().first);
    EXPECT_EQ(m, *boost::next(g.roots().first));

    EXPECT_TRUE(g.write_validation_report(cerr));
}

TEST_F(TestMergeGraph, KnownRoot)
{
    MergeGraph g;
    node_p n = parse("(A (B (C)) (B (C)))");
    node_p m = parse("(B (C))");
    node_p backup_m = m;

    EXPECT_NO_THROW(g.add_root(n));
    EXPECT_NO_THROW(g.add_root(m));
    EXPECT_NE(backup_m, m);

    EXPECT_TRUE(g.write_validation_report(cerr));
}

TEST_F(TestMergeGraph, Replace)
{
    MergeGraph g;
    node_p n = parse("(A (B (C)))");
    node_p m = parse("(B (C))");
    node_p m2 = parse("(A)");
    size_t n_i = 0;
    size_t m_i = 0;

    EXPECT_NO_THROW(n_i = g.add_root(n));
    EXPECT_NO_THROW(m_i = g.add_root(m));

    EXPECT_NO_THROW(g.replace(m, m2));
    EXPECT_EQ("(A (A))", g.root(n_i)->to_s());
    EXPECT_EQ("(A)",     g.root(m_i)->to_s());

    EXPECT_TRUE(g.write_validation_report(cerr));

    EXPECT_EQ(m2, g.find_transform(m));
    g.clear_transform_record();
    EXPECT_THROW(g.find_transform(m), IronBee::enoent);
}

TEST_F(TestMergeGraph, DoubleReplace)
{
    MergeGraph g;
    node_p n = parse("(A (B (C)))");
    node_p m = parse("(B (C))");
    node_p m2 = parse("(A)");
    node_p m3 = parse("(B)");
    size_t n_i = 0;
    size_t m_i = 0;

    EXPECT_NO_THROW(n_i = g.add_root(n));
    EXPECT_NO_THROW(m_i = g.add_root(m));

    EXPECT_NO_THROW(g.replace(m, m2));
    EXPECT_NO_THROW(g.replace(m2, m3));

    EXPECT_EQ("(A (B))", g.root(n_i)->to_s());
    EXPECT_EQ("(B)",     g.root(m_i)->to_s());

    EXPECT_TRUE(g.write_validation_report(cerr));

    EXPECT_EQ(m3, g.find_transform(m));
    g.clear_transform_record();
    EXPECT_THROW(g.find_transform(m), IronBee::enoent);
}

TEST_F(TestMergeGraph, ReplaceLoop)
{
    MergeGraph g;
    node_p n = parse("(A (B (C)))");
    node_p m = parse("(B (C))");
    node_p m2 = parse("(A (B (C)))");
    size_t n_i = 0;
    size_t m_i = 0;

    EXPECT_NO_THROW(n_i = g.add_root(n));
    EXPECT_NO_THROW(m_i = g.add_root(m));

    EXPECT_TRUE(g.write_validation_report(cerr));
    EXPECT_NO_THROW(g.replace(m, m2));
    EXPECT_EQ("(A (A (B (C))))", g.root(n_i)->to_s());
    EXPECT_EQ("(A (B (C)))",     g.root(m_i)->to_s());

    EXPECT_TRUE(g.write_validation_report(cerr));
    EXPECT_EQ(m2, g.find_transform(m));
}

TEST_F(TestMergeGraph, Add)
{
    MergeGraph g;
    node_p n = parse("(A (B (C)))");
    node_p m = parse("(B (C))");
    node_p o = parse("(A)");
    size_t n_i = 0;
    size_t m_i = 0;

    EXPECT_NO_THROW(n_i = g.add_root(n));
    EXPECT_NO_THROW(m_i = g.add_root(m));

    EXPECT_NO_THROW(g.add(m, o));
    EXPECT_EQ("(A (B (C) (A)))", g.root(n_i)->to_s());
    EXPECT_EQ("(B (C) (A))",    g.root(m_i)->to_s());

    EXPECT_TRUE(g.write_validation_report(cerr));
}

TEST_F(TestMergeGraph, AddLoop)
{
    MergeGraph g;
    node_p n = parse("(A (B (C)))");
    node_p m = parse("(B (C))");
    node_p o = parse("(B (C))");
    size_t n_i = 0;
    size_t m_i = 0;

    EXPECT_NO_THROW(n_i = g.add_root(n));
    EXPECT_NO_THROW(m_i = g.add_root(m));

    EXPECT_NO_THROW(g.add(m, o));
    EXPECT_EQ("(A (B (C) (B (C))))", g.root(n_i)->to_s());
    EXPECT_EQ("(B (C) (B (C)))",    g.root(m_i)->to_s());

    EXPECT_TRUE(g.write_validation_report(cerr));
}

TEST_F(TestMergeGraph, Remove)
{
    MergeGraph g;
    node_p n = parse("(A (B (C)))");
    node_p m = parse("(B (C))");
    size_t n_i = 0;
    size_t m_i = 0;

    EXPECT_NO_THROW(n_i = g.add_root(n));
    EXPECT_NO_THROW(m_i = g.add_root(m));

    node_p to_remove = parse("(C)");
    EXPECT_NO_THROW(g.remove(m, to_remove));
    EXPECT_EQ("(A (B))", g.root(n_i)->to_s());
    EXPECT_EQ("(B)",    g.root(m_i)->to_s());

    EXPECT_TRUE(g.write_validation_report(cerr));

    EXPECT_NO_THROW(g.remove(n, m));
    EXPECT_EQ(2UL, g.size());
    EXPECT_EQ("(A)", g.root(n_i)->to_s());
    EXPECT_EQ("(B)", g.root(m_i)->to_s());

    EXPECT_TRUE(g.write_validation_report(cerr));

    EXPECT_FALSE(g.find_transform(to_remove));
}
