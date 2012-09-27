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

#ifndef _IA_LOGGER_HPP_
#define _IA_LOGGER_HPP_

/**
 * @file
 * @brief IronAutomata --- Logger
 *
 * General purpose logger callback.
 *
 * @sa logger_t
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <boost/function.hpp>

#include <iostream>
#include <string>

namespace IronAutomata {

/**
 * Log message type.
 */
enum log_message_t
{
    /**
     * Informative message.
     *
     * Info messages provide information and are not indicative of any
     * problem.
     */
    IA_LOG_INFO,

    /**
     * Warning message.
     *
     * Warning messages indicate a problem but do not result in inconsistent
     * data, i.e., are recoverable.  However, it is likely that the result of
     * the operation that generated the warning is not what was desired.
     */
    IA_LOG_WARN,

    /**
     * Error message.
     *
     * Error messages indicate problems that will either leave the result in
     * an inconsistent state or one that is minimally useful.  Error messages
     * may or may not abort execution.  Generally, execution continues if it
     * is possible for further meaningful error messages to be generated.  If
     * you want to ensure stopping at the first error message, throw an
     * exception in the logger.
     */
    IA_LOG_ERROR
};

/**
 * Logger callback.
 *
 * Various IronAutomata functions take a logger callback to deliver log
 * messages.  The callback is called with a message type (see log_message_t
 * for type descriptions), a description of where (possibly empty), and a
 * message.
 */
typedef boost::function<
    void(
        log_message_t,
        const std::string&,
        const std::string&
    )
> logger_t;

/**
 * NOP Logger.  Discards all messages.
 *
 * This is the default logger and simply discards messages.
 */
void nop_logger(
    log_message_t message_type,
    const std::string& where,
    const std::string& what
);

/**
 * Ostream Logger.  Log messages to an ostream.
 *
 * This formats messages as strings and writes them to the specified ostream.
 */
class ostream_logger
{
public:
    /**
     * Constructor.
     *
     * @param[in] out Stream to write to.
     */
    explicit
    ostream_logger(std::ostream& out);

    /**
     * Call operator.
     *
     * Format message as string and write to ostream.  Messages will be
     * terminated with new lines.
     *
     * @param[in] message_type Message type.
     * @param[in] where        Where event occurred; may be empty.
     * @param[in] what         What occurred.
     */
    void operator()(
        log_message_t message_type,
        const std::string& where,
        const std::string& what
    );

private:
    std::ostream& m_out;
};

} // IronAutomata

#endif
