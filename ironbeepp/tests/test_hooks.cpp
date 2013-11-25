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
 * @brief IronBee++ Internals --- Hooks Tests
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 **/

#include <ironbeepp/hooks.hpp>
#include <ironbeepp/connection.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/transaction.hpp>
#include <ironbeepp/parsed_name_value.hpp>
#include <ironbeepp/parsed_request_line.hpp>
#include <ironbeepp/parsed_response_line.hpp>

#include <ironbeepp/test_fixture.hpp>

#include "gtest/gtest.h"

#include <ironbee/engine.h>

#include "state_notify_private.h"
#include "engine_private.h"

#include <string>
#include <sstream>

using namespace std;
using namespace IronBee;

class TestHooks : public ::testing::Test, public TestFixture
{
public:
    enum callback_e
    {
        CB_NOT_CALLED,
        CB_NULL,
        CB_HEADER_DATA,
        CB_REQUEST_LINE,
        CB_RESPONSE_LINE,
        CB_CONNECTION,
        CB_CONNECTION_DATA,
        CB_TRANSACTION,
        CB_TRANSACTION_DATA,
        CB_CONTEXT
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
        const char*           data;
        size_t                data_length;
        Context               context;
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
            m_info.which = CB_HEADER_DATA;
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
            Connection connection,
            Engine::state_event_e event
        )
        {
            m_info.which = CB_CONNECTION;
            m_info.engine = engine;
            m_info.event = event;
            m_info.connection = connection;
        }

        void operator()(
            Engine engine,
            Transaction transaction,
            Engine::state_event_e event,
            const char* data, size_t data_length
        )
        {
            m_info.which = CB_TRANSACTION_DATA;
            m_info.engine = engine;
            m_info.transaction = transaction;
            m_info.event = event;
            m_info.data = data;
            m_info.data_length = data_length;
        }

        void operator()(
            Engine engine,
            Context context,
            Engine::state_event_e event
        )
        {
            m_info.which = CB_CONTEXT;
            m_info.engine = engine;
            m_info.context = context;
            m_info.event = event;
        }

    private:
        handler_info_t& m_info;
    };


