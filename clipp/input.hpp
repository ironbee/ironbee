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
 * @brief IronBee &mdash; CLIPP Input
 *
 * Defines input_t, the fundamental unit of input that CLIPP will give to
 * IronBee and related structures: buffer_t (a copyless substring) and
 * input_generator_t (a generic producer of input).
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IRONBEE__CLIPP__INPUT__
#define __IRONBEE__CLIPP__INPUT__

#include <boost/any.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
#include <boost/shared_ptr.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <string>
#include <vector>
#include <iostream>

#include <stdint.h>

namespace IronBee {
namespace CLIPP {

/**
 * Simple representation of memory buffer.
 *
 * This structure is a data pointer and length.  It's primary use is to refer
 * to substrings without copying.
 **/
struct buffer_t
{
    //! Default constructor.
    buffer_t();

    //! Constructor.
    /**
     * @param[in] data   Pointer to buffer.  Not necessarily null terminated.
     * @param[in] length Length of buffer.
     **/
    buffer_t(const char* data_, size_t length_);

    //! Construct from string.
    /**
     * The parameter @a s will need to outlive this buffer_t.
     *
     * @param[in] s String to initialize buffer from.
     **/
    explicit
    buffer_t(const std::string& s);

    //! Convert to string.  Makes a copy.
    std::string to_s() const;

    //! Pointer to buffer.  Not necessarily null terminated.
    const char* data;
    //! Length of buffer.
    size_t      length;
};

//! Output operator for buffers.
std::ostream& operator<<(std::ostream& out, const buffer_t& buffer);

/**
 * An input for CLIPP to send to IronBee.
 *
 * This structure represents the basic input of CLIPP.  It is connection
 * information and 0 or more transactions.
 **/
struct input_t
{
    //! ID.  Optional.  For human consumption.
    std::string id;

    //! Local IP address.  Must outlive input_t.
    buffer_t local_ip;
    //! Local port.
    uint16_t local_port;

    //! Remote IP address.  Must outlive input_t.
    buffer_t remote_ip;
    //! Remote port.
    uint16_t remote_port;

    //! A transaction for IronBee to process.
    struct transaction_t {
        //! Default constructor.
        transaction_t();

        //! Constructor.
        transaction_t(buffer_t request_, buffer_t response_);

        //! Request data.  Must outlive input_t.
        buffer_t request;
        //! Response data.  Must outlive input_t.
        buffer_t response;
    };

    //! Zero or more transactions.
    std::vector<transaction_t> transactions;

    //! This boost::any can be used to associate memory with the input.
    boost::any source;
};

//! Shared pointer to input_t.
typedef boost::shared_ptr<input_t> input_p;

//! Ostream output operator for an input.
std::ostream& operator<<(std::ostream& out, const input_t& input);

} // CLIPP
} // IronBee

#endif
