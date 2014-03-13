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
 * @brief IronAutomata --- Intermediate test.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironautomata/intermediate.hpp>
#include <ironautomata/bits.h>

#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include <algorithm>
#include <sstream>

#include <arpa/inet.h>

#include "gtest/gtest.h"

using namespace std;
using namespace IronAutomata::Intermediate;

namespace {

template <typename A, typename B>
bool equal(const A& a, const B& b)
{
    return equal(a.begin(), a.end(), b.begin());
}

}

// Edge

TEST(TestIntermediate, EdgeConstructors)
{
    Edge edge;

    EXPECT_FALSE(edge.target());
    EXPECT_TRUE(edge.advance());
    EXPECT_TRUE(edge.empty());

    node_p target_a = boost::make_shared<Node>();
    node_p target_b = boost::make_shared<Node>();

    edge = Edge(target_a, false);
    EXPECT_EQ(target_a, edge.target());
    EXPECT_FALSE(edge.advance());
    EXPECT_TRUE(edge.empty());

    byte_vector_t bytes;
    bytes.push_back('a');
    bytes.push_back('b');
    bytes.push_back('c');

    edge = Edge::make_from_vector(target_b, true, bytes);
    EXPECT_EQ(target_b, edge.target());
    EXPECT_TRUE(edge.advance());
    EXPECT_EQ(3UL, edge.size());
    EXPECT_TRUE(edge.has_value('a'));
    EXPECT_TRUE(edge.has_value('b'));
    EXPECT_TRUE(edge.has_value('c'));

    bytes.clear();
    bytes.resize(32, 0);
    ia_setbitv(bytes.data(), 'd');
    ia_setbitv(bytes.data(), 'e');
    ia_setbitv(bytes.data(), 'f');
    edge = Edge::make_from_bitmap(target_a, false, bytes);
    EXPECT_EQ(target_a, edge.target());
    EXPECT_FALSE(edge.advance());
    EXPECT_EQ(3UL, edge.size());
    EXPECT_TRUE(edge.has_value('d'));
    EXPECT_TRUE(edge.has_value('e'));
    EXPECT_TRUE(edge.has_value('f'));
}

TEST(TestIntermediate, EdgeBitmapIterator)
{
    byte_vector_t bitmap(32, 0);
    for (int i = 0; i <= 36; ++i ) {
        ia_setbitv(bitmap.data(), 7 * i);
    }
    Edge edge = Edge::make_from_bitmap(node_p(), false, bitmap);
    vector<uint8_t> result;
    copy(edge.begin(), edge.end(), back_inserter(result));
    ASSERT_EQ(37UL, result.size());
    for (int i = 0; i <= 36; ++i) {
        EXPECT_EQ(result[i], 7 * i);
    }
}

TEST(TestIntermediate, EdgeVectorIterator)
{
    byte_vector_t values;
    for (int i = 0; i <= 36; ++i ) {
        values.push_back(7 * i);
    }
    Edge edge = Edge::make_from_vector(node_p(), false, values);

    vector<uint8_t> result;
    copy(edge.begin(), edge.end(), back_inserter(result));
    ASSERT_EQ(37UL, result.size());
    for (int i = 0; i <= 36; ++i) {
        EXPECT_EQ(result[i], 7 * i);
    }
}

TEST(TestIntermediate, EdgeAddRemove)
{
    Edge edge;

    edge.add('a');
    EXPECT_EQ(1UL, edge.size());
    EXPECT_TRUE(edge.bitmap().empty());
    EXPECT_TRUE(edge.has_value('a'));
    edge.add('b');
    EXPECT_EQ(2UL, edge.size());
    EXPECT_TRUE(edge.bitmap().empty());
    EXPECT_TRUE(edge.has_value('b'));

    edge.clear();

    for (uint8_t c = 0; c < 200; c += 3) {
        edge.add(c);
    }

    for (uint8_t c = 0; c < 200; ++c) {
        EXPECT_EQ(c % 3 == 0, edge.has_value(c));
    }

    edge.remove(21);
    EXPECT_FALSE(edge.has_value(21));
}

