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
 * @brief IronBee --- CLIPP Generator for ModSec Audit Logs Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "modsec_audit_log_generator.hpp"

#include <clipp/parse_modifier.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses"
#pragma clang diagnostic ignored "-Wchar-subscripts"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
#include <boost/regex.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#include <boost/scoped_ptr.hpp>

#include <fstream>
#include <stdexcept>

using namespace std;

namespace IronBee {
namespace CLIPP {

struct ModSecAuditLogGenerator::State
{
    State(
        const string& path,
        on_error_t    on_error_
    ) :
        id(path),
        on_error(on_error_)
    {
        if (path == "-") {
            input = &cin;
        }
        else {
            input = new ifstream(path.c_str(), ios::binary);
            if (! *input) {
                throw runtime_error("Could not open " + path + " for reading.");
            }
        }
        parser.reset(new ModSecAuditLog::Parser(*input));
    }

    ~State()
    {
        if (id != "-") {
            delete input;
        }
    }

    string                                    id;
    on_error_t                                on_error;
    istream*                                  input;
    boost::scoped_ptr<ModSecAuditLog::Parser> parser;
};

ModSecAuditLogGenerator::ModSecAuditLogGenerator(
    const string& path,
    on_error_t on_error
) :
    m_state(new ModSecAuditLogGenerator::State(path, on_error))
{
    if (! *m_state->input) {
        throw runtime_error("Error reading " + path);
    }
}

bool ModSecAuditLogGenerator::operator()(Input::input_p& out_input)
{
    boost::shared_ptr<ModSecAuditLog::Entry> e
        = boost::make_shared<ModSecAuditLog::Entry>();
    out_input->source = e;

    bool have_entry = false;
    bool result = false;
    while (! have_entry) {
        try {
            result = (*m_state->parser)(*e);
        }
        catch (const exception& err) {
            if (m_state->on_error.empty()) {
                throw;
            }
            if (m_state->on_error(err.what())) {
                m_state->parser->recover();
                continue;
            }
        }
        if (! result) {
            return false;
        }
        have_entry = true;
    }

    // Extract connection information.
    static const boost::regex section_a(
      "([-@\\w]+) ([0-9.]+) (\\d+) ([0-9.]+) (\\d+)$"
    );
    boost::smatch match;
    try {
        const string& A = (*e)["A"];
        if (regex_search(A, match, section_a)) {
            out_input->id              = m_state->id + ":" + match.str(1);
            out_input->connection = Input::Connection();
            out_input->connection.connection_opened(
                Input::Buffer(A.c_str() + match.position(2),match.length(2)),
                boost::lexical_cast<uint16_t>(match.str(3)),
                Input::Buffer(A.c_str() + match.position(4), match.length(4)),
                boost::lexical_cast<uint16_t>(match.str(5))
            );
            out_input->connection.connection_closed();
        }
        else {
            throw runtime_error(
                "Could not parse connection information: " + A
            );
        }

        out_input->connection.add_transaction(
            Input::Buffer((*e)["B"]),
            Input::Buffer((*e)["F"])
        );
    }
    catch (...) {
        m_state->parser->recover();
        throw;
    }

    ParseModifier()(out_input);

    return true;
}

} // CLIPP
} // IronBee
