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
 * @brief IronBee --- CLIPP Suricata Generator Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <clipp/suricata_generator.hpp>
#include <clipp/parse_modifier.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wparentheses"
#pragma clang diagnostic ignored "-Wchar-subscripts"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
#include <boost/regex.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#include <boost/make_shared.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>

using namespace std;

namespace {

static const string s_default_ip("0.0.0.0");
static const string s_local_ip("0.0.0.0");
static const string s_eol("\r\n");
static const string s_version("HTTP/1.0");

// 1 uri
// 2 user-agent
// 3 referer
// 4 method
// 5 protocol
// 6 response
// 7 src ip
// 8 src port
// 9 dst ip
// 10 dst port
static const boost::regex s_re_line(
    "^.+? \\[\\*\\*\\] (.*?) \\[\\*\\*\\] (.*?) \\[\\*\\*\\] (.*?) "
    "\\[\\*\\*\\] (.*?) \\[\\*\\*\\] (.*?) \\[\\*\\*\\] (.*?) "
    "\\[\\*\\*\\] .*? \\[\\*\\*\\] (.*?):(\\d+?) -> (.*?):(\\d+?)$"
);
static const boost::regex s_re_response(
    "^(\\d+) => (.+)\n"
);

}

namespace IronBee {
namespace CLIPP {

struct SuricataGenerator::State
{
    State(const string& path) :
        prefix(path),
        line_number(0)
    {
        if (prefix == "-") {
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
        if (prefix != "-") {
            delete input;
        }
    }

    string   prefix;
    istream* input;
    size_t   line_number;
};

SuricataGenerator::SuricataGenerator()
{
    // nop
}

SuricataGenerator::SuricataGenerator(const std::string& input_path) :
    m_state(boost::make_shared<State>(input_path))
{
    // nop
}

namespace  {

struct data_t
{
    string request;
    string response;
    string local_ip;
    string remote_ip;
};

typedef boost::shared_ptr<data_t> data_p;

}

bool SuricataGenerator::operator()(Input::input_p& input)
{
    if (! *m_state->input) {
        return false;
    }

    ++m_state->line_number;

    *input = Input::Input();

    input->id = m_state->prefix + ":" +
        boost::lexical_cast<string>(m_state->line_number);

    data_p data = boost::make_shared<data_t>();
    input->source = data;

    boost::smatch match;
    string line;

    getline(*m_state->input, line);
    if (! *m_state->input) {
        return false;
    }

    if (regex_match(line, match, s_re_line)) {
        data->local_ip = match.str(9);
        data->remote_ip = match.str(7);

        input->connection.connection_opened(
            Input::Buffer(data->local_ip),
            boost::lexical_cast<uint32_t>(
                match.str(10)
            ),
            Input::Buffer(data->remote_ip),
            boost::lexical_cast<uint32_t>(
                match.str(8)
            )
        );

        data->request = match.str(4) + " " + match.str(1) + " " \
                        + match.str(5) + s_eol;
        data->request += "Referer: " + match.str(3) + s_eol;
        data->request += "User-Agent: " + match.str(2) + s_eol;
        data->request += s_eol;

        boost::smatch response_match;
        if (regex_match(match.str(6), response_match, s_re_response)) {
            data->response = match.str(5) + " " + response_match.str(1)
                + s_eol;
            data->response += "Location: " + response_match.str(2) + s_eol;
        }
        else {
            data->response = match.str(5) + " " + match.str(6) + s_eol;
        }

        input->connection.add_transaction(
            Input::Buffer(data->request),
            Input::Buffer(data->response)
        );

        input->connection.connection_closed();
    }
    else {
        throw runtime_error("Unparsed line: " + line);
    }

    ParseModifier()(input);

    return true;
}

} // CLIPP
} // IronBee