protected:
    void test_tx(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        const ib_hook_t* hook;
        ib_state_event_type_t ib_logevent =
            static_cast<ib_state_event_type_t>(event);

        info = handler_info_t();
        const ib_list_node_t *node =
            ib_list_last_const(m_engine.ib()->hooks[event]);
        EXPECT_TRUE(node != NULL);
        hook = (const ib_hook_t *)node->data;

        EXPECT_EQ(IB_OK,
            hook->callback.tx(
                m_engine.ib(), m_transaction.ib(),
                ib_logevent, hook->cbdata)
        );
        EXPECT_EQ(CB_TRANSACTION, info.which);
        EXPECT_EQ(m_engine, info.engine);
        EXPECT_EQ(event, info.event);
    }

    void test_conn(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        const ib_hook_t* hook;
        ib_state_event_type_t ib_logevent =
            static_cast<ib_state_event_type_t>(event);
        const ib_list_node_t *node =
            ib_list_last_const(m_engine.ib()->hooks[event]);
        EXPECT_TRUE(node != NULL);
        hook = (const ib_hook_t *)node->data;
        info = handler_info_t();

        EXPECT_EQ(IB_OK,
            hook->callback.conn(
                m_engine.ib(), m_connection.ib(),
                ib_logevent, hook->cbdata)
        );
        EXPECT_EQ(CB_CONNECTION, info.which);
        EXPECT_EQ(m_engine, info.engine);
        EXPECT_EQ(event, info.event);
    }

    void test_null(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        const ib_hook_t* hook;
        ib_state_event_type_t ib_logevent =
            static_cast<ib_state_event_type_t>(event);
        const ib_list_node_t *node =
            ib_list_last_const(m_engine.ib()->hooks[event]);
        EXPECT_TRUE(node != NULL);
        hook = (const ib_hook_t *)node->data;
        info = handler_info_t();

        EXPECT_EQ(IB_OK,
            hook->callback.null(m_engine.ib(), ib_logevent, hook->cbdata)
        );
        EXPECT_EQ(CB_NULL, info.which);
        EXPECT_EQ(m_engine, info.engine);
        EXPECT_EQ(event, info.event);
    }

    void test_data_argument(
        Engine::state_event_e        event,
        handler_info_t&              info,
        callback_e                   which_cb
    )
    {
        const ib_hook_t* hook;
        ib_state_event_type_t ib_logevent =
            static_cast<ib_state_event_type_t>(event);
        info = handler_info_t();
        const ib_list_node_t *node =
            ib_list_last_const(m_engine.ib()->hooks[event]);
        EXPECT_TRUE(node != NULL);
        hook = (const ib_hook_t *)node->data;
        const char d(1);
        ib_status_t rc =
            (hook->callback.txdata)(
                m_engine.ib(), m_transaction.ib(),
                ib_logevent, &d, 7, hook->cbdata
            );
        EXPECT_EQ(IB_OK, rc);
        EXPECT_EQ(which_cb, info.which);
        EXPECT_EQ(m_engine, info.engine);
        EXPECT_EQ(m_transaction, info.transaction);
        EXPECT_EQ(event, info.event);
        EXPECT_EQ(&d, info.data);
        EXPECT_EQ(7UL, info.data_length);
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

        const ib_hook_t* hook;
        ib_state_event_type_t ib_logevent =
            static_cast<ib_state_event_type_t>(event);
        info = handler_info_t();
        const ib_list_node_t *node =
            ib_list_last_const(m_engine.ib()->hooks[event]);
        EXPECT_TRUE(node != NULL);
        hook = (const ib_hook_t *)node->data;
        DataType ib_data;
        ib_status_t rc =
            reinterpret_cast<ib_callback_t>(hook->callback.as_void)(
                m_engine.ib(), m_transaction.ib(),
                ib_logevent, &ib_data, hook->cbdata
            );
        EXPECT_EQ(IB_OK, rc);
        EXPECT_EQ(which_cb, info.which);
        EXPECT_EQ(m_engine, info.engine);
        EXPECT_EQ(m_transaction, info.transaction);
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
            DataType*,
            ib_state_event_type_t,
            void*
        );

        const ib_hook_t* hook;
        ib_state_event_type_t ib_logevent =
            static_cast<ib_state_event_type_t>(event);
        info = handler_info_t();
        const ib_list_node_t *node =
            ib_list_last_const(m_engine.ib()->hooks[event]);
        EXPECT_TRUE(node != NULL);
        hook = (const ib_hook_t *)node->data;
        DataType ib_data;
        ib_status_t rc =
            reinterpret_cast<ib_callback_t>(hook->callback.as_void)(
                m_engine.ib(), &ib_data, ib_logevent, hook->cbdata
            );
        EXPECT_EQ(IB_OK, rc);
        EXPECT_EQ(which_cb, info.which);
        EXPECT_EQ(m_engine, info.engine);
        EXPECT_EQ(event, info.event);
        EXPECT_EQ(&ib_data, (info.*which_member).ib());
    }

    void test_header_data(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        test_one_argument<ib_parsed_header_t>(
            event,
            info,
            CB_HEADER_DATA,
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
        test_conn(
            event,
            info
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
        test_data_argument(
            event,
            info,
            CB_TRANSACTION_DATA
        );
    }

    void test_context(
        Engine::state_event_e event,
        handler_info_t&       info
    )
    {
        test_notx_one_argument<ib_context_t>(
            event,
            info,
            CB_CONTEXT,
            &handler_info_t::context
        );
    }
};

// Need to be static as we'll be used in engine destruction.

static TestHooks::handler_info_t s_info;
static TestHooks::Handler s_handler(s_info);

TEST_F(TestHooks, Basic)
{
    HooksRegistrar H(m_engine);

    H.request_header_data(s_handler);
    test_header_data(Engine::request_header_data, s_info);
    H.response_header_data(s_handler);
    test_header_data(Engine::response_header_data, s_info);
    H.request_started(s_handler);
    test_request_line(Engine::request_started, s_info);
    H.response_started(s_handler);
    test_response_line(Engine::response_started, s_info);
    H.connection_started(s_handler);
    test_connection(Engine::connection_started, s_info);
    H.connection_finished(s_handler);
    test_connection(Engine::connection_finished, s_info);
    H.connection_opened(s_handler);
    test_connection(Engine::connection_opened, s_info);
    H.connection_closed(s_handler);
    test_connection(Engine::connection_closed, s_info);
    H.handle_context_connection(s_handler);
    test_connection(Engine::handle_context_connection, s_info);
    H.handle_connect(s_handler);
    test_connection(Engine::handle_connect, s_info);
    H.handle_disconnect(s_handler);
    test_connection(Engine::handle_disconnect, s_info);
    H.transaction_started(s_handler);
    test_transaction(Engine::transaction_started, s_info);
    H.transaction_process(s_handler);
    test_transaction(Engine::transaction_process, s_info);
    H.transaction_finished(s_handler);
    test_transaction(Engine::transaction_finished, s_info);
    H.handle_context_transaction(s_handler);
    test_transaction(Engine::handle_context_transaction, s_info);
    H.handle_request_header(s_handler);
    test_transaction(Engine::handle_request_header, s_info);
    H.handle_request(s_handler);
    test_transaction(Engine::handle_request, s_info);
    H.handle_response_header(s_handler);
    test_transaction(Engine::handle_response_header, s_info);
    H.handle_response(s_handler);
    test_transaction(Engine::handle_response, s_info);
    H.handle_postprocess(s_handler);
    test_transaction(Engine::handle_postprocess, s_info);
    H.handle_logging(s_handler);
    test_transaction(Engine::handle_logging, s_info);
    H.request_header_finished(s_handler);
    test_transaction(Engine::request_header_finished, s_info);
    H.request_finished(s_handler);
    test_transaction(Engine::request_finished, s_info);
    H.response_header_finished(s_handler);
    test_transaction(Engine::response_header_finished, s_info);
    H.response_finished(s_handler);
    test_transaction(Engine::response_finished, s_info);
    H.request_body_data(s_handler);
    test_transaction_data(Engine::request_body_data, s_info);
    H.response_body_data(s_handler);
    test_transaction_data(Engine::response_body_data, s_info);
    H.context_open(s_handler);
    test_context(Engine::context_open, s_info);
    H.context_close(s_handler);
    test_context(Engine::context_close, s_info);
    H.context_destroy(s_handler);
    test_context(Engine::context_destroy, s_info);
}

