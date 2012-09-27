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
 * @brief IronAutomata --- Logger Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/logger.hpp>

using namespace std;

namespace IronAutomata {

void nop_logger(
    log_message_t message_type,
    const string& where,
    const string& what
)
{
    // nop
}

ostream_logger::ostream_logger(ostream& out) :
    m_out(out)
{
    // nop
}

void ostream_logger::operator()(
    log_message_t message_type,
    const string& where,
    const string& what
)
{
    const char* type_s = NULL;
    switch (message_type) {
        case IA_LOG_INFO: type_s = "INFO"; break;
        case IA_LOG_WARN: type_s = "WARNING"; break;
        case IA_LOG_ERROR: type_s = "ERROR"; break;
        default: type_s = "UNKNOWN";
    }

    m_out << type_s << (where.empty() ? "" : " [" + where + "]") << ": "
          << what << endl;
}

} // IronAutomata
