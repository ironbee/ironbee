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

bool flatten_children(
    const node_p&      to,
    const node_cp&     from,
    const std::string& name
)
{
    bool did_something = false;

    BOOST_FOREACH(const node_p& child, from->children()) {
        if (
            boost::dynamic_pointer_cast<Call>(child) &&
            boost::dynamic_pointer_cast<Call>(child)->name() == name
        ) {
            BOOST_FOREACH(const node_p& subchild, child->children()) {
                to->add_child(subchild);
            }
            did_something = true;
        }
        else {
            to->add_child(child);
        }
    }

    return did_something;
}

} // Predicate
} // IronBee
