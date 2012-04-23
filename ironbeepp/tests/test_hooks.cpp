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
 * @brief IronBee++ Internals &mdash; Hooks Tests
 * @internal
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/hooks.hpp>
#include <ironbeepp/hooks.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/connection_data.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/transaction_data.hpp>
#include <ironbeepp/parsed_name_value.hpp>
#include <ironbeepp/parsed_request_line.hpp>
#include <ironbeepp/parsed_response_line.hpp>

#include "fixture.hpp"

#include "gtest/gtest.h"

#include <ironbee/debug.h>
#include <ironbee/engine.h>

#include "ironbee_private.h"

#include <string>
#include <sstream>

using namespace std;
using namespace IronBee;

class TestHooks : public ::testing::Test, public IBPPTestFixture
{
protected:
    enum callback_e
    {
        CB_NOT_CALLED,
        CB_NULL,
        CB_HEADERS,
        CB_REQUEST_LINE,
        CB_RESPONSE_LINE,
        CB_CONNECTION,
        CB_CONNECTION_DATA,
        CB_TRANSACTION,
        CB_TRANSACTION_DATA
    };

    struct handler_info_t
    {
        handler_info_t() : which(CB_NOT_CALLED) {}
        callback_e            which;
        Engine                engine;
        Transaction           transaction;
        Engine::state_event_e event;

        ParsedNameValue       parsed_name_value;
        ParsedRequestLine     parsed_request_line;
        ParsedResponseLine    parsed_response_line;
        Connection            connection;
        ConnectionData        connection_data;
        TransactionData       transaction_data;
    };

    class Handler
    {
    public:
        Handler(handler_info_t& info) : m_info(info) {}

        void operator()(
            Engine                engine,
            Engine::state_event_e event
        )
        {
            m_info.which = CB_NULL;
            m_info.engine = engine;
            m_info.event = event;
        }

        void operator()(
            Engine                engine,
            Transaction           transaction,
            Engine::state_event_e event
        )
        {
            m_info.which = CB_TRANSACTION;
            m_info.engine = engine;
            m_info.transaction = transaction;
            m_info.event = event;
        }

        void operator()(
            Engine engine,
            Transaction transaction,
            Engine::state_event_e event,
            ParsedNameValue parsed_name_value
        )
        {
            m_info.which = CB_HEADERS;
            m_info.engine = engine;
            m_info.transaction = transaction;
            m_info.event = event;
            m_info.parsed_name_value = parsed_name_value;
        }

        void operator()(
            Engine engine,
            Transaction transaction,
            Engine::state_event_e event,
            ParsedRequestLine parsed_request_line
        )
        {
            m_info.which = CB_REQUEST_LINE;
            m_info.engine = engine;
            m_info.transaction = transaction;
            m_info.event = event;
            m_info.parsed_request_line = parsed_request_line;
        }

        void operator()(
            Engine engine,
            Transaction transaction,
            Engine::state_event_e event,
            ParsedResponseLine parsed_response_line
        )
        {
            m_info.which = CB_RESPONSE_LINE;
            m_info.engine = engine;
            m_info.transaction = transaction;
            m_info.event = event;
            m_info.parsed_response_line = parsed_response_line;
        }

        void operator()(
            Engine engine,
            Engine::state_event_e event,
            Connection connection
        )
        {
            m_info.which = CB_CONNECTION;
            m_info.engine = engine;
            m_info.event = event;
            m_info.connection = connection;
        }

        void operator()(
            Engine engine,
            Engine::state_event_e event,
            ConnectionData connection_data
        )
        {
            m_info.which = CB_CONNECTION_DATA;
            m_info.engine = engine;
            m_info.event = event;
            m_info.connection_data = connection_data;
        }

        void operator()(
            Engine engine,
            Transaction transaction,
            Engine::state_event_e event,
            TransactionData transaction_data
        )
        {
            m_info.which = CB_TRANSACTION_DATA;
            m_info.engine = engine;
            m_info.transaction = transaction;
            m_info.event = event;
            m_info.transaction_data = transaction_data;
        }

