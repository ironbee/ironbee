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
 * @brief Predicate --- Tree Copy Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/predicate/tree_copy.hpp>
#include <ironbee/predicate/parse.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {

node_p tree_copy(const node_cp& source, CallFactory factory)
{
    // Simply parse the sexpr.  This isn't the most efficient but is
    // extremely simple.
    size_t i = 0;
    string sexpr = source->to_s();
    if (sexpr.empty()) {
        return node_p();
    }
    if (sexpr[0] == '(') {
        return parse_call(sexpr, i, factory);
    }
    return parse_literal(sexpr, i);
}

} // Predicate
} // IronBee
