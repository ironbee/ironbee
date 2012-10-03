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

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

using namespace std;

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
            boost::bind(&node_list_t::push_back, boost::ref(nodes), _1)
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
                    targets = target->targets_for(c);
                    if (targets.empty()) {
                        // Remove target.  Note lack of push.
                        did_something_for_node = true;
                        ++operations_done;
                    }
                    else if (targets.size() == 1 || ! conservative) {
                        // Redirect.
                        copy(
                            targets.begin(), targets.end(),
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

} // Intermediate
} // IronAutomata
