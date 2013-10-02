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
 * @brief Predicate --- Call Helpers Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/call_helpers.hpp>

namespace IronBee {
namespace Predicate {

Value simple_value(const node_cp& node)
{
    if (! node->is_finished()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Asked for simple value of unfinished node."
            )
        );
    }
    if (node->values().size() > 1) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Asked for simple values of non-simple node."
            )
        );
    }

    if (node->values().empty()) {
        return Value();
    }
    else {
        return node->values().front();
    }
}

Value literal_value(const node_p& node)
{
    if (! node->is_literal()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Asked for literal value of non-literal node."
            )
        );
    }
    if (! node->is_finished()) {
        node->eval(EvalContext());
    }
    return simple_value(node);
}

} // Predicate
} // IronBee
