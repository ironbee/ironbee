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
 * This file provides convert_exception, a function to aid in converting
 * C++ exceptions into ib_status_t return values.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__INTERNAL__CATCH__
#define __IBPP__INTERNAL__CATCH__

#include <ironbee/types.h>

#include <cstdlib> // for NULL

struct ib_engine_t;

namespace IronBee {

class Engine;

namespace Internal {


/**
 * Handle any exception and translate into an IronBee status code.
 *
 * This function will terminate the process if called outside of a catch
 * block.
 *
 * Use like this:
 * @code
 * try {
 *   my_cpp_func();
 * }
 * catch (...) {
 *   return Internal::convert_exception(ib_engine);
 * }
 * @endcode
 *
 * All exceptions except allocation errors will result in log messages.  If
 * an engine is provided, the engine logger will be used, otherwise the util
 * logger will be used.  This logging behavior can be overridden via the
 * @a logging parameter.
 *
 * - The subclasses of IronBee::error turn into their corresponding
 *   ib_status_t.  E.g., declined becomes IB_DECLINED.
 * - std::invalid_argument becomes IB_EINVAL.
 * - std::bad_alloc becomes IB_EALLOC.
 * - Any other exception becomes IB_EUNKNOWN, including IronBee::error.
 *
 * The log message depends on the exception type:
 *
 * - For any exception that translates to IB_EALLOC, nothing is logged to
 *   avoid further allocations.
 * - For IronBee::error and its subclasses, the errinfo_what will be
 *   extracted and reported.  If IBPP_DEBUG is defined, full diagnostic
 *   info is logged (boost::diagnostic_information()).
 * - For any other boost::exception, a generic message is logged, and
 *   full diagnostic info if IBPP_DEBUG is defined.
 * - For any other std::exception, std::exception::what() is logged.
 * - For any other exception, a generic message is logged.
 *
 * Log level 1 is used unless an errinfo_level is attached to the exception,
 * in which case its value is used instead.
 *
 * @param[in] engine  Engine to use for logging; may be NULL
 * @param[in] logging Can be set to false to prevent any logging.
 **/
ib_status_t convert_exception(
    ib_engine_t* engine  = NULL,
    bool         logging = true
);

//! Overload of previous for Engine.
ib_status_t convert_exception(
    Engine engine,
    bool   logging = true
);

} // Internal
} // IronBee

#endif
