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
 * @brief Predicate --- pre_eval_graph()
 *
 * Defines routines to pre-evaluate an entire MergeGraph.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__PRE_EVAL_GRAPH__
#define __PREDICATE__PRE_EVAL_GRAPH__

#include <ironbee/predicate/dag.hpp>
#include <ironbee/predicate/reporter.hpp>

namespace IronBee {
namespace Predicate {

class MergeGraph;   // merge_graph.hpp

/**
 * Pre-evaluate a MergeGraph.
 *
 * Calls Node::pre_eval() on every node in BFS fashion down from the roots.
 *
 * @param[in] reporter    Reporter to use for NodeReporter's.
 * @param[in] graph       Graph to pre-evaluate.
 * @param[in] environment Environment to pass to Node::pre_eval().
 **/
void pre_eval_graph(
    reporter_t  reporter,
    MergeGraph& graph,
    Environment environment
);

} // Predicate
} // IronBee

#endif
