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
 * @brief IronAutomata --- Compact Nonadvancing implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/translate_nonadvancing.hpp>
#include <ironautomata/optimize_edges.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <list>
#include <set>

using namespace std;

namespace {

// push_back is often overloaded and thus tricky for bind.
template <typename Container>
void push_back(
    Container&                          to,
    typename Container::const_reference value
)
{
    to.push_back(value);
}

}

namespace IronAutomata {
namespace Intermediate {

size_t translate_nonadvancing(
    Automata& automata,
    bool      conservative
)
{
    // The current approach is focused on code simplicity.  It could
    // be far more efficient by making changes to the existing edge
    // list and defaults.  Instead, it builds up to 256 new edges
    // and uses optimize_edges to collapse them.
    size_t operations_done = 0;

    typedef list<node_p> node_list_t;
    node_list_t nodes;

    bool needs_attention = true;
    while (needs_attention) {
        needs_attention = false;

        // Make list of nodes.
        nodes.clear();
        breadth_first(
            automata,
            boost::bind(&push_back<node_list_t>, boost::ref(nodes), _1)
        );

        BOOST_FOREACH(const node_p& node, nodes) {
            vector<list<Node::target_info_t> > new_targets(256);
            bool did_something_for_node = false;
            for (int c = 0; c < 256; ++c) {
                Node::target_info_list_t targets = node->targets_for(c);
                for (
                    Node::target_info_list_t::iterator i = targets.begin();
                    i != targets.end();
                    ++i
                ) {
                    // Only concerned non-advancing edges.
                    if (i->second) {
                        new_targets[c].push_back(*i);
                        continue;
                    }
                    node_p target = i->first;

                    // Only concerned if target would not generate output.
                    if (
                        target->first_output() &&
                        ! automata.no_advance_no_output()
                    ) {
                        new_targets[c].push_back(*i);
                        continue;
                    }

                    // Only concerned if target has 0 or 1 exit.
                    Node::target_info_list_t target_targets =
						 target->targets_for(c);
                    if (target_targets.empty()) {
                        // Remove target.  Note lack of push.
                        did_something_for_node = true;
                        ++operations_done;
                    }
                    else if (target_targets.size() == 1 || ! conservative) {
                        // Redirect.
                        copy(
                            target_targets.begin(), target_targets.end(),
                            back_inserter(new_targets[c])
                        );
                        did_something_for_node = true;
                        ++operations_done;
                    }
                    else {
                        // Do nothing.
                        new_targets[c].push_back(*i);
                    }
                }
            }

            if (did_something_for_node) {
                // Build new edge list.
                node->edges().clear();
                node->default_target().reset();

                for (int c = 0; c < 256; ++c) {
                    BOOST_FOREACH(
                        const Node::target_info_t& target_info,
                        new_targets[c]
                    ) {
                        node->edges().push_back(
                            Edge::make_from_vector(
                                target_info.first,
                                target_info.second,
                                byte_vector_t(1, c)
                            )
                        );
                    }
                }

                optimize_edges(node);

                needs_attention = true;
            }
        }
    }

    return operations_done;
}

namespace {

//! A representation of all inputs of an edge.
typedef set<uint8_t> input_set_t;

//! Calculate the input set for an edge.
input_set_t input_set_of_edge(const Intermediate::Edge& edge)
{
    input_set_t result;
    copy(edge.begin(), edge.end(), inserter(result, result.begin()));
    return result;
}

//! Calculate the complete input set.
input_set_t all_inputs()
{
    input_set_t result;
    for (int c = 0; c < 256; ++c) {
        result.insert(c);
    }
    return result;
}

/**
 * Find the translated next target.
 *
 * If @a target has a unique next target for every input in @a inputs, and
 * entering @a target will not cause output to be generated, then the info
 * for the next target will be returned.  Otherwise a default constructed
 * target_info_t will be.
 */
Node::target_info_t find_next_target(
    const Automata&    automata,
    const input_set_t& inputs,
    const node_p&      target
)
{
    Node::target_info_t result;
    if (
        target->first_output() &&
        ! automata.no_advance_no_output()
    ) {
        return result;
    }
    BOOST_FOREACH(uint8_t c, inputs) {
        Node::target_info_list_t targets = target->targets_for(c);
        if (targets.size() != 1) {
            result.first.reset();
            break;
        }
        Node::target_info_t candidate = targets.front();
        if (result.first && candidate.first != result.first) {
            result.first.reset();
            break;
        }
        else if (! result.first) {
            result = candidate;
        }
    }
    return result;
}

}

size_t translate_nonadvancing_structural(
    Automata& automata
)
{
    size_t operations_done = 0;

    typedef list<node_p> node_list_t;
    node_list_t nodes;

    bool needs_attention = true;
    while (needs_attention) {
        needs_attention = false;

        // Make list of nodes.
        nodes.clear();
        breadth_first(
            automata,
            boost::bind(&push_back<node_list_t>, boost::ref(nodes), _1)
        );

        BOOST_FOREACH(const node_p& node, nodes) {
            input_set_t default_inputs = all_inputs();
            BOOST_FOREACH(Edge& edge, node->edges()) {
                input_set_t inputs = input_set_of_edge(edge);
                // This should be
                //   default_inputs.erase(inputs.begin(), inputs.end());
                // However, that corrupts default_inputs on certain platforms.
                BOOST_FOREACH(uint8_t c, inputs) {
                    default_inputs.erase(c);
                }
                if (edge.advance()) {
                    continue;
                }
                Node::target_info_t next_target = find_next_target(
                    automata,
                    inputs,
                    edge.target()
                );
                if (! next_target.first) {
                    continue;
                }

                // Good to go.
                ++operations_done;
                edge.target() = next_target.first;
                edge.advance() = next_target.second;
                needs_attention = true;
            }

            if (
                node->default_target() &&
                ! node->advance_on_default() &&
                ! default_inputs.empty()
            ) {
                Node::target_info_t next_target = find_next_target(
                    automata,
                    default_inputs,
                    node->default_target()
                );
                if (! next_target.first) {
                    continue;
                }

                // Good to go.
                ++operations_done;
                node->default_target() = next_target.first;
                node->advance_on_default() = next_target.second;
                needs_attention = true;
            }
        }
    }

    return operations_done;
}

} // Intermediate
} // IronAutomata
