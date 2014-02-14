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

#include <ironbee/config.h>  // ib_cfg_log()
#include <ironbee/types.h>  // ib_status_t, ib_status_to_string()
#include <ironbee/log.h> // ib_log_*
#include <ironbee/util.h> // ib_util_log_*

#include <ironbeepp/catch.hpp>

#include <ironbeepp/configuration_parser.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/exception.hpp>
#include <ironbeepp/transaction.hpp>

using namespace std;

namespace IronBee {

ib_status_t convert_exception(
    const ib_engine_t* engine,
    bool               logging
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
            int level = static_cast<ib_logger_level_t>(IB_LOG_ERROR);

            message = string(ib_status_to_string(status)) + ": ";
            if (boost::get_error_info<errinfo_what>(e)) {
                message += *boost::get_error_info<errinfo_what>(e);
            }
            else {
                message.clear();
            }

            if (boost::get_error_info<errinfo_level>(e)) {
                level = *boost::get_error_info<errinfo_level>(e);
            }
            ib_logger_level_t ib_level = static_cast<ib_logger_level_t>(level);

            if (
                const ConfigurationParser* cp =
                    boost::get_error_info<errinfo_configuration_parser>(e)
            ) {
                if (! message.empty()) {
                    ib_cfg_log(cp->ib(), ib_level, "%s", message.c_str());
                }
                ib_cfg_log_info(
                    cp->ib(),
                    "%s",
                    diagnostic_information(e).c_str()
                );
            }
            else if (
                const Transaction* tx =
                    boost::get_error_info<errinfo_transaction>(e)
            ) {
                if (! message.empty()) {
                    ib_log_tx(tx->ib(), ib_level, "%s", message.c_str());
                }
                ib_log_info_tx(
                    tx->ib(),
                    "%s",
                    diagnostic_information(e).c_str()
                );
            }
            else if (engine) {
                if (! message.empty()) {
                    ib_log(engine, ib_level, "%s", message.c_str());
                }
                ib_log_info(
                    engine,
                    "%s",
                    diagnostic_information(e).c_str()
                );
            }
            else {
                if (! message.empty()) {
                    ib_util_log_error("%s", message.c_str());
                }
                ib_util_log_error("%s", diagnostic_information(e).c_str());
            }
        }
        catch (const boost::exception& e) {
            string message;

            if (boost::get_error_info<boost::throw_function>(e)) {
                message = "Unknown boost::exception thrown by ";
                message += *boost::get_error_info<boost::throw_function>(e);
            }
            else {
                message = "Unknown boost::exception thrown.";
            }

            if (engine) {
                ib_log_error(engine, "%s", message.c_str());
                ib_log_info(engine, "%s",
                    diagnostic_information(e).c_str()
                );
            } else {
                ib_util_log_error("%s", message.c_str());
                ib_util_log_error("%s", diagnostic_information(e).c_str());
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
    ConstEngine engine,
    bool        logging
)
{
    return convert_exception(engine.ib(), logging);
}

} // IronBee
