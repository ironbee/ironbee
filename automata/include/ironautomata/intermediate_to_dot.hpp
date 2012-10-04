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

#ifndef _IA_INTERMEDIATE_TO_DOT_HPP_
#define _IA_INTERMEDIATE_TO_DOT_HPP_

/**
 * @file
 * @brief IronAutomata --- Render an automata in GraphViz DOT format.
 *
 * @sa to_dot.cpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/intermediate.hpp>

namespace IronAutomata {
namespace Intermediate {

/**
 * Render @a automata to @a to in GraphViz dot format.
 *
 * @warning Only useful for small to moderate sized automata.
 *
 * Nodes are diamonds and labeled either by breadth first order (default) or
 * pointer (if @a label_by_pointer is true).  Edges are solid arrows labeled
 * by values.  Epsilon edges are labeled with an epsilon.  Non-advancing
 * edges are red.  Default edges are dashed.  Outputs are rectangles labeled
 * by content.  Dotted arrows point from nodes to outputs and from outputs to
 * outputs.
 *
 * All labels will show non-printable bytes by decimal value in angle
 * brackets.
 *
 * There is a very similar utility, to_dot, which does the same things but
 * streams the protobuf format instead of writing out an Automata.
 *
 * @param[in] to               Where to write dot.
 * @param[in] automata         Automata to render.
 * @param[in] label_by_pointer If true, nodes will be labeled by pointer
 *                             rather than BFS order.
 */
void intermediate_to_dot(
    std::ostream&           to,
    const Automata&         automata,
    bool label_by_pointer = false
);

} // Intermediate
} // IronAutomata

#endif
