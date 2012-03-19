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
 * @brief IronBee++ Internals -- Catch
 * @internal
 *
 * @sa catch.hpp
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/types.h>  // ib_status_t, ib_status_to_string()
#include <ironbee/engine.h> // ib_log_*
#include <ironbee/util.h> // ib_util_log_*

#include <ironbeepp/internal/catch.hpp>

#include <ironbeepp/engine.hpp>

namespace IronBee {
namespace Internal {

ib_status_t ibpp_caught_ib_exception(
    ib_engine_t* engine,
    ib_status_t  status,
    const error& e
)
{
    std::string message;
    int level = 1;

    message = std::string(ib_status_to_string(status)) + ":";
    if (boost::get_error_info<errinfo_what>(e)) {
        message += *boost::get_error_info<errinfo_what>(e);
    }
    else {
        message += "IronBee++ Exception but no explanation provided.  "
                   "Please report as bug.";
    }

    if (boost::get_error_info<errinfo_level>(e)) {
        level = *boost::get_error_info<errinfo_level>(e);
    }

    if (engine) {
        ib_log_error(engine, level, "%s", message.c_str());
        ib_log_debug(engine, level, "%s",
            diagnostic_information(e).c_str()
        );
    } else {
        ib_util_log_error(level, "%s", message.c_str());
        ib_util_log_debug(level, "%s",
            diagnostic_information(e).c_str()
        );
    }
    return status;
}

ib_status_t ibpp_caught_boost_exception(
    ib_engine_t*            engine,
    const boost::exception& e
)
{
    std::string message;
    int level = 1;

    message = "Unknown boost::exception thrown: ";
    if (boost::get_error_info<boost::throw_function>(e)) {
        message += *boost::get_error_info<boost::throw_function>(e);
    }
    else {
        message += "No information provided.  Please report as bug.";
    }

    if (boost::get_error_info<errinfo_level>(e)) {
        level = *boost::get_error_info<errinfo_level>(e);
    }

    if (engine) {
        ib_log_error(engine, level, "%s", message.c_str());
        ib_log_debug(engine, level, "%s",
            diagnostic_information(e).c_str()
        );
    } else {
        ib_util_log_error(level, "%s", message.c_str());
        ib_util_log_debug(level, "%s",
            diagnostic_information(e).c_str()
        );
    }

    return IB_EUNKNOWN;
}

ib_status_t ibpp_caught_std_exception(
    ib_engine_t*          engine,
    ib_status_t           status,
    const std::exception& e
)
{
    if (status == IB_EALLOC) {
        return status;
    }

    std::string message;
    if (status == IB_EINVAL) {
        message = "Invalid argument: ";
    }
    else {
        message = "Unknown std::exception thrown: ";
    }
    message += e.what();

    if (engine) {
        ib_log_error(engine, 1, "%s", message.c_str());
    } else {
        ib_util_log_error(1, "%s", message.c_str());
    }

    return status;
}

ib_status_t ibpp_caught_unknown_exception(
    ib_engine_t* engine
)
{
    if (engine) {
        ib_log_error(engine, 1, "%s",
            "Completely unknown exception thrown.  "
            "Please report as bug."
        );
    } else {
        ib_util_log_error(1, "%s",
            "Completely unknown exception thrown.  "
            "Please report as bug."
        );
    }

    return IB_EUNKNOWN;
}

ib_engine_t* normalize_engine(Engine& engine)
{
    return engine.ib();
}

ib_engine_t* normalize_engine(ib_engine_t* engine)
{
    return engine;
}

} // Internal
} // IronBee
