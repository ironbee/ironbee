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
 * @brief Predicate --- Helper routines for developing call nodes.
 *
 * These routines are useful for developers of new Call nodes.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__CALL_HELPERS__
#define __PREDICATE__CALL_HELPERS__

#include <predicate/eval.hpp>

namespace IronBee {
namespace Predicate {

/**
 * Check and extract literal value from literal node.
 *
 * @note No eval state necessary.
 *
 * @param[in] node Node to extract value from.
 * @return Only value or Value() if Null.
 * @throw einval if @a node is not literal.
 **/
Value literal_value(const node_cp& node);

} // Predicate
} // IronBee

#endif
