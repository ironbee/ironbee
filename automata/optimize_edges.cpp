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
 * @brief IronAutomata --- Optimize Edges Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/optimize_edges.hpp>
#include <ironautomata/bits.h>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/foreach.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <map>
#include <set>

using namespace std;

namespace IronAutomata {
namespace Intermediate {

void optimize_edges(const node_p& node)
{
    typedef set<uint8_t> input_set_t;
    typedef map<Node::target_info_t, input_set_t> inputs_by_target_t;

    Node::targets_by_input_t by_input = node->build_targets_by_input();
    inputs_by_target_t by_target;

    // Invert map.
    for (int c = 0; c < 256; ++c) {
        BOOST_FOREACH(const Node::target_info_t& info, by_input[c]) {
            by_target[info].insert(c);
        }
    }

    // Check for use default.  That is, every input has a target but no
    // target has every input.
    bool is_complete = true;
    for (int c = 0; c < 256; ++c) {
        if (by_input[c].empty()) {
            is_complete = false;
            break;
        }
    }

    // Find biggest, this will also tell us if there is any epsilon.
    inputs_by_target_t::iterator biggest;
    size_t biggest_size = 0;
    for (
        inputs_by_target_t::iterator i = by_target.begin();
        i != by_target.end();
        ++i
    ) {
        size_t s = i->second.size();
        if (s > biggest_size) {
            biggest_size = s;
            biggest = i;
        }
    }
    bool has_epsilon = (biggest_size == 256);

    // If complete and no epsilons or a single complete edge, use default.
    if (is_complete && (! has_epsilon || by_target.size() == 1)) {
        node->default_target() = biggest->first.first;
        node->advance_on_default() = biggest->first.second;
        by_target.erase(biggest);
    }
    else {
        // no default
        node->default_target().reset();
    }

    // Default is set, now build edges.
    node->edges().clear();
    BOOST_FOREACH(const inputs_by_target_t::value_type& v, by_target) {
        node->edges().push_back(Edge(v.first.first, v.first.second));
        Edge& edge = node->edges().back();

        if (v.second.size() != 256) {
            BOOST_FOREACH(uint8_t c, v.second) {
                edge.add(c);
            }
        }
        // Else Epsilon.
    }
}

} // Intermediate
} // IronAutomata
