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

#include "reporter.hpp"

#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {

Reporter::Reporter() :
    m_num_errors(0),
    m_num_warnings(0)
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

void Reporter::write_report(ostream& out) const
{
    BOOST_FOREACH(const string& msg, m_messages) {
        out << msg << endl;
    }
}

NodeReporter::NodeReporter(Reporter& reporter, const node_cp& node) :
    m_reporter(reporter),
    m_node(node)
{

}

void NodeReporter::error(const string& msg)
{
    m_reporter.error(m_node->to_s() + ": " + msg);
}

void NodeReporter::warn(const string& msg)
{
    m_reporter.warn(m_node->to_s() + ": " + msg);
}

} // Predicate
} // IronBee
