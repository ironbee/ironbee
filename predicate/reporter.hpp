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
 * @brief Predicate --- Reporter
 *
 * Defines Reporter and NodeReporter for Calls to report warnings and errors.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__REPORTER__
#define __PREDICATE__REPORTER__

#include <predicate/dag.hpp>

#include <list>
#include <string>

namespace IronBee {
namespace Predicate {

/**
 * Abstract interface for a reporter.
 *
 * First parameter is true if the report is an error rather than a warning.
 * Second parameter is a message.  Third parameter is an associated node.
 **/
typedef boost::function<
    void(
        bool, 
        const std::string&, 
        const node_cp&
    )
> reporter_t;

/**
 * An implementation of the reporter_t interface.
 *
 * Provides easy access to number of warnings and errors and to outputting
 * a report to an ostream.
 **/
class Reporter
{
public:
    /** 
     * Constructor.
     * 
     * @param[in] use_prefix If true, node sexprs will be prefixed to message.
     **/
    explicit
    Reporter(bool use_prefix = true);

    //! Add error message.
    void error(const std::string& message);
    //! Add warning message.
    void warn(const std::string& message);

    //! Convert to reporter_t.
    operator reporter_t();

    //! Write all messages to @a out.
    void write_report(std::ostream& out) const;

    //! Number of error messages added.
    size_t num_errors() const
    {
        return m_num_errors;
    }

    //! Number of warning messages added.
    size_t num_warnings() const
    {
        return m_num_warnings;
    }

private:
    typedef std::list<std::string> messages_t;
    messages_t m_messages;
    size_t m_num_errors;
    size_t m_num_warnings;
    bool          m_use_prefix;
};

/**
 * Collect error and warning messages for a specific node.
 **/
class NodeReporter
{
public:
    /**
     * Constructor.
     *
     * @param[in] reporter   Reporter to report messages to.
     * @param[in] node       Node to report messages for.
     **/
    NodeReporter(
        reporter_t     reporter,
        const node_cp& node
    );

    //! Node accessor.
    const node_cp& node() const
    {
        return m_node;
    }

    //! Add error message.
    void error(const std::string& msg);
    //! Add warning message.
    void warn(const std::string& msg);

private:
    reporter_t    m_reporter;
    const node_cp m_node;
};

} // Predicate
} // IronBee

#endif
