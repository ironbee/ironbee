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
 * @brief IronBee &mdash; CLIPP IronBee Consumer Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_consumer.hpp"

#include <ironbeepp/all.hpp>
#include <boost/make_shared.hpp>

using namespace std;

using boost::make_shared;

namespace IronBee {
namespace CLIPP {

namespace {

class adapt_header :
    public unary_function<const Input::header_t, IronBee::ParsedNameValue>
{
public:
    explicit adapt_header(IronBee::MemoryPool mp) :
        m_mp( mp )
    {
        // nop
    }

    IronBee::ParsedNameValue operator()(const Input::header_t& header) const
    {
        return IronBee::ParsedNameValue::create(
            m_mp,
            IronBee::ByteString::create_alias(
                m_mp, header.first.data, header.first.length
            ),
            IronBee::ByteString::create_alias(
                m_mp, header.second.data, header.second.length
            )
        );
    }

private:
    IronBee::MemoryPool m_mp;
};

class IronBeeDelegate :
    public Input::Delegate
{
public:
    explicit
    IronBeeDelegate(IronBee::Engine engine) :
        m_engine( engine )
    {
        // nop
    }

    void connection_opened(const Input::ConnectionEvent& event)
    {
        using namespace boost;

        m_connection = IronBee::Connection::create(m_engine);

        char* local_ip = strndup(
            event.local_ip.data,
            event.local_ip.length
        );
        m_connection.memory_pool().register_cleanup(bind(free,local_ip));
        char* remote_ip = strndup(
            event.remote_ip.data,
            event.remote_ip.length
        );
        m_connection.memory_pool().register_cleanup(bind(free,remote_ip));


        m_connection.set_local_ip_string(local_ip);
        m_connection.set_local_port(event.local_port);
        m_connection.set_remote_ip_string(remote_ip);
        m_connection.set_remote_port(event.remote_port);

        m_engine.notify().connection_opened(m_connection);
    }

    void connection_closed(const Input::NullEvent& event)
    {
        if (! m_connection) {
            throw runtime_error(
                "CONNECTION_CLOSED event fired outside "
                "of connection lifetime."
            );
        }
        m_engine.notify().connection_closed(m_connection);
        m_connection = IronBee::Connection();
    };

    void connection_data_in(const Input::DataEvent& event)
    {
        if (! m_connection) {
            throw runtime_error(
                "CONNECTION_DATA_IN event fired outside "
                "of connection lifetime."
            );
        }

        // Copy because IronBee needs mutable input.
        IronBee::ConnectionData data = IronBee::ConnectionData::create(
            m_connection,
            event.data.data,
            event.data.length
        );

        m_engine.notify().connection_data_in(data);
    }

    void connection_data_out(const Input::DataEvent& event)
    {
        if (! m_connection) {
            throw runtime_error(
                "CONNECTION_DATA_IN event fired outside "
                "of connection lifetime."
            );
        }

        // Copy because IronBee needs mutable input.
        IronBee::ConnectionData data = IronBee::ConnectionData::create(
            m_connection,
            event.data.data,
            event.data.length
        );

        m_engine.notify().connection_data_out(data);
    }

    void request_started(const Input::RequestEvent& event)
    {
        if (! m_connection) {
            throw runtime_error(
                "REQUEST_STARTED event fired outside "
                "of connection lifetime."
            );
        }

        m_transaction = IronBee::Transaction::create(m_connection);

        IronBee::ParsedRequestLine prl =
            IronBee::ParsedRequestLine::create_alias(
                m_transaction,
                event.raw.data,      event.raw.length,
                event.method.data,   event.method.length,
                event.uri.data,      event.uri.length,
                event.protocol.data, event.protocol.length
            );

        m_engine.notify().request_started(m_transaction, prl);
    }

    void request_header(const Input::HeaderEvent& event)
    {
        if (! m_transaction) {
            throw runtime_error(
                "REQUEST_HEADER_FINISHED event fired outside "
                "of connection lifetime."
            );
        }

        adapt_header adaptor(m_transaction.memory_pool());
        m_engine.notify().request_header_data(
            m_transaction,
            boost::make_transform_iterator(event.headers.begin(), adaptor),
            boost::make_transform_iterator(event.headers.end(),   adaptor)
        );
    }

    void request_header_finished(const Input::NullEvent& event)
    {
        if (! m_transaction) {
            throw runtime_error(
                "REQUEST_HEADER_FINISHED event fired outside "
                "of connection lifetime."
            );
        }
        m_engine.notify().request_header_finished(m_transaction);
    }

    void request_body(const Input::DataEvent& event)
    {
        if (! m_transaction) {
            throw runtime_error(
                "REQUEST_BODY event fired outside "
                "of connection lifetime."
            );
        }

        // Copy because IronBee needs mutable input.
        char* mutable_data = strndup(event.data.data, event.data.length);
        m_connection.memory_pool().register_cleanup(
            boost::bind(free, mutable_data)
        );
        IronBee::TransactionData data =
            IronBee::TransactionData::create_alias(
                m_connection.memory_pool(),
                mutable_data,
                event.data.length
            );

        m_engine.notify().request_body_data(m_transaction, data);
    }

    void request_finished(const Input::NullEvent& event)
    {
        if (! m_transaction) {
            throw runtime_error(
                "REQUEST_FINISHED event fired outside "
                "of transaction lifetime."
            );
        }
        m_engine.notify().request_finished(m_transaction);
    }

    void response_started(const Input::ResponseEvent& event)
    {
        if (! m_transaction) {
            throw runtime_error(
                "RESPONSE_STARTED event fired outside "
                "of transaction lifetime."
            );
        }

        IronBee::ParsedResponseLine prl =
            IronBee::ParsedResponseLine::create_alias(
                m_transaction,
                event.raw.data,      event.raw.length,
                event.protocol.data, event.protocol.length,
                event.status.data,   event.status.length,
                event.message.data,  event.message.length
            );

        m_engine.notify().response_started(m_transaction, prl);
    }

    void response_header(const Input::HeaderEvent& event)
    {
        if (! m_transaction) {
            throw runtime_error(
                "RESPONSE_HEADER event fired outside "
                "of connection lifetime."
            );
        }

        adapt_header adaptor(m_transaction.memory_pool());
        m_engine.notify().response_header_data(
            m_transaction,
            boost::make_transform_iterator(event.headers.begin(), adaptor),
            boost::make_transform_iterator(event.headers.end(),   adaptor)
        );
    }

    void response_header_finished(const Input::NullEvent& event)
    {
        if (! m_transaction) {
            throw runtime_error(
                "RESPONSE_HEADER_FINISHED event fired outside "
                "of connection lifetime."
            );
        }
        m_engine.notify().response_header_finished(m_transaction);
    }

    void response_body(const Input::DataEvent& event)
    {
        if (! m_transaction) {
            throw runtime_error(
                "RESPONSE_BODY event fired outside "
                "of connection lifetime."
            );
        }

        // Copy because IronBee needs mutable input.
        char* mutable_data = strndup(event.data.data, event.data.length);
        m_connection.memory_pool().register_cleanup(
            boost::bind(free, mutable_data)
        );
        IronBee::TransactionData data =
            IronBee::TransactionData::create_alias(
                m_connection.memory_pool(),
                mutable_data,
                event.data.length
            );

        m_engine.notify().response_body_data(m_transaction, data);
    }

    void response_finished(const Input::NullEvent& event)
    {
        if (! m_transaction) {
            throw runtime_error(
                "RESPONSE_FINISHED event fired outside "
                "of connection lifetime."
            );
        }

        m_engine.notify().response_finished(m_transaction);
        m_transaction = IronBee::Transaction();
    }

private:
    IronBee::Engine      m_engine;
    IronBee::Connection  m_connection;
    IronBee::Transaction m_transaction;
};

void load_configuration(IronBee::Engine engine, const std::string& path)
{
    engine.notify().configuration_started();

    IronBee::ConfigurationParser parser
        = IronBee::ConfigurationParser::create(engine);

    parser.parse_file(path);

    parser.destroy();
    engine.notify().configuration_finished();
}

}

struct IronBeeConsumer::EngineState
{
    EngineState() :
        server_value(__FILE__, "clipp")
    {
        IronBee::initialize();
        engine = IronBee::Engine::create(server_value.get());
    }

    ~EngineState()
    {
        engine.destroy();
        IronBee::shutdown();
    }

    IronBee::Engine      engine;
    IronBee::ServerValue server_value;
};

IronBeeConsumer::IronBeeConsumer()
{
    // nop
}

IronBeeConsumer::IronBeeConsumer(const string& config_path) :
    m_engine_state(make_shared<EngineState>())
{
    load_configuration(m_engine_state->engine, config_path);
}

bool IronBeeConsumer::operator()(const Input::input_p& input)
{
    IronBeeDelegate delegate(m_engine_state->engine);
    input->connection.dispatch(delegate, true);

    return true;
}

} // CLIPP
} // IronBee