    private:
        handler_info_t& m_info;
    };

    void test_tx(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        ib_hook_t* hook;
        ib_state_event_type_t ib_event =
            static_cast<ib_state_event_type_t>(event);
        info = handler_info_t();
        hook = m_ib_engine->hook[event];
        while (hook->next != NULL) {
            hook = hook->next;
        }
        EXPECT_EQ(IB_OK,
            hook->callback.tx(m_ib_engine, m_ib_transaction, ib_event, hook->cdata)
        );
        EXPECT_EQ(CB_TRANSACTION, info.which);
        EXPECT_EQ(m_ib_engine, info.engine.ib());
        EXPECT_EQ(event, info.event);
    }

    void test_null(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        ib_hook_t* hook;
        ib_state_event_type_t ib_event =
            static_cast<ib_state_event_type_t>(event);
        info = handler_info_t();
        hook = m_ib_engine->hook[event];
        while (hook->next != NULL) {
            hook = hook->next;
        }
        EXPECT_EQ(IB_OK,
            hook->callback.null(m_ib_engine, ib_event, hook->cdata)
        );
        EXPECT_EQ(CB_NULL, info.which);
        EXPECT_EQ(m_ib_engine, info.engine.ib());
        EXPECT_EQ(event, info.event);
    }

    template <typename DataType, typename MemberType>
    void test_one_argument(
        Engine::state_event_e        event,
        handler_info_t&              info,
        callback_e                   which_cb,
        MemberType handler_info_t::* which_member
    )
    {
        typedef ib_status_t (*ib_callback_t)(
            ib_engine_t*,
            ib_tx_t*,
            ib_state_event_type_t,
            DataType*,
            void*
        );

        ib_hook_t* hook;
        ib_state_event_type_t ib_event =
            static_cast<ib_state_event_type_t>(event);
        info = handler_info_t();
        hook = m_ib_engine->hook[event];
        while (hook->next != NULL) {
            hook = hook->next;
        }
        DataType ib_data;
        ib_status_t rc =
            reinterpret_cast<ib_callback_t>(hook->callback.as_void)(
                m_ib_engine, m_ib_transaction, ib_event, &ib_data, hook->cdata
            );
        EXPECT_EQ(IB_OK, rc);
        EXPECT_EQ(which_cb, info.which);
        EXPECT_EQ(m_ib_engine, info.engine.ib());
        EXPECT_EQ(m_ib_transaction, info.transaction.ib());
        EXPECT_EQ(event, info.event);
        EXPECT_EQ(&ib_data, (info.*which_member).ib());
    }

    template <typename DataType, typename MemberType>
    void test_notx_one_argument(
        Engine::state_event_e        event,
        handler_info_t&              info,
        callback_e                   which_cb,
        MemberType handler_info_t::* which_member
    )
    {
        typedef ib_status_t (*ib_callback_t)(
            ib_engine_t*,
            ib_state_event_type_t,
            DataType*,
            void*
        );

        ib_hook_t* hook;
        ib_state_event_type_t ib_event =
            static_cast<ib_state_event_type_t>(event);
        info = handler_info_t();
        hook = m_ib_engine->hook[event];
        while (hook->next != NULL) {
            hook = hook->next;
        }
        DataType ib_data;
        ib_status_t rc =
            reinterpret_cast<ib_callback_t>(hook->callback.as_void)(
                m_ib_engine, ib_event, &ib_data, hook->cdata
            );
        EXPECT_EQ(IB_OK, rc);
        EXPECT_EQ(which_cb, info.which);
        EXPECT_EQ(m_ib_engine, info.engine.ib());
        EXPECT_EQ(event, info.event);
        EXPECT_EQ(&ib_data, (info.*which_member).ib());
    }

    void test_headers_data(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        test_one_argument<ib_parsed_name_value_pair_list_t>(
            event,
            info,
            CB_HEADERS,
            &handler_info_t::parsed_name_value
        );
    }

