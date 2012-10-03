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

#ifndef _IA_OPTIMIZE_EDGES_HPP_
#define _IA_OPTIMIZE_EDGES_HPP_

/**
 * @file
 * @brief IronAutomata --- Optimized edges of Intermediate Format
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/intermediate.hpp>

namespace IronAutomata {
namespace Intermediate {

/**
 * Ensure that @a node has the optimal (in terms of space) representation of
 * its edges.
 *
 * Ensures that @c node.edges contains exactly one edge_t for each distinct
 * target/advance setting.  For each such edge, a bitmap will be used if there
 * are 32 or more values and a values vector otherwise.
 *
 * @param[in] node Node to optimize.
 */
void optimize_edges(const node_p& node);

} // Intermediate
} // IronAutomata

#endif
