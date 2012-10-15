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
 * @brief IronAutomata --- Trie Generator
 *
 * This is a simple Trie generator.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/buffer.hpp>
#include <ironautomata/deduplicate_outputs.hpp>
#include <ironautomata/intermediate.hpp>
#include <ironautomata/optimize_edges.hpp>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

#include <list>

using namespace std;
using namespace IronAutomata::Intermediate;
using boost::make_shared;

/**
 * Returns next node for an input of @a c at node @a node or node_p() if none.
 */
node_p find_next(const node_p& node, uint8_t c)
{
    Node::target_info_list_t targets = node->targets_for(c);
    if (targets.empty()) {
        return node_p();
    }
    else {
        if (targets.size() != 1) {
            throw logic_error("Unexpected non-determinism.");
        }
        return targets.front().first;
    }
}

/**
 * Add word @a s to automata @a a.
 */
void add_word(Automata& a, const string& s)
{
    if (! a.start_node()) {
        a.start_node() = make_shared<Node>();
    }

    node_p current_node = a.start_node();
    size_t j = 0;
    while (j < s.length()) {
        uint8_t c = s[j];

        node_p next_node = find_next(current_node, c);
        if (! next_node) {
            break;
        }
        ++j;

        current_node = next_node;
    }

    while (j < s.length()) {
        uint8_t c = s[j];
        ++j;

        current_node->edges().push_back(Edge());
        Edge& edge = current_node->edges().back();
        edge.target() = make_shared<Node>();
        edge.add(c);
        current_node = edge.target();
    }

    assert(! current_node->first_output());
    output_p output = make_shared<Output>();
    current_node->first_output() = output;

    IronAutomata::buffer_t content_buffer;
    IronAutomata::BufferAssembler assembler(content_buffer);
    assembler.append_object(uint32_t(1));

    output->content().assign(content_buffer.begin(), content_buffer.end());
}

//! Main
int main(int argc, char** argv)
{
    if (argc < 1 || argc > 2) {
        cout << "Usage: trie_generator [<chunk_size>]" << endl;
        return 1;
    }

    size_t chunk_size = 0;
    if (argc == 2) {
        chunk_size = boost::lexical_cast<size_t>(argv[1]);
    }

    Automata a;

    list<string> words;
    string s;
    while (cin) {
        getline(cin, s);
        if (! s.empty()) {
            add_word(a, s);
        }
    }

    assert(a.start_node());

    breadth_first(a, optimize_edges);
    deduplicate_outputs(a);

    write_automata(a, cout, chunk_size);
}
