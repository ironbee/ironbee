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
 * This file provides IBPP_TRY_CATCH(), a macro to aid in converting C++
 * exceptions into ib_status_t return values.
 *
 * @warning Because this file works at the C++/C boundary, it includes some C
 *          IronBee files.  This will greatly pollute the global namespace
 *          and macro space.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP_INTERNAL_CATCH__
#define __IBPP_INTERNAL_CATCH__

#include <ironbeepp/exception.hpp>

#include <ironbee/types.h>

struct ib_engine_t;

namespace IronBee {

class Engine;

namespace Internal {

/**
 * Handle a IronBee++ exception.
 * @internal
 *
 * If @a engine is non-NULL, then uses it to emit a log error via
 * ib_log_error().  Otherwise, does nothing.
 *
 * @param[in] engine Engine to use for logging; may be null.
 * @param[in] which  Which error occurred.
 * @param[in] e      Exception caught.
 *
 * @returns Appropriate ib_status_t.
 **/
ib_status_t ibpp_caught_ib_exception(
     ib_engine_t* engine,
     ib_status_t  which,
     const error& e
);

/**
 * Log, if possible, a boost::exception.
 * @internal
 *
 * If @a engine is non-NULL, then uses it to emit a log error via
 * ib_log_error().  Otherwise, does nothing.
 *
 * @param[in] engine Engine to use for logging; may be null.
 * @param[in] e      Exception caught.
 *
 * @returns Appropriate ib_status_t.
 **/
ib_status_t ibpp_caught_boost_exception(
    ib_engine_t*        engine,
    const boost::exception& e
);

/**
 * Log, if possible, a std::exception.
 * @internal
 *
 * If @a engine is non-NULL, then uses it to emit a log error via
 * ib_log_error().  Otherwise, does nothing.
 *
 * @param[in] engine Engine to use for logging; may be null.
 * @param[in] which  Which error occurred.
 * @param[in] e      Exception caught.
 **/
ib_status_t ibpp_caught_std_exception(
    ib_engine_t*          engine,
    ib_status_t           which,
    const std::exception& e
);

/**
 * Log, if possible, an exception we know nothing about.
 * @internal
 *
 * If @a engine is non-NULL, then uses it to emit a log error via
 * ib_log_error().  Otherwise, does nothing.
 *
 * @param[in] engine Engine to use for logging; may be null.
 * @param[in] e      Exception caught.
 **/
ib_status_t ibpp_caught_unknown_exception(
    ib_engine_t* engine
);

/**
 * Turn an Ironbee::Engine into an ib_engine_t*.
 * @internal
 *
 * @param[in] engine Engine to transform.
 *
 * @returns ib_engine_t* form of @a engine.
 **/
ib_engine_t* normalize_engine( Engine& engine );

/**
 * Overload of previous function to handle trivial case.
 * @internal
 *
 * @param[in] engine Engine.
 *
 * @returns @a engine
 **/
ib_engine_t* normalize_engine( ib_engine_t* engine );

} // Internal
} // IronBee

/**
 * Helper macro for implementing IBPP_TRY_CATCH().
 * @internal
 *
 * @sa IBPP_TRY_CATCH()
 *
 * @param[in] ib_engine      Engine to use for logging, possibly NULL.
 * @param[in] exception_type The type of the exception to catch.
 * @param[in] ib_status      The return value to return.
 **/
#define IBPP_CATCH_IB(  ib_engine, exception_type, ib_status ) \
    catch ( const exception_type& e ) { \
        ibpp_try_catch_status = \
            ::IronBee::Internal::ibpp_caught_ib_exception( \
                ::IronBee::Internal::normalize_engine( ib_engine ), \
                ib_status, \
                e \
            ); \
    }

/**
 * Helper macro for implementing IBPP_TRY_CATCH().
 * @internal
 *
 * @sa IBPP_TRY_CATCH()
 *
 * @param[in] ib_engine      Engine to use for logging, possibly NULL.
 * @param[in] exception_type The type of the exception to catch.
 * @param[in] ib_status      The return value to return.
 **/
