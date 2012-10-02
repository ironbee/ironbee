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
 * @brief IronAutomata --- Optimize Edges test.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironautomata/optimize_edges.hpp>

#include <ironautomata/bits.h>

#include <boost/make_shared.hpp>

#include "gtest/gtest.h"

using namespace std;
using namespace IronAutomata::Intermediate;
using boost::make_shared;

TEST(TestOptimizeEdges, Basic)
{
    node_p node = make_shared<node_t>();
    node_p target_a = make_shared<node_t>();
    node_p target_b = make_shared<node_t>();

    edge_t edge;
    edge.target = target_a;
    edge.values.push_back('c');

    node->edges.push_back(edge);
    edge.values[0] = 'd';
    node->edges.push_back(edge);
    edge.target = target_b;
    node->edges.push_back(edge);

    optimize_edges(node);

    ASSERT_EQ(2UL, node->edges.size());
    const edge_t* to_a = &node->edges.front();
    const edge_t* to_b = &node->edges.back();;
    if (node->edges.front().target == target_b) {
        swap(to_a, to_b);
    }

    ASSERT_EQ(2UL, to_a->values.size());
    ASSERT_EQ(target_a, to_a->target);
    ASSERT_TRUE(to_a->values[0] == 'c' || to_a->values[1] == 'c');
    ASSERT_TRUE(to_a->values[0] == 'd' || to_a->values[1] == 'd');

    ASSERT_EQ(1UL, to_b->values.size());
    ASSERT_EQ(target_b, to_b->target);
    ASSERT_TRUE(to_b->values[0] == 'd');
}

TEST(TestOptimizeEdges, NonDeterministic)
{
    node_p node = make_shared<node_t>();
    node_p target_a = make_shared<node_t>();
    node_p target_b = make_shared<node_t>();

    edge_t edge;
    edge.target = target_a;
    edge.values.push_back('c');

    node->edges.push_back(edge);
    edge.target = target_b;
    node->edges.push_back(edge);

    optimize_edges(node);

    ASSERT_EQ(2UL, node->edges.size());
    const edge_t* to_a = &node->edges.front();
    const edge_t* to_b = &node->edges.back();;
    if (node->edges.front().target == target_b) {
        swap(to_a, to_b);
    }

    ASSERT_EQ(1UL, to_a->values.size());
    ASSERT_EQ(target_a, to_a->target);
    ASSERT_TRUE(to_a->values[0] == 'c');

    ASSERT_EQ(1UL, to_b->values.size());
    ASSERT_EQ(target_b, to_b->target);
    ASSERT_TRUE(to_b->values[0] == 'c');
}

TEST(TestOptimizeEdges, Bitmap)
{
    node_p node = make_shared<node_t>();
    node_p target_a = make_shared<node_t>();
    node_p target_b = make_shared<node_t>();

    edge_t edge;
    edge.target = target_a;
    edge.values_bm.resize(32, 0);
    ia_setbitv(edge.values_bm.data(), 'c');

    node->edges.push_back(edge);
    edge.values_bm.clear();
    edge.values_bm.resize(32, 0);
    ia_setbitv(edge.values_bm.data(), 'd');
    node->edges.push_back(edge);
    edge.target = target_b;
    node->edges.push_back(edge);

    optimize_edges(node);

    ASSERT_EQ(2UL, node->edges.size());
    const edge_t* to_a = &node->edges.front();
    const edge_t* to_b = &node->edges.back();;
    if (node->edges.front().target == target_b) {
        swap(to_a, to_b);
    }

    ASSERT_EQ(2UL, to_a->values.size());
    ASSERT_EQ(target_a, to_a->target);
    ASSERT_TRUE(to_a->values[0] == 'c' || to_a->values[1] == 'c');
    ASSERT_TRUE(to_a->values[0] == 'd' || to_a->values[1] == 'd');

    ASSERT_EQ(1UL, to_b->values.size());
    ASSERT_EQ(target_b, to_b->target);
    ASSERT_TRUE(to_b->values[0] == 'd');
}