TEST(TestIntermediate, EdgeSwitch)
{
    Edge edge;
    edge.add('a');
    edge.add('d');
    edge.add('g');

    edge.switch_to_bitmap();
    ASSERT_EQ(32UL, edge.bitmap().size());
    EXPECT_TRUE(edge.vector().empty());

    EXPECT_TRUE(edge.has_value('a'));
    EXPECT_TRUE(edge.has_value('d'));
    EXPECT_TRUE(edge.has_value('g'));
    EXPECT_FALSE(edge.has_value('h'));

    edge.switch_to_vector();
    ASSERT_EQ(3UL, edge.vector().size());
    EXPECT_TRUE(edge.bitmap().empty());
    EXPECT_TRUE(edge.has_value('a'));
    EXPECT_TRUE(edge.has_value('d'));
    EXPECT_TRUE(edge.has_value('g'));
    EXPECT_FALSE(edge.has_value('h'));
}

TEST(TestIntermediate, EdgeMatches)
{
    Edge edge;

    EXPECT_TRUE(edge.matches('a'));

    edge.add('b');
    EXPECT_FALSE(edge.matches('a'));
    EXPECT_TRUE(edge.matches('b'));
    edge.add('c');
    EXPECT_TRUE(edge.matches('b'));
    EXPECT_TRUE(edge.matches('c'));

    edge.switch_to_bitmap();
    EXPECT_TRUE(edge.matches('b'));
    EXPECT_TRUE(edge.matches('c'));
}

// Node

TEST(TestIntermediate, NodeConstructor)
{
    Node node;
    EXPECT_TRUE(node.advance_on_default());
    EXPECT_FALSE(node.first_output());
    EXPECT_FALSE(node.default_target());
    EXPECT_TRUE(node.edges().empty());
    node = Node(false);
    EXPECT_FALSE(node.advance_on_default());
}

TEST(TestIntermediate, NodeEdges)
{
    Node node;
    node_p target_a = boost::make_shared<Node>();
    node_p target_a2 = boost::make_shared<Node>();
    node_p target_b = boost::make_shared<Node>();
    byte_vector_t values;
    values.push_back('a');
    node.edges().push_back(Edge::make_from_vector(target_a, false, values));
    node.edges().push_back(Edge::make_from_vector(target_a2, false, values));
    values[0] = 'b';
    node.edges().push_back(Edge::make_from_vector(target_b, false, values));

    Node::edge_list_t edges = node.edges_for('a');
    EXPECT_EQ(2UL, edges.size());
    EXPECT_TRUE(edges.front().matches('a'));
    EXPECT_EQ(1UL, edges.front().size());
    edges = node.edges_for('b');
    EXPECT_EQ(1UL, edges.size());
    EXPECT_EQ(target_b, edges.front().target());
    EXPECT_TRUE(edges.front().matches('b'));
    EXPECT_EQ(1UL, edges.front().size());

    node_p target_default = boost::make_shared<Node>();

    node.default_target() = target_default;

    Node::target_info_list_t targets;

    targets = node.targets_for('a');
    EXPECT_EQ(2UL, targets.size());
    targets = node.targets_for('b');
    EXPECT_EQ(1UL, targets.size());
    targets = node.targets_for('c');
    EXPECT_EQ(1UL, targets.size());
    EXPECT_EQ(target_default, targets.front().first);

    node_p target_epsilon = boost::make_shared<Node>();
    node.edges().push_back(Edge(target_epsilon));
    targets = node.targets_for('a');
    EXPECT_EQ(3UL, targets.size());
    targets = node.targets_for('b');
    EXPECT_EQ(2UL, targets.size());
    targets = node.targets_for('c');
    EXPECT_EQ(1UL, targets.size());
}

TEST(TestIntermediate, BuildTargetsByInput)
{
    Node node;
    node_p target_a = boost::make_shared<Node>();
    node_p target_b = boost::make_shared<Node>();
    node_p target_b2 = boost::make_shared<Node>();
    node_p target_other = boost::make_shared<Node>();
    node.edges().push_back(Edge::make_from_vector(target_a, false, byte_vector_t(1, 'a')));
    node.edges().push_back(Edge::make_from_vector(target_b, false, byte_vector_t(1, 'b')));
    node.edges().push_back(Edge::make_from_vector(target_b2, false, byte_vector_t(1, 'b')));
    node.default_target() = target_other;

    Node::targets_by_input_t targets = node.build_targets_by_input();
    EXPECT_TRUE(equal(node.targets_for('a'), targets['a']));
    EXPECT_TRUE(equal(node.targets_for('b'), targets['b']));
    EXPECT_TRUE(equal(node.targets_for('c'), targets['c']));

    node.clear();
    node.edges().push_back(Edge::make_from_vector(target_a, false, byte_vector_t(1, 'a')));
    node.edges().push_back(Edge(target_a));
    targets = node.build_targets_by_input();
    EXPECT_TRUE(equal(node.targets_for('a'), targets['a']));
    EXPECT_TRUE(equal(node.targets_for('b'), targets['b']));
}

