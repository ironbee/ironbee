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
 * @brief IronAutomata --- Aho-Corasick Generator Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/generator/aho_corasick.hpp>
#include <ironautomata/buffer.hpp>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

#include <list>
#include <stdexcept>

using boost::make_shared;
using namespace std;

namespace IronAutomata {
namespace Generator {

namespace {

/**
 * Specialized subclass node type.
 *
 * Adds a pointer to the last output in the output chain to allow easy
 * merging of output sets.  All nodes will be created as ACNode's and
 * static pointer casts will be used to retrieve when needed.
 */
struct ACNode : public Intermediate::Node
{
    /**
     * Last output in output change.
     *
     * prepend_output() will maintain this and @c first_output.
     * append_outputs() will use it to append another nodes outputs to
     * this nodes output.  append_outputs() should be called at most once.
     */
    Intermediate::output_p last_output;

    /**
     * Prepend an output with content @a content.
     */
    void prepend_output(const Intermediate::byte_vector_t& content)
    {
        Intermediate::output_p output = make_shared<Intermediate::Output>();
        output->content() = content;
        output->next_output() = first_output();
        if (! last_output) {
            last_output = output;
        }
        first_output() = output;
    }

    /**
     * Append outputs of node @a other to this node.
     *
     * This method should be called zero or one times for each node.  It is
     * is fine, however, to append this node to other nodes multiple times.
     */
    void append_outputs(const Intermediate::node_p& other)
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

/**
 * Returns next node for an input of @a c at node @a node or Intermediate::node_p() if none.
 */
Intermediate::node_p find_next(
    const Intermediate::node_p& node,
    uint8_t       c
)
{
    Intermediate::Node::edge_list_t next_edges = node->edges_for(c);
    if (next_edges.empty()) {
        return Intermediate::node_p();
    }
    else {
        if (next_edges.size() != 1) {
            throw logic_error("Unexpected non-determinism.");
        }
        return next_edges.front().target();
    }
}

/**
 * Process all failure transitions of @a automata.
 *
 * See add_word() for discussion.
 */
void process_failures(Intermediate::Automata& automata)
{
    assert(automata.start_node());

    typedef list<Intermediate::node_p> node_list_t;
    node_list_t todo;

    BOOST_FOREACH(
        const Intermediate::Edge& edge,
        automata.start_node()->edges()
    ) {
        const Intermediate::node_p& target = edge.target();
        target->default_target() = automata.start_node();
        target->advance_on_default() = false;
        todo.push_back(target);
    }

    // Variable names are partially chosen to match Ruby implementation and
    // AC paper.
    while (! todo.empty()) {
        Intermediate::node_p r = todo.front();
        todo.pop_front();

        BOOST_FOREACH(const Intermediate::Edge& edge, r->edges()) {
            assert(edge.size() == 1);
            uint8_t c = *edge.begin();
            Intermediate::node_p s = edge.target();

            todo.push_back(s);

            Intermediate::node_p current_node = r->default_target();
            Intermediate::node_p next_node;
            for (;;) {
                next_node = find_next(current_node, c);
                if (current_node == automata.start_node() || next_node) {
                    break;
                }
                assert(current_node->default_target());
                current_node = current_node->default_target();
            }

            assert(current_node == automata.start_node() || next_node);
            if (current_node == automata.start_node() && ! next_node) {
                s->default_target() = automata.start_node();
            }
            else {
                s->default_target() = next_node;
            }
            s->advance_on_default() = false;

            if (s->default_target()->first_output()) {
                boost::shared_ptr<ACNode> ac_s =
                    boost::static_pointer_cast<ACNode>(s);
                ac_s->append_outputs(s->default_target());
            }
        }
    }
}

}

void aho_corasick_begin(
    Intermediate::Automata& automata
)
{
    if (automata.start_node()) {
        throw invalid_argument("Automata not empty.");
    }
    automata.start_node() = make_shared<ACNode>();
}

void aho_corasick_add_length(
    Intermediate::Automata& automata,
    const string&           s
)
{
    IronAutomata::buffer_t data_buffer;
    IronAutomata::BufferAssembler assembler(data_buffer);
    assembler.append_object(uint32_t(s.length()));

    aho_corasick_add_data(automata, s, data_buffer);
}

void aho_corasick_add_data(
    Intermediate::Automata&            automata,
    const std::string&                 s,
    const Intermediate::byte_vector_t& data
)
{
    if (! automata.start_node()) {
        throw invalid_argument("Automata lacks start node.");
    }
    Intermediate::node_p current_node = automata.start_node();

    size_t j = 0;
    while (j < s.length()) {
        uint8_t c = s[j];

        Intermediate::node_p next_node = find_next(current_node, c);
        if (! next_node) {
            break;
        }
        ++j;

        current_node = next_node;
    }

    while (j < s.length()) {
        uint8_t c = s[j];
        ++j;

        current_node->edges().push_back(Intermediate::Edge());
        Intermediate::Edge& edge = current_node->edges().back();
        edge.target() = make_shared<ACNode>();
        edge.add(c);
        current_node = edge.target();
    }

    boost::static_pointer_cast<ACNode>(current_node)->prepend_output(data);
}

void aho_corasick_finish(
    Intermediate::Automata& automata
)
{
    assert(automata.start_node());
    automata.start_node()->default_target() = automata.start_node();
    automata.start_node()->advance_on_default() = true;

    automata.no_advance_no_output() = true;

    process_failures(automata);
}

} // Generator
} // IronAutomata
