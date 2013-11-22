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
 * @brief IronBee++ --- Notifier Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/notifier.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/parsed_request_line.hpp>
#include <ironbeepp/parsed_response_line.hpp>

namespace IronBee {

Notifier::Notifier(Engine engine) :
    m_engine( engine )
{
    // nop
}

Notifier Notifier::connection_opened(Connection connection)
{
    throw_if_error(
        ib_state_notify_conn_opened(
            m_engine.ib(),
            connection.ib()
        )
    );
    return *this;
}

Notifier Notifier::connection_closed(Connection connection)
{
    throw_if_error(
        ib_state_notify_conn_closed(
            m_engine.ib(),
            connection.ib()
        )
    );
    return *this;
}


Notifier Notifier::request_started(
    Transaction       transaction,
    ParsedRequestLine parsed_request_line
)
{
    throw_if_error(
        ib_state_notify_request_started(
            m_engine.ib(),
            transaction.ib(),
            parsed_request_line.ib()
        )
    );
    return *this;
}

Notifier Notifier::request_header_data(
    Transaction                       transaction,
    const std::list<ParsedNameValue>& header
)
{
    return request_header_data(transaction, header.begin(), header.end());
}

Notifier Notifier::request_header_finished(
    Transaction transaction
)
{
    throw_if_error(
        ib_state_notify_request_header_finished(
            m_engine.ib(),
            transaction.ib()
        )
    );
    return *this;
}

Notifier Notifier::request_body_data(
    Transaction transaction,
    const char* data,
    size_t      data_length
)
{
    throw_if_error(
        ib_state_notify_request_body_data(
            m_engine.ib(),
            transaction.ib(),
            data, data_length
        )
    );
    return *this;
}

Notifier Notifier::request_finished(
    Transaction transaction
)
{
    throw_if_error(
        ib_state_notify_request_finished(
            m_engine.ib(),
            transaction.ib()
        )
    );
    return *this;
}

Notifier Notifier::response_started(
    Transaction        transaction,
    ParsedResponseLine parsed_response_line
)
{
    throw_if_error(
        ib_state_notify_response_started(
            m_engine.ib(),
            transaction.ib(),
            parsed_response_line.ib()
        )
    );
    return *this;
}

Notifier Notifier::response_header_data(
    Transaction                       transaction,
    const std::list<ParsedNameValue>& header
)
{
    return response_header_data(transaction, header.begin(), header.end());
}

Notifier Notifier::response_header_finished(
    Transaction transaction
)
{
    throw_if_error(
        ib_state_notify_response_header_finished(
            m_engine.ib(),
            transaction.ib()
        )
    );
    return *this;
}

Notifier Notifier::response_body_data(
    Transaction transaction,
    const char* data,
    size_t      data_length
)
{
    throw_if_error(
        ib_state_notify_response_body_data(
            m_engine.ib(),
            transaction.ib(),
            data, data_length
        )
    );
    return *this;
}

Notifier Notifier::response_finished(
    Transaction transaction
)
{
    throw_if_error(
        ib_state_notify_response_finished(
            m_engine.ib(),
            transaction.ib()
        )
    );
    return *this;
}

} // IronBee
