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
 * @brief Predicate --- transform_graph()
 *
 * Defines routines to transform an entire MergeGraph.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__TRANSFORM_GRAPH__
#define __PREDICATE__TRANSFORM_GRAPH__

#include <ironbee/predicate/dag.hpp>
#include <ironbee/predicate/reporter.hpp>

namespace IronBee {
namespace Predicate {

class MergeGraph;   // merge_graph.hpp
class ConstFactory; // call_factory.hpp

/**
 * Transform a MergeGraph.
 *
 * Calls Node::transform() on every node, starting with leaves and working
 * up in a BFS.
 *
 * @param[in] reporter     Reporter to use for NodeReporter's.
 * @param[in] graph        Graph to transform.
 * @param[in] call_factory CallFactory to pass to transform().
 * @param[in] environment  Environment to pass to transform().
 * @return true iff any transform call returned true, i.e., if the graph
 *         was changed.
 **/
bool transform_graph(
    reporter_t         reporter,
    MergeGraph&        graph,
    const CallFactory& call_factory,
    Environment        environment
);

} // Predicate
} // IronBee

#endif
