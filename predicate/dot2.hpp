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
 * @brief Predicate --- Output DAG to GraphViz dot; alternative.
 *
 * Alternative graphviz renderer.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__DOT2__
#define __PREDICATE__DOT2__

#include <predicate/dag.hpp>
#include <predicate/validate_graph.hpp>

#include <boost/function.hpp>

namespace IronBee {
namespace Predicate {

//! Function to translate root indices into root names.
typedef boost::function<std::string(size_t)> root_namer_t;

/**
 * Write @a G out to @a out in GraphViz format.
 *
 * This version is designed to generate pretty and useful graphs for
 * consumption by predicate expression writers.  In contrast, to_dot() is a
 * more low level routine designed for use by Predicate developers.  The main
 * additional information to_dot2() provides is the ability to attach names to
 * roots and to display validation information.  Beyond that, it produces a
 * prettier and cleaner graph.
 *
 * @param[in] out Where to write output.
 * @param[in] G   Graph to render.
 * @param[in] validate   What, if any, validation to do.  Validation results
 *                       will color their respective nodes and attach the
 *                       messages to the side of the node.
 * @param[in] root_namer If provided, additional roots will be rendered and
 *                       attached to their appropriate nodes.
 **/
void to_dot2(
    std::ostream& out,
    const MergeGraph& G,
    validation_e validate = VALIDATE_NONE,
    root_namer_t root_namer = root_namer_t()
);

} // Predicate
} // IronBee

#endif
