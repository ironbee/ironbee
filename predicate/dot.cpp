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
 * @brief Predicate --- DOT implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/predicate/dot.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {

string DefaultNodeDecorator::operator()(const node_cp& node) const
{

    call_cp as_call = boost::dynamic_pointer_cast<const Call>(node);
    if (as_call) {
        return "label=\"" + as_call->name() + "\"";
    }
    else {
        return "label=\"" + node->to_s() + "\"";
    }
}

namespace Impl {

dot_node_outputer::dot_node_outputer(
    ostream&             out,
    dot_node_decorator_t node_decorator
) :
    m_out(out),
    m_node_decorator(node_decorator)
{
    // nop
}

void dot_node_outputer::operator()(const node_cp& node) const
{
    m_out << "  \"" << node << "\""
         << " [" << m_node_decorator(node) << "];" << endl;

    BOOST_FOREACH(const node_p& child, node->children()) {
        m_out << "  \"" << node << "\" -> \"" << child << "\";"
             << endl;
    }
}

} // Impl

void to_dot(
    std::ostream& out,
    const node_cp& node,
    dot_node_decorator_t node_decorator
)
{
    vector<node_cp> v(1, node);
    to_dot(out, v.begin(), v.end(), node_decorator);
}

} // Predicate
} // IronBee
