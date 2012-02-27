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
 * @brief IronBee++ -- Exceptions
 *
 * Defines the exception hierarchy used by IronBee++.
 *
 * These exceptions are translated into IronBee log error messages and
 * ib_status_t codes at the C++/C boundary.  You can use them, combined with
 * their info, to control what status codes are returned and what log
 * messages are emitted.
 *
 * The exception type determines the status code.  There as an exception for
 * every IronBee error status.  The errinfo_what info determined the log
 * message and the errinfo_level info determines the log level.
 *
 * @code
 * BOOST_THROW_EXCEPTION(
 *     IronBee::enoent
 *         << errinfo_what( "Entry not found " )
 *         << errinfo_level( 3 )
 * );
 * @endcode
 *
 * The boost::diagnostic_information() of an exception is also logged to the
 * debug log at the same log level as the error message.
 *
 * std::invalid_argument translates to IB_EINVAL and std::bad_alloc
 * translates to IB_EALLOC.  All other exceptions are translated to an
 * IB_EUNKNOWN error.  Log level 1 is used when no errinfo_level is available.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__EXCEPTION__
#define __IBPP__EXCEPTION__

#include <boost/exception/all.hpp>

#include <string>

namespace IronBee {

/**
 * Base exception type for all IronBee exceptions.  See exception.hpp
 *
 * You should never need to throw this directly.  Instead, prefer one of the
 * subclasses.
 *
 * This exception translates to IB_EUNKNOWN.
 **/
struct error : public boost::exception, public std::exception {};

//! Translates to IB_DECLINED.  See exception.hpp
struct declined : public error {};
//! Translates to IB_EUNKNOWN.  See exception.hpp
struct eunknown  : public error {};
//! Translates to IB_ENOTIMPL.  See exception.hpp
struct enotimpl  : public error {};
//! Translates to IB_EINCOMPAT.  See exception.hpp
struct eincompat : public error {};
//! Translates to IB_EALLOC.  See exception.hpp
struct ealloc    : public error {};
//! Translates to IB_EINVAL.  See exception.hpp
struct einval    : public error {};
//! Translates to IB_ENOENT.  See exception.hpp
struct enoent    : public error {};
//! Translates to IB_ETRUNC.  See exception.hpp
struct etrunc    : public error {};
//! Translates to IB_ETIMEDOUT.  See exception.hpp
struct etimedout : public error {};
//! Translates to IB_EAGAIN.  See exception.hpp
struct eagain    : public error {};
//! Translates to IB_EOTHER.  See exception.hpp
struct eother    : public error {};

/**
 * String exception info explaining what happened.
 *
 * If present in an exception, will be used to control the log message.  If
 * absent, a message indicating such is used instead.
 **/
typedef boost::error_info<struct tag_errinfo_what,std::string> errinfo_what;

/**
 * Level exception info given the desired log level.
 *
 * If present in an exception, will be used to control the log level.  If
 * absent, log level 1 is used.
 **/
typedef boost::error_info<struct tag_errinfo_level,int> errinfo_level;

} // IronBee

#endif
