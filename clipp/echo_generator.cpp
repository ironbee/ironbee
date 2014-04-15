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
 * @brief IronBee --- CLIPP Echo Generator Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <clipp/echo_generator.hpp>
#include <clipp/parse_modifier.hpp>

#include <boost/make_shared.hpp>

using namespace std;

namespace IronBee {
namespace CLIPP {

struct EchoGenerator::State
{
    State() :
        produced_input(false)
    {
        // nop
    }

    bool   produced_input;
    string id;
    string request;
};

EchoGenerator::EchoGenerator()
{
    // nop
}

EchoGenerator::EchoGenerator(
    const string& request_line
) :
    m_state(boost::make_shared<State>())
{
    m_state->id = request_line;
    m_state->request = request_line + "\r\n";
}

bool EchoGenerator::operator()(Input::input_p& out_input)
{
    if (m_state->produced_input) {
        return false;
    }

    out_input->id                = m_state->id;
    out_input->source             = m_state;
    out_input->connection = Input::Connection();
    out_input->connection.connection_opened(
        Input::Buffer(local_ip),  local_port,
        Input::Buffer(remote_ip), remote_port
    );
    out_input->connection.connection_closed();
    out_input->connection.add_transaction()
        .connection_data_in(Input::Buffer(m_state->request));

    ParseModifier()(out_input);

    m_state->produced_input = true;

    return true;
}

const string EchoGenerator::local_ip    = "1.2.3.4";
const string EchoGenerator::remote_ip   = "5.6.7.8";
const uint16_t    EchoGenerator::local_port  = 1234;
const uint16_t    EchoGenerator::remote_port = 5678;

} // CLIPP
} // IronBee
