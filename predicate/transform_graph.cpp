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
 * @brief Predicate --- Transform Graph Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/predicate/transform_graph.hpp>

#include <ironbee/predicate/leaves.hpp>
#include <ironbee/predicate/merge_graph.hpp>

#include <boost/bind.hpp>
#include <boost/function_output_iterator.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {

bool transform_graph(
    reporter_t         reporter,
    MergeGraph&        graph,
    const CallFactory& call_factory,
    Environment        environment
)
{
    bool result = false;
    node_list_t todo;
    set<node_p> visited;

    // find_leaves guarantees no duplicates in output.
    find_leaves(
        graph.roots().first, graph.roots().second,
        back_inserter(todo)
    );

    while (! todo.empty()) {
        // Node added to list in the past.
        node_p n = todo.front();
        // What n has been transformed to since it was added to todo.
        node_p tn;
        // What tn has been transformed to by n->transform()
        node_p ttn;

        todo.pop_front();

        if (visited.count(n)) {
            continue;
        }

        try {
            tn = graph.find_transform(n);
        }
        catch (IronBee::enoent) {
            tn = n;
        }
        if (! tn) {
            continue;
        }

        if (! visited.insert(tn).second) {
            continue;
        }

        result = tn->transform(
            graph,
            call_factory,
            environment,
            NodeReporter(reporter, tn)
        ) || result;

        try {
            ttn = graph.find_transform(n);
        }
        catch (IronBee::enoent) {
            ttn = n;
        }

        if (ttn) {
            visited.insert(ttn);
            BOOST_FOREACH(const weak_node_p& parent, ttn->parents()) {
                todo.push_back(parent.lock());
            }
        }
    }

    return result;
}

} // Predicate
} // IronBee
