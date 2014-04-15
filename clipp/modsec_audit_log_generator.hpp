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
 * @brief IronBee --- CLIPP Generator for ModSec Audit Logs
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE_CLIPP__MODSEC_AUDIT_LOG_GENERATOR__
#define __IRONBEE_CLIPP__MODSEC_AUDIT_LOG_GENERATOR__

#include <clipp/input.hpp>
#include <clipp/modsec_audit_log.hpp>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include <iostream>
#include <string>

namespace IronBee {
namespace CLIPP {

/**
 * @class ModSecAuditLogGenerator
 * @brief Input generator from modsec audit logs.
 *
 * Produces input_t's from an modsec audit log.  This uses
 * IronBee::AuditLog::Parser to parse the audit log.  It requires that the
 * audit log provide sections B and F.
 **/
class ModSecAuditLogGenerator
{
public:
    //! Default Constructor.
    /**
     * Behavior except for assigning to is undefined.
     **/
    ModSecAuditLogGenerator();

    //! Type of on_error.  See AuditLogGenerator()
    typedef boost::function<bool(const std::string&)> on_error_t;

    //! Constructor.
    /**
     * @param[in] path     Path to audit log.
     * @param[in] on_error Function to call if an error occurs.  Message will
     *                     be passed in.  If returns true, generator will try
     *                     to recover, otherwise generator will stop parsing.
     *                     If default, then generator will throw exception on
     *                     error.
     **/
    explicit
    ModSecAuditLogGenerator(
        const std::string& path,
        on_error_t on_error = on_error_t()
    );

    //! Produce an input.  See input_t and input_generator_t.
    bool operator()(Input::input_p& out_input);

private:
    struct State;
    boost::shared_ptr<State> m_state;
};

} // CLIPP
} // IronBee

#endif
