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

#include "ironbee_config_auto.h"

#include <clipp/htp_generator.hpp>
#include <clipp/parse_modifier.hpp>

#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

#include <fstream>

using namespace std;

namespace {

static const string s_remote_ip("0.0.0.0");
static const string s_local_ip("0.0.0.0");
static const uint32_t s_remote_port = 0;
static const uint32_t s_local_port  = 0;
static const string s_eol("\r\n");

}

namespace IronBee {
namespace CLIPP {

struct HTPGenerator::State
{
    State(const string& path_) :
        path(path_),
        produced_input(false)
    {
        if (path == "-") {
            input = &cin;
        }
        else {
            input = new ifstream(path.c_str(), ios::binary);
            if (! *input) {
                throw runtime_error("Could not open " + path + " for reading.");
            }
        }
    }

    ~State()
    {
        if (path != "-") {
            delete input;
        }
    }

    string   path;
    istream* input;
    bool     produced_input;
};

HTPGenerator::HTPGenerator()
{
    // nop
}

HTPGenerator::HTPGenerator(const std::string& input_path) :
    m_state(boost::make_shared<State>(input_path))
{
    // nop
}

namespace {

enum direction_e {IN, OUT};

struct tx_t
{
    string request;
    string response;
};

typedef list<tx_t> tx_list_t;

struct data_t
{
    tx_list_t txs;
};

typedef boost::shared_ptr<data_t> data_p;

}

bool HTPGenerator::operator()(Input::input_p& input)
{
    if (m_state->produced_input) {
        return false;
    }
    m_state->produced_input = true;

    if (! *m_state->input) {
        return false;
    }

    *input = Input::Input();

    input->id = m_state->path;

    data_p data = boost::make_shared<data_t>();
    input->source = data;

    string line;
    string* current_buffer = NULL;

    while (*m_state->input) {
        getline(*m_state->input, line);
        // remove CR
        size_t string_size = line.length();
        if (line[string_size-1] == '\r') {
            line.erase(string_size-1);
        }
        if (! *m_state->input) {
            break;
        }

        if (line == ">>>") {
            data->txs.push_back(tx_t());
            current_buffer = &data->txs.back().request;
        }
        else if (line == "<<<") {
            if (data->txs.empty()) {
                throw runtime_error("Out block without an In block first.");
            }
            current_buffer = &data->txs.back().response;
        }
        else if (! current_buffer) {
            throw runtime_error("Received data outside of a block.");
        }
        else {
            *current_buffer += line + s_eol;
        }
    }

    input->connection.connection_opened(
        Input::Buffer(s_local_ip),
        s_local_port,
        Input::Buffer(s_remote_ip),
        s_remote_port
    );

    BOOST_FOREACH(const tx_t& tx, data->txs) {
        input->connection.add_transaction(
            Input::Buffer(tx.request),
            Input::Buffer(tx.response)
        );
    }

    input->connection.connection_closed();

    ParseModifier()(input);

    return true;
}

} // CLIPP
} // IronBee
