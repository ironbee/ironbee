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
 * @brief IronBee --- ModSec Audit Log Parser
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__MODSEC_AUDIT_LOG__
#define __IRONBEE__CLIPP__MODSEC_AUDIT_LOG__

#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace IronBee {
namespace CLIPP {
namespace ModSecAuditLog {

/**
 * @class Entry
 * @brief An AuditLog entry.
 *
 * Represents an audit log entry.  Contains a buffer of each section.
 *
 * Format documented at: http://www.modsecurity.org/documentation/
 *
 * @sa Parser.
 **/
class Entry
{
    friend class Parser;

public:
    //! Access section @a section.
    /**
     * @param[in] section Section of audit log to return.
     * @return Section text as string.
     **/
    const std::string& operator[](const std::string& section) const;

    //! Clear entry.
    void clear();

private:
    typedef std::map<std::string, std::string> sections_t;
    sections_t m_sections;
};

/**
 * @class Parser
 * @brief Audit log parser.
 *
 * This class implements an audit log parser.  To use it, call operator()()
 * repeatedly.  If there is an error, you can use recover() to attempt to
 * recover from it.
 *
 * @sa Entry
 **/
class Parser
{
public:
    //! Constructor.
    /**
     * Any data in the input stream before the first A boundary is ignored.
     *
     * @param[in,out] in The input stream to parse.
     **/
    explicit
    Parser(std::istream& in);

    //! Fetch next entry.
    /**
     * Fetches the next entry from the input stream.  If there are no more
     * entries, it will return false.  If there is a parsing error, it will
     * throw a runtime_exception.  If an exception is thrown, its behavior for
     * future calls is undefined unless recover() is called.
     *
     * @param[out] out_entry Where to write the next entry.
     * @return true iff another entry was found.
     *
     * @throw runtime_exception on parse error.
     **/
    bool operator()(Entry& out_entry);

    //! Recover from an error.
    /**
     * This routines attempts to recover from a parsing error by looking for
     * the next A boundary.  This typically means that the entry the parse
     * error occurred on is discarded.  After this call, whether successful
     * or not, operator()() can be used again.
     *
     * @return true iff recovery was possible.  If false, then all future
     * calls to operator()() or recover() will also return false.
     **/
    bool recover();

private:
  std::istream& m_in;
  std::string   m_section;
  std::string   m_boundary;
  bool          m_have_entry;
};

} // CLIPP
} // ModSecAuditLog
} // IronBee

#endif
