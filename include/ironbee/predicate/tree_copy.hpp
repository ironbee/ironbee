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
 * @brief Predicate --- Tree Copy
 *
 * Defines tree_copy() which duplicates a tree of nodes.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__TREE_COPY__
#define __PREDICATE__TREE_COPY__

#include <ironbee/predicate/dag.hpp>

namespace IronBee {
namespace Predicate {

class CallFactory;

/**
 * Construct a copy of the tree rooted at @a source.
 *
 * This function will create a copy of a tree, duplicating all nodes.  The
 * returned result will be a tree with each node having at most one parent.
 * That is, the result is independent of any DAG that @a source is part of.
 *
 * @param[in] source  Root of tree to copy.
 * @param[in] factory Call factory that knows about all calls in @a source.
 * @return Root of copied tree.
 * @throw einval if @a source contains a call that @a factory does not know
 *               about.
 **/
node_p tree_copy(const node_cp& source, CallFactory factory);

} // Predicate
} // IronBee

#endif
