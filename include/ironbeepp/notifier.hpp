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
 * @brief IronBee++ --- Notifier
 *
 * This file defines Notifier, a helper class for notifying an Engine of
 * events (state changes).
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __IBPP__NOTIFIER__
#define __IBPP__NOTIFIER__


#include <ironbeepp/abi_compatibility.hpp>
#include <ironbeepp/byte_string.hpp>
#include <ironbeepp/engine.hpp>
#include <ironbeepp/parsed_name_value.hpp>
#include <ironbeepp/throw.hpp>
#include <ironbeepp/transaction.hpp>

#include <ironbee/state_notify.h>

#include <list>

namespace IronBee {

class Connection;
class ParsedRequestLine;
class ParsedResponseLine;

/**
 * Helper class for Engine::notify().
 *
 * This class is returned by Engine::notify() and defines methods to notify
 * the engine of events (state changes).  E.g.,
 *
 * @code
 * engine.notify()
 *     .connection_opened(conn)
 *     .connection_data_in(conndata)
 *     .connection_closed(conn)
 *     ;
 * @endcode
 *
 * The IronBee state machine is complicated.  See state_notify.h for details.
 *
 * @sa Engine::notify()
 * @sa state_notify.h
 **/
class Notifier
{
public:
    /**
     * Constructor.
     *
     * Usually you will not construct a Notifier directly.  Instead,
     * call Engine::notify() which returns an appropriate Notifier.
     *
     * @sa Engine::notify()
     **/
    explicit
    Notifier(Engine engine);

    //! Notify connection_opened event.
    Notifier connection_opened(Connection connection);
    //! Notify connection_closed event.
    Notifier connection_closed(Connection connection);

    //! Notify request_started event.
    Notifier request_started(
        Transaction       transaction,
        ParsedRequestLine parsed_request_line
    );

    //! Notify request_header_data event.
    template <typename Iterator>
    Notifier request_header_data(
        Transaction       transaction,
        Iterator          header_begin,
        Iterator          header_end
    );

    //! Notify request_header_data event.
    Notifier request_header_data(
        Transaction                       transaction,
        const std::list<ParsedNameValue>& header
    );

    //! Notify request_header_finished event.
    Notifier request_header_finished(
        Transaction transaction
    );

    //! Notify request_body_data event.
    Notifier request_body_data(
        Transaction transaction,
        const char* data,
        size_t      data_length
    );

    //! Notify request_finished event.
    Notifier request_finished(
        Transaction transaction
    );

    //! Notify response_started event.
    Notifier response_started(
        Transaction        transaction,
        ParsedResponseLine parsed_response_line
    );

    //! Notify response_header_data event.
    template <typename Iterator>
    Notifier response_header_data(
        Transaction       transaction,
        Iterator          header_begin,
        Iterator          header_end
    );

    //! Notify response_header_data event.
    Notifier response_header_data(
        Transaction                       transaction,
        const std::list<ParsedNameValue>& header
    );

    //! Notify response_header_finished event.
    Notifier response_header_finished(
        Transaction transaction
    );

    //! Notify response_body_data event.
    Notifier response_body_data(
        Transaction transaction,
        const char* data,
        size_t      data_length
    );

    //! Notify response_finished event.
    Notifier response_finished(
        Transaction transaction
    );

private:
    Engine m_engine;
};

template <typename Iterator>
Notifier Notifier::request_header_data(
    Transaction       transaction,
    Iterator          header_begin,
    Iterator          header_end
)
{
    throw_if_error(
        ib_state_notify_request_header_data(
            m_engine.ib(),
            transaction.ib(),
            Internal::make_pnv_list(
                transaction.memory_pool(),
                header_begin, header_end
            )
        )
    );

    return *this;
}

template <typename Iterator>
Notifier Notifier::response_header_data(
    Transaction       transaction,
    Iterator          header_begin,
    Iterator          header_end
)
{
    throw_if_error(
        ib_state_notify_response_header_data(
            m_engine.ib(),
            transaction.ib(),
            Internal::make_pnv_list(
                transaction.memory_pool(),
                header_begin, header_end
            )
        )
    );

    return *this;
}

} // IronBee

#endif
