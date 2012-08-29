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
#include <ironbeepp/exception.hpp>

using namespace std;

namespace IronBee {
namespace Internal {

ib_status_t convert_exception(
    ib_engine_t* engine,
    bool         logging
)
{
    ib_status_t status = IB_OK;
    try {
        throw;
    }
    catch (declined)         { status = IB_DECLINED;  }
    catch (eunknown)         { status = IB_EUNKNOWN;  }
    catch (enotimpl)         { status = IB_ENOTIMPL;  }
    catch (eincompat)        { status = IB_EINCOMPAT; }
    catch (ealloc)           { status = IB_EALLOC;    }
    catch (einval)           { status = IB_EINVAL;    }
    catch (enoent)           { status = IB_ENOENT;    }
    catch (etrunc)           { status = IB_ETRUNC;    }
    catch (etimedout)        { status = IB_ETIMEDOUT; }
    catch (eagain)           { status = IB_EAGAIN;    }
    catch (eother)           { status = IB_EOTHER;    }
    catch (ebadval)          { status = IB_EBADVAL;   }
    catch (eexist)           { status = IB_EEXIST;    }
    catch (invalid_argument) { status = IB_EINVAL;    }
    catch (bad_alloc)        { status = IB_EALLOC;    }
    catch (...)              { status = IB_EUNKNOWN;  }

    if (logging) {
        try {
            throw;
        }
        catch (const error& e) {
            string message;
            int level = 1;

            message = string(ib_status_to_string(status)) + ":";
            if (boost::get_error_info<errinfo_what>(e)) {
                message += *boost::get_error_info<errinfo_what>(e);
            }
            else {
                message +=
                    "IronBee++ Exception but no explanation provided.  "
                    "Please report as bug.";
            }

            if (boost::get_error_info<errinfo_level>(e)) {
                level = *boost::get_error_info<errinfo_level>(e);
            }

            if (engine) {
                ib_log_level_t ib_level = static_cast<ib_log_level_t>(level);
                ib_log(engine, ib_level, "%s", message.c_str());
                ib_log_debug(engine, "%s", diagnostic_information(e).c_str() );
            } else {
                ib_util_log_error("%s", message.c_str());
                ib_util_log_debug("%s", diagnostic_information(e).c_str());
            }
        }
        catch (const exception& e) {
            string message;
            if (status == IB_EINVAL) {
                message = "Invalid argument: ";
            }
            else {
                message = "Unknown exception thrown: ";
            }
            message += e.what();

            if (engine) {
                ib_log_error(engine,  "%s", message.c_str());
            } else {
                ib_util_log_error("%s", message.c_str());
            }
        }
        catch (const boost::exception& e) {
            string message;
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
                ib_log_level_t ib_level = static_cast<ib_log_level_t>(level);
                ib_log(engine, ib_level, "%s", message.c_str());
                ib_log_debug(engine, "%s",
                    diagnostic_information(e).c_str()
                );
            } else {
                ib_util_log_error("%s", message.c_str());
                ib_util_log_debug("%s", diagnostic_information(e).c_str());
            }
        }
        catch(...) {
            if (engine) {
                ib_log_error(engine,  "%s",
                    "Completely unknown exception thrown.  "
                    "Please report as bug."
                );
            } else {
                ib_util_log_error("%s",
                    "Completely unknown exception thrown.  "
                    "Please report as bug."
                );
            }
        }
    }

    return status;
}

ib_status_t convert_exception(
    Engine engine,
    bool   logging
)
{
    return convert_exception(engine.ib(), logging);
}

} // Internal
} // IronBee
