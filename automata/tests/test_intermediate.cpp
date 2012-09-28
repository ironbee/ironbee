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

#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include <sstream>

#include <arpa/inet.h>

#include "gtest/gtest.h"

using namespace std;
using namespace IronAutomata::Intermediate;

namespace {

void chunk_to_if(ostream& out, const PB::Chunk& chunk)
{
    string buffer;
    google::protobuf::io::StringOutputStream output(&buffer);
    google::protobuf::io::GzipOutputStream zipped_output(&output);

    chunk.SerializeToZeroCopyStream(&zipped_output);
    zipped_output.Close();

    uint32_t size = buffer.length();
    uint32_t nsize = htonl(size);

    out.write(
        reinterpret_cast<const char*>(&nsize), sizeof(uint32_t)
    );
    out.write(buffer.data(), size);
}

}

TEST(TestIntermediate, Basic)
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

        chunk_to_if(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_TRUE(reader.clean());
    EXPECT_TRUE(reader.success());

    const automata_t& automata = reader.automata();
    EXPECT_FALSE(automata.no_advance_no_output);
    ASSERT_TRUE(automata.start_node);
    const node_t& node_a = *automata.start_node;
    EXPECT_FALSE(node_a.default_target);
    EXPECT_TRUE(node_a.advance_on_default);
    EXPECT_FALSE(node_a.output);
    EXPECT_EQ(1UL, node_a.edges.size());
    const edge_t& edge_ab = *node_a.edges.begin();
    ASSERT_TRUE(edge_ab.target);
    EXPECT_TRUE(edge_ab.advance);
    EXPECT_TRUE(edge_ab.values_bm.empty());
    EXPECT_EQ(1U, edge_ab.values.size());
    EXPECT_EQ('a', edge_ab.values[0]);
    const node_t& node_b = *edge_ab.target;
    EXPECT_FALSE(node_b.output);
    EXPECT_FALSE(node_b.default_target);
    EXPECT_TRUE(node_b.advance_on_default);
    EXPECT_TRUE(node_b.edges.empty());
}

TEST(TestIntermediate, Trivial)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;
        chunk_to_if(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_TRUE(reader.clean());
    EXPECT_TRUE(reader.success());

    const automata_t& automata = reader.automata();
    EXPECT_FALSE(automata.no_advance_no_output);
    EXPECT_FALSE(automata.start_node);
}

TEST(TestIntermediate, EmptyInput)
{
    stringstream s;

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.automata().start_node);
}

TEST(TestIntermediate, InvalidSize)
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

TEST(TestIntermediate, InvalidChunk)
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

TEST(TestIntermediate, DuplicateOutput)
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

        chunk_to_if(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_TRUE(reader.success());
}

TEST(TestIntermediate, DuplicateNode)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        PB::Node* pb_node_b = pb_chunk.add_nodes();
        pb_node_b->set_id(1);

        chunk_to_if(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_TRUE(reader.success());
}

TEST(TestIntermediate, TooValuedEdge)
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

        chunk_to_if(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_TRUE(reader.success());
}

TEST(TestIntermediate, BadValuesBitmap)
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

        chunk_to_if(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_TRUE(reader.success());
}

TEST(TestIntermediate, BitMapEdge)
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

        chunk_to_if(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_TRUE(reader.clean());
    EXPECT_TRUE(reader.success());

    const automata_t& automata = reader.automata();
    ASSERT_TRUE(automata.start_node);
    const node_t& node_a = *automata.start_node;
    ASSERT_EQ(1UL, node_a.edges.size());
    const edge_t& edge_ab = *node_a.edges.begin();
    EXPECT_TRUE(edge_ab.values.empty());
    EXPECT_EQ(32UL, edge_ab.values_bm.size());
    EXPECT_EQ('a', edge_ab.values_bm[0]);
    EXPECT_EQ('b', edge_ab.values_bm[1]);
    EXPECT_EQ('c', edge_ab.values_bm[2]);
    EXPECT_EQ('d', edge_ab.values_bm[3]);
}

TEST(TestIntermediate, EpsilonEdge)
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

        chunk_to_if(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_TRUE(reader.clean());
    EXPECT_TRUE(reader.success());

    const automata_t& automata = reader.automata();
    ASSERT_TRUE(automata.start_node);
    const node_t& node_a = *automata.start_node;
    ASSERT_EQ(1UL, node_a.edges.size());
    const edge_t& edge_ab = *node_a.edges.begin();
    EXPECT_TRUE(edge_ab.values.empty());
    EXPECT_TRUE(edge_ab.values_bm.empty());
}

TEST(TestIntermediate, MissingNode)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        PB::Edge* pb_edge_ab = pb_node_a->add_edges();
        pb_edge_ab->set_target(2);

        chunk_to_if(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_FALSE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_FALSE(reader.success());
}

TEST(TestIntermediate, ExcessNode)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        PB::Node* pb_node_b = pb_chunk.add_nodes();
        pb_node_b->set_id(2);

        chunk_to_if(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_TRUE(reader.success());
}

TEST(TestIntermediate, MissingOutput)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Node* pb_node_a = pb_chunk.add_nodes();
        pb_node_a->set_id(1);
        pb_node_a->set_first_output(2);

        chunk_to_if(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_FALSE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_FALSE(reader.success());
}

TEST(TestIntermediate, ExcessOutput)
{
    stringstream s;
    {
        PB::Chunk pb_chunk;

        PB::Output* pb_output = pb_chunk.add_outputs();
        pb_output->set_id(1);
        pb_output->set_content("content");

        chunk_to_if(s, pb_chunk);
    }
    s.seekp(0);

    IronAutomata::ostream_logger logger(cout);
    AutomataReader reader(logger);

    bool success = reader.read_from_istream(s);
    EXPECT_TRUE(success);
    EXPECT_FALSE(reader.clean());
    EXPECT_TRUE(reader.success());
}

TEST(TestIntermediate, BitmapEdgeIterator)
{
    using namespace IronAutomata::Intermediate;

    edge_t edge;
    edge.values_bm.resize(32);

    for (int i = 0; i <= 36; ++i ) {
        ia_setbitv(edge.values_bm.data(), 7 * i);
    }

    vector<uint8_t> result;
    pair<edge_value_iterator, edge_value_iterator> values = edge_values(edge);
    copy(values.first, values.second, back_inserter(result));
    ASSERT_EQ(37UL, result.size());
    for (int i = 0; i <= 36; ++i) {
        EXPECT_EQ(result[i], 7 * i);
    }
}

TEST(TestIntermediate, VectorEdgeIterator)
{
    using namespace IronAutomata::Intermediate;

    edge_t edge;

    for (int i = 0; i <= 36; ++i ) {
        edge.values.push_back(7 * i);
    }

    vector<uint8_t> result;
    pair<edge_value_iterator, edge_value_iterator> values = edge_values(edge);
    copy(values.first, values.second, back_inserter(result));
    ASSERT_EQ(37UL, result.size());
    for (int i = 0; i <= 36; ++i) {
        EXPECT_EQ(result[i], 7 * i);
    }
}
