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
 * @brief Predicate --- validate_graph()
 *
 * Defines routines to validate an entire MergeGraph.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__VALIDATE_GRAPH__
#define __PREDICATE__VALIDATE_GRAPH__

#include <ironbee/predicate/dag.hpp>
#include <ironbee/predicate/reporter.hpp>

namespace IronBee {
namespace Predicate {

class MergeGraph;   // merge_graph.hpp

//! Which of pre_transform and post_transform to validate.
enum validation_e {
    VALIDATE_NONE,
    VALIDATE_PRE,
    VALIDATE_POST
};

/**
 * Validate a MergeGraph.
 *
 * Calls Node::pre_transform() or Node::post_transform() on every node,
 * starting with leaves and working up in a BFS.
 *
 * @param[in] which    Which validation routine to use.
 * @param[in] reporter Reporter to use for NodeReporter's.
 * @param[in] graph    Graph to validate.
 **/
void validate_graph(
    validation_e      which,
    reporter_t        reporter,
    const MergeGraph& graph
);

} // Predicate
} // IronBee

#endif
