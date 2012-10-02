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

#include <boost/foreach.hpp>

#include <map>
#include <set>

using namespace std;

namespace IronAutomata {
namespace Intermediate {

void optimize_edges(const node_p& node)
{
    typedef set<uint8_t> input_set_t;
    typedef pair<node_p, bool> target_key_t;
    typedef map<target_key_t, input_set_t> targets_map_t;
    typedef set<target_key_t> targets_set_t;

    targets_map_t targets;
    targets_set_t epsilons;

    BOOST_FOREACH(const Edge& edge, node->edges()) {
        if (edge.epsilon()) {
            epsilons.insert(make_pair(edge.target(), edge.advance()));
        }
        else {
            BOOST_FOREACH(uint8_t c, edge) {
                targets[make_pair(edge.target(), edge.advance())].insert(c);
            }
        }
    }

    list<Edge> new_edges;
    BOOST_FOREACH(const targets_map_t::value_type& v, targets) {
        new_edges.push_back(Edge());
        Edge& edge = new_edges.back();
        edge.target() = v.first.first;
        edge.advance() = v.first.second;
        BOOST_FOREACH(uint8_t c, v.second) {
            edge.add(c);
        }
    }
    BOOST_FOREACH(const target_key_t& key, epsilons) {
        new_edges.push_back(Edge());
        Edge& edge = new_edges.back();
        edge.target() = key.first;
        edge.advance() = key.second;
    }

    node->edges().swap(new_edges);
}

} // Intermediate
} // IronAutomata