#define IBPP_CATCH_STD(  ib_engine, exception_type, ib_status ) \
    catch ( const exception_type& e ) { \
        ibpp_try_catch_status = \
            ::IronBee::Internal::ibpp_caught_std_exception( \
                ::IronBee::Internal::normalize_engine( ib_engine ), \
                ib_status, \
                e \
            ); \
    }

/**
 * Macro to translate exceptions to log messages and return values.
 * @internal
 *
 * This will evaluate @a statement instead a try block and catch any
 * exceptions.  If an exception is thrown, an error message will be logged
 * (except for IB_EALLOC errors).  The macro as a whole evaluates to an
 * appropriate ib_status_t.
 *
 * E.g.,
 * @code
 * IB_FTRACE_RET_STATUS( IBPP_TRY_CATCH( ib_engine, my_cpp_func() ) );
 * @endcode
 *
 * If @a ib_engine is non-NULL, an error log message will be logged via
 * ib_log_error().
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
 * @param[in] ib_engine Engine to use for logging; may be NULL, an
 *                      ib_engine_t*, or an IronBee::Engine&.
 * @param[in] statement The statement to evaluate.
 **/
#define IBPP_TRY_CATCH( ib_engine, statement) \
     ({ \
        ib_status_t ibpp_try_catch_status = IB_OK; \
        try { (statement); } \
        IBPP_CATCH_IB(  (ib_engine), ::IronBee::declined,     IB_DECLINED  ) \
        IBPP_CATCH_IB(  (ib_engine), ::IronBee::eunknown,     IB_EUNKNOWN  ) \
        IBPP_CATCH_IB(  (ib_engine), ::IronBee::enotimpl,     IB_ENOTIMPL  ) \
        IBPP_CATCH_IB(  (ib_engine), ::IronBee::eincompat,    IB_EINCOMPAT ) \
        IBPP_CATCH_IB(  (ib_engine), ::IronBee::ealloc,       IB_EALLOC    ) \
        IBPP_CATCH_IB(  (ib_engine), ::IronBee::einval,       IB_EINVAL    ) \
        IBPP_CATCH_IB(  (ib_engine), ::IronBee::enoent,       IB_ENOENT    ) \
        IBPP_CATCH_IB(  (ib_engine), ::IronBee::etrunc,       IB_ETRUNC    ) \
        IBPP_CATCH_IB(  (ib_engine), ::IronBee::etimedout,    IB_ETIMEDOUT ) \
        IBPP_CATCH_IB(  (ib_engine), ::IronBee::eagain,       IB_EAGAIN    ) \
        IBPP_CATCH_IB(  (ib_engine), ::IronBee::eother,       IB_EOTHER    ) \
        IBPP_CATCH_IB(  (ib_engine), ::IronBee::error,        IB_EUNKNOWN  ) \
        IBPP_CATCH_STD( (ib_engine), ::std::invalid_argument, IB_EINVAL    ) \
        IBPP_CATCH_STD( (ib_engine), ::std::bad_alloc,        IB_EALLOC    ) \
        catch ( const boost::exception& e ) {\
            ibpp_try_catch_status = \
                ::IronBee::Internal::ibpp_caught_boost_exception( \
                    ::IronBee::Internal::normalize_engine( (ib_engine) ), \
                    e \
                ); \
        } \
        catch ( const std::exception& e ) {\
            ibpp_try_catch_status = \
                ::IronBee::Internal::ibpp_caught_std_exception( \
                    ::IronBee::Internal::normalize_engine( (ib_engine) ), \
                    IB_EUNKNOWN, \
                    e \
                ); \
        } \
        catch( ... ) {\
            ibpp_try_catch_status = \
                ::IronBee::Internal::ibpp_caught_unknown_exception( \
                    ::IronBee::Internal::normalize_engine( (ib_engine) ) \
                ); \
        } \
        ibpp_try_catch_status; \
    })

#endif
