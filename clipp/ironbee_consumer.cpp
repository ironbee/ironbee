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

void load_configuration(IronBee::Engine engine, const std::string& path)
{
    engine.notify().configuration_started();

    IronBee::ConfigurationParser parser
        = IronBee::ConfigurationParser::create(engine);

    parser.parse_file(path);

    parser.destroy();
    engine.notify().configuration_finished();
}

IronBee::Connection open_connection(
    IronBee::Engine engine,
    const input_t& input
)
{
    using namespace boost;

    IronBee::Connection conn
        = IronBee::Connection::create(engine);

    char* local_ip = strndup(
        input.local_ip.data,
        input.local_ip.length
    );
    conn.memory_pool().register_cleanup(bind(free,local_ip));
    char* remote_ip = strndup(
        input.remote_ip.data,
        input.remote_ip.length
    );
    conn.memory_pool().register_cleanup(bind(free,remote_ip));


    conn.set_local_ip_string(local_ip);
    conn.set_local_port(input.local_port);
    conn.set_remote_ip_string(remote_ip);
    conn.set_remote_port(input.remote_port);

    conn.engine().notify().connection_opened(conn);

    return conn;
}

void data_in(IronBee::Connection connection, buffer_t request)
{
    // Copy because IronBee needs mutable input.
    IronBee::ConnectionData data = IronBee::ConnectionData::create(
        connection,
        request.data,
        request.length
    );

    connection.engine().notify().connection_data_in(data);
}

void data_out(IronBee::Connection connection, buffer_t response)
{
    // Copy because IronBee needs mutable input.
    IronBee::ConnectionData data = IronBee::ConnectionData::create(
        connection,
        response.data,
        response.length
    );

    connection.engine().notify().connection_data_in(data);
}

void close_connection(IronBee::Connection connection)
{
    connection.engine().notify().connection_closed(connection);
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

bool IronBeeConsumer::operator()(const input_t& input)
{
    IronBee::Connection connection =
        open_connection(m_engine_state->engine, input);
    BOOST_FOREACH(
        const input_t::transaction_t& transaction,
        input.transactions
    ) {
        data_in(connection, transaction.request);
        data_out(connection, transaction.response);
    }
    close_connection(connection);

    return true;
}

} // CLIPP
} // IronBee
