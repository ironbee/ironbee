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
 * @brief IronBee++ &mdash; Notifier Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbeepp/notifier.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/connection_data.hpp>
#include <ironbeepp/transaction_data.hpp>
#include <ironbeepp/parsed_request_line.hpp>
#include <ironbeepp/parsed_response_line.hpp>

namespace IronBee {

Notifier::Notifier(Engine engine) :
    m_engine( engine )
{
    // nop
}


Notifier Notifier::configuration_started()
{
    Internal::throw_if_error(
        ib_state_notify_cfg_started(
            m_engine.ib()
        )
    );
    return *this;
}

Notifier Notifier::configuration_finished()
{
    Internal::throw_if_error(
        ib_state_notify_cfg_finished(
            m_engine.ib()
        )
    );
    return *this;
}


Notifier Notifier::connection_opened(Connection connection)
{
    Internal::throw_if_error(
        ib_state_notify_conn_opened(
            m_engine.ib(),
            connection.ib()
        )
    );
    return *this;
}

Notifier Notifier::connection_data_in(ConnectionData connection_data)
{
    Internal::throw_if_error(
        ib_state_notify_conn_data_in(
            m_engine.ib(),
            connection_data.ib()
        )
    );
    return *this;
}

Notifier Notifier::connection_data_out(ConnectionData connection_data)
{
    Internal::throw_if_error(
        ib_state_notify_conn_data_out(
            m_engine.ib(),
            connection_data.ib()
        )
    );
    return *this;
}

Notifier Notifier::connection_closed(Connection connection)
{
    Internal::throw_if_error(
        ib_state_notify_conn_closed(
            m_engine.ib(),
            connection.ib()
        )
    );
    return *this;
}


Notifier Notifier::transaction_data_in(
    Transaction     transaction,
    TransactionData transaction_data
)
{
    Internal::throw_if_error(
        ib_state_notify_tx_data_in(
            m_engine.ib(),
            transaction.ib(),
            transaction_data.ib()
        )
    );
    return *this;
}

Notifier Notifier::transaction_data_out(
    Transaction     transaction,
    TransactionData transaction_data
)
{
    Internal::throw_if_error(
        ib_state_notify_tx_data_out(
            m_engine.ib(),
            transaction.ib(),
            transaction_data.ib()
        )
    );
    return *this;
}


Notifier Notifier::request_started(
    Transaction       transaction,
    ParsedRequestLine parsed_request_line
)
{
    Internal::throw_if_error(
        ib_state_notify_request_started(
            m_engine.ib(),
            transaction.ib(),
            parsed_request_line.ib()
        )
    );
    return *this;
}

Notifier Notifier::request_headers_data(
    Transaction                       transaction,
    const std::list<ParsedNameValue>& headers
)
{
    return request_headers_data(transaction, headers.begin(), headers.end());
}

Notifier Notifier::request_body_data(
    Transaction     transaction,
    TransactionData transaction_data
)
{
    Internal::throw_if_error(
        ib_state_notify_request_body_data(
            m_engine.ib(),
            transaction.ib(),
            transaction_data.ib()
        )
    );
    return *this;
}

Notifier Notifier::request_finished(
    Transaction transaction
)
{
    Internal::throw_if_error(
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
    Internal::throw_if_error(
        ib_state_notify_response_started(
            m_engine.ib(),
            transaction.ib(),
            parsed_response_line.ib()
        )
    );
    return *this;
}

Notifier Notifier::response_headers_data(
    Transaction                       transaction,
    const std::list<ParsedNameValue>& headers
)
{
    return response_headers_data(transaction, headers.begin(), headers.end());
}

Notifier Notifier::response_body_data(
    Transaction     transaction,
    TransactionData transaction_data
)
{
    Internal::throw_if_error(
        ib_state_notify_response_body_data(
            m_engine.ib(),
            transaction.ib(),
            transaction_data.ib()
        )
    );
    return *this;
}

Notifier Notifier::response_finished(
    Transaction transaction
)
{
    Internal::throw_if_error(
        ib_state_notify_response_finished(
            m_engine.ib(),
            transaction.ib()
        )
    );
    return *this;
}

} // IronBee
