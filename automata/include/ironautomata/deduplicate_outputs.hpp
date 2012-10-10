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

#ifndef _IA_DEDUPLICATE_OUTPUTS_HPP_
#define _IA_DEDUPLICATE_OUTPUTS_HPP_

/**
 * @file
 * @brief IronAutomata --- Deduplicate Outputs
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/intermediate.hpp>

namespace IronAutomata {
namespace Intermediate {

/**
 * Ensure that each possible output (content + next) is unique.
 *
 * Looks for pairs of outputs that are identical in both content and next
 * and merges them.  Iterates until stable.
 *
 * @param[in] automata Automata to process.
 * @return Number of outputs removed.
 */
size_t deduplicate_outputs(Automata& automata);

} // Intermediate
} // IronAutomata

#endif