// Reader

TEST(TestIntermediate, ReaderBasic)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        PB::Edge* pb_edge_ab = pb_node_a->add_edges();
        pb_edge_ab->set_target(2);
        pb_edge_ab->set_values("a");
        PB::Node* pb_node_b = pb_chunk.add_nodes();
        pb_node_b->set_id(2);

        write_chunk(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_TRUE(reader.clean());
    EXPECT_TRUE(reader.success());

    const Automata& automata = reader.automata();
    EXPECT_FALSE(automata.no_advance_no_output());
    ASSERT_TRUE(bool(automata.start_node()));
    const Node& node_a = *automata.start_node();
    EXPECT_FALSE(node_a.default_target());
    EXPECT_TRUE(node_a.advance_on_default());
    EXPECT_FALSE(node_a.first_output());
    EXPECT_EQ(1UL, node_a.edges().size());
    const Edge& edge_ab = *node_a.edges().begin();
    ASSERT_TRUE(bool(edge_ab.target()));
    EXPECT_TRUE(edge_ab.advance());
    EXPECT_EQ(1U, edge_ab.size());
    EXPECT_EQ('a', *edge_ab.begin());
    const Node& node_b = *edge_ab.target();
    EXPECT_FALSE(node_b.first_output());
    EXPECT_FALSE(node_b.default_target());
    EXPECT_TRUE(node_b.advance_on_default());
    EXPECT_TRUE(node_b.edges().empty());
}

TEST(TestIntermediate, ReaderTrivial)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;
        write_chunk(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_TRUE(reader.clean());
    EXPECT_TRUE(reader.success());

    const Automata& automata = reader.automata();
    EXPECT_FALSE(automata.no_advance_no_output());
    EXPECT_FALSE(automata.start_node());
}

TEST(TestIntermediate, ReaderEmptyInput)
{
    stringstream s;

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.automata().start_node());
}

TEST(TestIntermediate, ReaderInvalidSize)
{
    stringstream s;

    uint32_t nsize = htonl(123);
    s.write(
        reinterpret_cast<const char*>(&nsize), sizeof(uint32_t)
    );
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_FALSE(success);
}

TEST(TestIntermediate, ReaderInvalidChunk)
{
    static const char junk[] = "Hello World";
    stringstream s;

    uint32_t nsize = htonl(sizeof(junk));
    s.write(
        reinterpret_cast<const char*>(&nsize), sizeof(uint32_t)
    );
    s.write(junk, sizeof(junk));
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_FALSE(success);
}

TEST(TestIntermediate, ReaderDuplicateOutput)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Output* pb_output_a = pb_chunk.add_outputs();
        pb_output_a->set_id(1);
        pb_output_a->set_content("foo");
        PB::Output* pb_output_b = pb_chunk.add_outputs();
        pb_output_b->set_id(1);
        pb_output_b->set_content("bar");
        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        pb_node_a->set_first_output(1);

        write_chunk(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_TRUE(reader.success());
}

TEST(TestIntermediate, ReaderDuplicateNode)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        PB::Node* pb_node_b = pb_chunk.add_nodes();
        pb_node_b->set_id(1);

        write_chunk(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_TRUE(reader.success());
}

TEST(TestIntermediate, ReaderTooValuedEdge)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        PB::Edge* pb_edge_ab = pb_node_a->add_edges();
        pb_edge_ab->set_target(2);
        pb_edge_ab->set_values("a");
        pb_edge_ab->set_values_bm("abcdabcdabcdabcdabcdabcdabcdabcd");
        PB::Node* pb_node_b = pb_chunk.add_nodes();
        pb_node_b->set_id(2);

        write_chunk(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_TRUE(reader.success());
}

TEST(TestIntermediate, ReaderBadValuesBitmap)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        PB::Edge* pb_edge_ab = pb_node_a->add_edges();
        pb_edge_ab->set_target(2);
        pb_edge_ab->set_values_bm("ab");
        PB::Node* pb_node_b = pb_chunk.add_nodes();
        pb_node_b->set_id(2);

        write_chunk(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_TRUE(reader.success());
}