TEST(TestOptimizeEdges, ManyValues)
{
    node_p node = make_shared<node_t>();
    node_p target_a = make_shared<node_t>();

    edge_t edge;
    edge.target = target_a;
    for (uint8_t i = 0; i < 200; ++i) {
        edge.values.push_back(i);
    }
    node->edges.push_back(edge);

    optimize_edges(node);

    ASSERT_EQ(1UL, node->edges.size());
    const edge_t* to_a = &node->edges.front();

    ASSERT_TRUE(to_a->values.empty());
    ASSERT_EQ(32UL, to_a->values_bm.size());
    for (uint8_t i = 0; i < 200; ++i) {
        ASSERT_TRUE(ia_bitv(to_a->values_bm.data(), i));
    }
}

TEST(TestOptimizeEdges, ThirtyTwo)
{
    node_p node = make_shared<node_t>();
    node_p target_a = make_shared<node_t>();

    edge_t edge;
    edge.target = target_a;
    for (uint8_t i = 0; i < 32; ++i) {
        edge.values.push_back(i);
    }
    node->edges.push_back(edge);

    optimize_edges(node);

    ASSERT_EQ(1UL, node->edges.size());
    const edge_t* to_a = &node->edges.front();

    ASSERT_TRUE(to_a->values.empty());
    ASSERT_EQ(32UL, to_a->values_bm.size());
    for (uint8_t i = 0; i < 32; ++i) {
        ASSERT_TRUE(ia_bitv(to_a->values_bm.data(), i));
    }
}

TEST(TestOptimizeEdges, ThirtyOne)
{
    node_p node = make_shared<node_t>();
    node_p target_a = make_shared<node_t>();

    edge_t edge;
    edge.target = target_a;
    for (uint8_t i = 0; i < 31; ++i) {
        edge.values.push_back(i);
    }
    node->edges.push_back(edge);

    optimize_edges(node);

    ASSERT_EQ(1UL, node->edges.size());
    const edge_t* to_a = &node->edges.front();

    ASSERT_TRUE(to_a->values_bm.empty());
    ASSERT_EQ(31UL, to_a->values.size());
}


TEST(TestOptimizeEdges, Advance)
{
    node_p node = make_shared<node_t>();
    node_p target_a = make_shared<node_t>();

    edge_t edge;
    edge.target = target_a;
    edge.advance = true;
    edge.values.push_back('c');
    node->edges.push_back(edge);
    edge.advance = false;
    node->edges.push_back(edge);

    optimize_edges(node);

    ASSERT_EQ(2UL, node->edges.size());
    const edge_t* advance = &node->edges.front();
    const edge_t* nonadvance = &node->edges.back();;
    if (! advance-> advance) {
        swap(advance, nonadvance);
    }

    ASSERT_EQ(1UL, advance->values.size());
    ASSERT_EQ(target_a, advance->target);
    ASSERT_TRUE(advance->values[0] == 'c');
    ASSERT_TRUE(advance->advance);

    ASSERT_EQ(1UL, nonadvance->values.size());
    ASSERT_EQ(target_a, nonadvance->target);
    ASSERT_TRUE(nonadvance->values[0] == 'c');
    ASSERT_FALSE(nonadvance->advance);
}

TEST(TestOptimizeEdges, Epsilon)
{
    node_p node = make_shared<node_t>();
    node_p target_a = make_shared<node_t>();
    node_p target_b = make_shared<node_t>();

    edge_t edge;
    edge.target = target_a;
    node->edges.push_back(edge);
    edge.values.push_back('c');
    node->edges.push_back(edge);

    optimize_edges(node);

    ASSERT_EQ(2UL, node->edges.size());
    const edge_t* c = &node->edges.front();
    const edge_t* epsilon = &node->edges.back();;
    if (c->values.empty()) {
        swap(c, epsilon);
    }

    ASSERT_EQ(1UL, c->values.size());
    ASSERT_EQ(target_a, c->target);
    ASSERT_TRUE(c->values[0] == 'c');

    ASSERT_TRUE(epsilon->values.empty());
    ASSERT_TRUE(epsilon->values_bm.empty());
    ASSERT_EQ(target_a, epsilon->target);
}