    void test_request_line(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        test_one_argument<ib_parsed_req_line_t>(
            event,
            info,
            CB_REQUEST_LINE,
            &handler_info_t::parsed_request_line
        );
    }

    void test_response_line(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        test_one_argument<ib_parsed_resp_line_t>(
            event,
            info,
            CB_RESPONSE_LINE,
            &handler_info_t::parsed_response_line
        );
    }

    void test_connection(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        test_notx_one_argument<ib_conn_t>(
            event,
            info,
            CB_CONNECTION,
            &handler_info_t::connection
        );
    }

    void test_connection_data(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        test_notx_one_argument<ib_conndata_t>(
            event,
            info,
            CB_CONNECTION_DATA,
            &handler_info_t::connection_data
        );
    }

    void test_transaction(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        test_tx(
            event,
            info
        );
    }

    void test_transaction_data(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        test_one_argument<ib_txdata_t>(
            event,
            info,
            CB_TRANSACTION_DATA,
            &handler_info_t::transaction_data
        );
    }
};

TEST_F(TestHooks, Basic)
{
    Engine engine(m_ib_engine);

    HooksRegistrar H(engine);
    handler_info_t info;
    Handler handler(info);

    H.configuration_started(handler);
    test_null(Engine::configuration_started, info);
    H.configuration_finished(handler);
    test_null(Engine::configuration_finished, info);


    H.request_headers_data(handler);
    test_headers_data(Engine::request_headers_data, info);
    H.response_headers_data(handler);
    test_headers_data(Engine::response_headers_data, info);
    H.request_started(handler);
    test_request_line(Engine::request_started, info);
    H.response_started(handler);
    test_response_line(Engine::response_started, info);
    H.connection_started(handler);
    test_connection(Engine::connection_started, info);
    H.connection_finished(handler);
    test_connection(Engine::connection_finished, info);
    H.connection_opened(handler);
    test_connection(Engine::connection_opened, info);
    H.connection_closed(handler);
    test_connection(Engine::connection_closed, info);
    H.handle_context_connection(handler);
    test_connection(Engine::handle_context_connection, info);
    H.handle_connect(handler);
    test_connection(Engine::handle_connect, info);
    H.handle_disconnect(handler);
    test_connection(Engine::handle_disconnect, info);
    H.connection_data_in(handler);
    test_connection_data(Engine::connection_data_in, info);
    H.connection_data_out(handler);
    test_connection_data(Engine::connection_data_out, info);
    H.transaction_started(handler);
    test_transaction(Engine::transaction_started, info);
    H.transaction_process(handler);
    test_transaction(Engine::transaction_process, info);
    H.transaction_finished(handler);
    test_transaction(Engine::transaction_finished, info);
    H.handle_context_transaction(handler);
    test_transaction(Engine::handle_context_transaction, info);
    H.handle_request_headers(handler);
    test_transaction(Engine::handle_request_headers, info);
    H.handle_request(handler);
    test_transaction(Engine::handle_request, info);
    H.handle_response_headers(handler);
    test_transaction(Engine::handle_response_headers, info);
    H.handle_response(handler);
    test_transaction(Engine::handle_response, info);
    H.handle_postprocess(handler);
    test_transaction(Engine::handle_postprocess, info);
    H.request_headers(handler);
    test_transaction(Engine::request_headers, info);
    H.request_finished(handler);
    test_transaction(Engine::request_finished, info);
    H.response_headers(handler);
    test_transaction(Engine::response_headers, info);
    H.response_finished(handler);
    test_transaction(Engine::response_finished, info);
    H.transaction_data_in(handler);
    test_transaction_data(Engine::transaction_data_in, info);
    H.transaction_data_out(handler);
    test_transaction_data(Engine::transaction_data_out, info);
    H.request_body_data(handler);
    test_transaction_data(Engine::request_body_data, info);
    H.response_body_data(handler);
    test_transaction_data(Engine::response_body_data, info);
}

