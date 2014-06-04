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
 * @brief Predicate --- Breadth First Search Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/predicate/bfs.hpp>

namespace IronBee {
namespace Predicate {

namespace Impl {

void bfs_append_list(
    node_clist_t& list,
    const node_cp&      which,
    bfs_up_tag
)
{
    BOOST_FOREACH(const weak_node_p& weak_parent, which->parents()) {
        list.push_back(weak_parent.lock());
    }
}

void bfs_append_list(
    node_clist_t& list,
    const node_cp&      which,
    bfs_down_tag
)
{
    std::copy(
        which->children().begin(), which->children().end(),
        std::back_inserter(list)
    );
}

} // Impl

} // Predicate
} // IronBee