TEST(TestIntermediate, ReaderBitMapEdge)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        PB::Edge* pb_edge_ab = pb_node_a->add_edges();
        pb_edge_ab->set_target(2);
        pb_edge_ab->set_values_bm("abcdabcdabcdabcdabcdabcdabcdabcd");
        PB::Node* pb_node_b = pb_chunk.add_nodes();
        pb_node_b->set_id(2);

        write_chunk(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_TRUE(reader.clean());
    EXPECT_TRUE(reader.success());

    const Automata& automata = reader.automata();
    ASSERT_TRUE(bool(automata.start_node()));
    const Node& node_a = *automata.start_node();
    ASSERT_EQ(1UL, node_a.edges().size());
    const Edge& edge_ab = *node_a.edges().begin();
    EXPECT_EQ(104UL, edge_ab.size());
}

TEST(TestIntermediate, ReaderEpsilonEdge)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        PB::Edge* pb_edge_ab = pb_node_a->add_edges();
        pb_edge_ab->set_target(2);
        PB::Node* pb_node_b = pb_chunk.add_nodes();
        pb_node_b->set_id(2);

        write_chunk(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_TRUE(reader.clean());
    EXPECT_TRUE(reader.success());

    const Automata& automata = reader.automata();
    ASSERT_TRUE(bool(automata.start_node()));
    const Node& node_a = *automata.start_node();
    ASSERT_EQ(1UL, node_a.edges().size());
    const Edge& edge_ab = *node_a.edges().begin();
    EXPECT_TRUE(edge_ab.empty());
}

TEST(TestIntermediate, ReaderMissingNode)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        PB::Edge* pb_edge_ab = pb_node_a->add_edges();
        pb_edge_ab->set_target(2);

        write_chunk(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_FALSE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_FALSE(reader.success());
}

TEST(TestIntermediate, ReaderExcessNode)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        PB::Node* pb_node_b = pb_chunk.add_nodes();
        pb_node_b->set_id(2);

        write_chunk(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_TRUE(reader.success());
}

TEST(TestIntermediate, ReaderMissingOutput)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        pb_node_a->set_first_output(2);

        write_chunk(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_FALSE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_FALSE(reader.success());
}

TEST(TestIntermediate, ReaderExcessOutput)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Output* pb_output = pb_chunk.add_outputs();
        pb_output->set_id(1);
        pb_output->set_content("content");

        write_chunk(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_TRUE(reader.success());
}

// Writer

// Very basic test; more significant testing will be done by end to end tests.
TEST(TestIntermediate, Writer)
{
    using namespace IronAutomata::Intermediate;

    stringstream s;

    {
        Automata a;
        node_p   node   = a.start_node()       = boost::make_shared<Node>();
        output_p output = node->first_output() = boost::make_shared<Output>();

        output->content().push_back('7');
        output->content().push_back('3');

        output_p other_output = output->next_output() = boost::make_shared<Output>();
        other_output->content().push_back('9');

        node->edges().push_back(Edge());
        Edge& edge = node->edges().back();

        node_p other_node = edge.target() = boost::make_shared<Node>();
        edge.add('5');

        write_automata(a, s);
        s.seekp(0);
    }

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_TRUE(reader.clean());
    EXPECT_TRUE(reader.success());

    Automata a = reader.automata();

    EXPECT_FALSE(a.no_advance_no_output());
    ASSERT_TRUE(bool(a.start_node()));
    node_p node = a.start_node();
    EXPECT_TRUE(node->advance_on_default());
    ASSERT_TRUE(bool(node->first_output()));
    ASSERT_EQ(1UL, node->edges().size());
    EXPECT_FALSE(node->default_target());
    output_p output = node->first_output();
    ASSERT_EQ(2UL, output->content().size());
    EXPECT_EQ('7', output->content()[0]);
    EXPECT_EQ('3', output->content()[1]);
    ASSERT_TRUE(bool(output->next_output()));
    output = output->next_output();
    ASSERT_EQ(1UL, output->content().size());
    EXPECT_EQ('9', output->content()[0]);
    EXPECT_FALSE(output->next_output());
    Edge& edge = node->edges().front();
    EXPECT_TRUE(edge.advance());
    ASSERT_TRUE(bool(edge.target()));
    ASSERT_EQ(1UL, edge.size());
    EXPECT_EQ('5', *edge.begin());
    node = edge.target();
    EXPECT_FALSE(node->default_target());
    EXPECT_TRUE(node->edges().empty());
    EXPECT_TRUE(node->advance_on_default());
    EXPECT_FALSE(node->first_output());
}
