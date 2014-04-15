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
 * @brief IronBee --- CLIPP Raw Generator Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "raw_generator.hpp"

#include <clipp/parse_modifier.hpp>

#include <boost/make_shared.hpp>

#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace std;

namespace IronBee {
namespace CLIPP {

namespace {

//! Remaining bytes in @a f.
size_t remaining(ifstream& f)
{
    size_t length;
    streampos current = f.tellg();
    f.seekg(0, ios::end);
    length = f.tellg();
    f.seekg(current, ios::beg);

    return length - current;
}

//! Load @a file into @a buffer.
void load(vector<char>& buffer, const string& file)
{
    if (file == "-") {
        buffer.clear();
        string line;
        while (getline(cin, line)) {
            line += "\n";
            buffer.reserve(buffer.size() + line.length());
            buffer.insert(buffer.end(), line.begin(), line.end());
        }
    }
    else {
        ifstream in(file.c_str());
        if (! in) {
            throw runtime_error("Could not read " + file);
        }
        size_t length = remaining(in);
        buffer = vector<char>(length);
        in.read(&*buffer.begin(), length);
    }
}

}

struct RawGenerator::State
{
    State() :
        produced_input(false)
    {
        // nop
    }

    bool               produced_input;
    std::string        id;
    std::vector<char>  request_buffer;
    std::vector<char>  response_buffer;
};

RawGenerator::RawGenerator()
{
    // nop
}

RawGenerator::RawGenerator(
    const std::string& request_path,
    const std::string& response_path
) :
    m_state(boost::make_shared<State>())
{
    m_state->id = request_path + "," + response_path;
    load(m_state->request_buffer,  request_path);
    load(m_state->response_buffer, response_path);
}

bool RawGenerator::operator()(Input::input_p& out_input)
{
    if (m_state->produced_input) {
        return false;
    }

    out_input->id                = m_state->id;
    out_input->connection = Input::Connection();
    out_input->connection.connection_opened(
        Input::Buffer(local_ip),  local_port,
        Input::Buffer(remote_ip), remote_port
    );
    out_input->connection.connection_closed();
    out_input->connection.add_transaction(
        Input::Buffer(
            &*m_state->request_buffer.begin(),
            m_state->request_buffer.size()
        ),
       Input::Buffer(
           &*m_state->response_buffer.begin(),
           m_state->response_buffer.size()
       )
    );

    ParseModifier()(out_input);

    m_state->produced_input = true;

    return true;
}

const std::string RawGenerator::local_ip    = "1.2.3.4";
const std::string RawGenerator::remote_ip   = "5.6.7.8";
const uint16_t    RawGenerator::local_port  = 80;
const uint16_t    RawGenerator::remote_port = 1234;

} // CLIPP
} // IronBee
