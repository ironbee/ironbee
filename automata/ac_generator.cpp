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
 * @brief IronAutomata --- Aho-Corasick Generator
 *
 * This is a simple Aho-Corasick generator.  It is intended as an example and
 * test rather than a serious generator.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/buffer.hpp>
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
 * Specialized subclass node type.
 *
 * Adds a pointer to the last output in the output chain to allow easy
 * merging of output sets.  All nodes will be created as ACNode's and
 * static pointer casts will be used to retrieve when needed.
 */
struct ACNode : public Node
{
    /**
     * Last output in output change.
     *
     * set_output() will set this and @c output to the same value.
     * append_outputs() will use it to append another nodes outputs to
     * this nodes output.  append_outputs() should be called at most once.
     */
    output_p last_output;

    /**
     * Set output to @a to.
     */
    void set_output(const output_p& to)
    {
        first_output() = last_output = to;
    }

    /**
     * Append outputs of node @a other to this node.
     *
     * This method should be called zero or one times for each node.  It is
     * is fine, however, to append this node to other nodes multiple times.
     */
    void append_outputs(const node_p& other)
    {
        if (! last_output) {
            assert(! first_output());
            first_output() = last_output = other->first_output();
        }
        else {
            last_output->next_output() = other->first_output();
            last_output.reset();
        }
    }
};

//! Shared pointer to ac_node_t.
typedef boost::shared_ptr<ACNode> ac_node_p;

/**
 * Returns next node for an input of @a c at node @a node or node_p() if none.
 */
node_p find_next(const node_p& node, uint8_t c)
{
    Node::edge_list_t next_edges = node->edges_for(c);
    if (next_edges.empty()) {
        return node_p();
    }
    else {
        if (next_edges.size() != 1) {
            throw logic_error("Unexpected non-determinism.");
        }
        return next_edges.front().target();
    }
}

/**
 * Add word @a s to automata @a a.
 *
 * This function must be called one or more times before process_failures() is
 * called and never after.
 */
void add_word(Automata& a, const string& s)
{
    if (! a.start_node()) {
        a.start_node() = make_shared<ACNode>();
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
        edge.target() = make_shared<ACNode>();
        edge.add(c);
        current_node = edge.target();
    }

    assert(! current_node->first_output());
    output_p output = make_shared<Output>();
    boost::static_pointer_cast<ACNode>(current_node)->set_output(output);

    IronAutomata::buffer_t content_buffer;
    IronAutomata::BufferAssembler assembler(content_buffer);
    assembler.append_object(uint32_t(s.length()));

    output->content().assign(content_buffer.begin(), content_buffer.end());
}

/**
 * Process all failure transitions of @a a.
 *
 * See add_word() for discussion.
 */
void process_failures(Automata& a)
{
    assert(a.start_node());

    typedef list<node_p> node_list_t;
    node_list_t todo;

    BOOST_FOREACH(const Edge& edge, a.start_node()->edges()) {
        const node_p& target = edge.target();
        target->default_target() = a.start_node();
        target->advance_on_default() = false;
        todo.push_back(target);
    }

    // Variable names are partially chosen to match Ruby implementation and
    // AC paper.
    while (! todo.empty()) {
        node_p r = todo.front();
        todo.pop_front();

        BOOST_FOREACH(const Edge& edge, r->edges()) {
            assert(edge.size() == 1);
            uint8_t c = *edge.begin();
            node_p s = edge.target();

            todo.push_back(s);

            node_p current_node = r->default_target();
            node_p next_node;
            for (;;) {
                next_node = find_next(current_node, c);
                if (current_node == a.start_node() || next_node) {
                    break;
                }
                assert(current_node->default_target());
                current_node = current_node->default_target();
            }

            assert(current_node == a.start_node() || next_node);
            if (current_node == a.start_node() && ! next_node) {
                s->default_target() = a.start_node();
            }
            else {
                s->default_target() = next_node;
            }
            s->advance_on_default() = false;

            if (s->default_target()->first_output()) {
                ac_node_p ac_s = boost::static_pointer_cast<ACNode>(s);
                ac_s->append_outputs(s->default_target());
            }
        }
    }
}

//! Main
int main(int argc, char** argv)
{
    if (argc < 1 || argc > 2) {
        cout << "Usage: ac_generator [<chunk_size>]" << endl;
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
    a.start_node()->default_target() = a.start_node();
    a.start_node()->advance_on_default() = true;

    process_failures(a);

    breadth_first(a, optimize_edges);
    write_automata(a, cout, chunk_size);
}
