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
 * @brief Predicate --- Reporter Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/predicate/reporter.hpp>

#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {

Reporter::Reporter(bool use_prefix) :
    m_num_errors(0),
    m_num_warnings(0),
    m_use_prefix(use_prefix)
{
    // nop
}

void Reporter::error(const string& message)
{
    m_messages.push_back("ERROR: " + message);
    ++m_num_errors;
}

void Reporter::warn(const string& message)
{
    m_messages.push_back("WARNING: " + message);
    ++m_num_warnings;
}

namespace {

class reporter_helper
{
public:
    reporter_helper(Reporter& parent, bool use_prefix) :
        m_parent(parent),
        m_use_prefix(use_prefix)
    {
        /*nop*/
    }

    void operator()(
        bool          is_error,
        const string& message,
        const Node*   node
    ) const
    {
        if (m_use_prefix) {
            if (is_error) {
                m_parent.error(node->to_s() + ":" + message);
            }
            else {
                m_parent.warn(node->to_s() + ":" + message);
            }
        }
        else {
            if (is_error) {
                m_parent.error(message);
            }
            else {
                m_parent.warn(message);
            }
        }
    }

private:
    Reporter& m_parent;
    bool m_use_prefix;
};

}

Reporter::operator reporter_t()
{
    return reporter_helper(*this, m_use_prefix);
}

void Reporter::write_report(ostream& out) const
{
    BOOST_FOREACH(const string& msg, m_messages) {
        out << msg << endl;
    }
}

NodeReporter::NodeReporter(
    reporter_t  reporter,
    const Node* node
):
    m_reporter(reporter),
    m_node(node)
{
    // nop
}

void NodeReporter::error(const string& msg)
{
    m_reporter(true, msg, m_node);
}

void NodeReporter::warn(const string& msg)
{
    m_reporter(false, msg, m_node);
}

} // Predicate
} // IronBee
