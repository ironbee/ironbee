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
 * @brief IronBee &mdash; CLIPP Generator for ModSec Audit Logs Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "modsec_audit_log_generator.hpp"

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
#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

#include <stdexcept>
#include <fstream>

using namespace std;

namespace IronBee {
namespace CLIPP {

ModSecAuditLogGenerator::ModSecAuditLogGenerator(
    const std::string& path,
    on_error_t on_error
) :
    m_id(path),
    m_on_error(on_error),
    m_input(boost::make_shared<ifstream>(path.c_str())),
    m_parser(*m_input)
{
    if (! *m_input) {
        throw runtime_error("Error reading " + path);
    }
}

bool ModSecAuditLogGenerator::operator()(input_p& out_input)
{
    boost::shared_ptr<ModSecAuditLog::Entry> e
        = boost::make_shared<ModSecAuditLog::Entry>();
    out_input->source = e;

    bool have_entry = false;
    bool result;
    while (! have_entry) {
        try {
            result = m_parser(*e);
        }
        catch (const exception& err) {
            if (m_on_error.empty()) {
                throw;
            }
            if (m_on_error(err.what())) {
                m_parser.recover();
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
            out_input->id              = m_id + ":" + match.str(1);
            out_input->local_ip.data   = A.c_str() + match.position(2);
            out_input->local_ip.length = match.length(2);

            out_input->local_port =
                boost::lexical_cast<uint16_t>(match.str(3));

            out_input->remote_ip.data   = A.c_str() + match.position(4);
            out_input->remote_ip.length = match.length(4);

            out_input->remote_port =
                boost::lexical_cast<uint16_t>(match.str(5));
        }
        else {
            throw runtime_error(
                "Could not parse connection information: " + A
            );
        }

        out_input->transactions.clear();
        out_input->transactions.push_back(input_t::transaction_t(
            buffer_t((*e)["B"]), buffer_t((*e)["F"])
        ));
    }
    catch (...) {
        m_parser.recover();
        throw;
    }

    return true;
}

} // CLIPP
} // IronBee
