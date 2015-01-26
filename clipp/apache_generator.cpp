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
 * @brief IronBee --- CLIPP Apache Generator Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "apache_generator.hpp"

#include <clipp/parse_modifier.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>
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
#include <boost/shared_ptr.hpp>

#include <fstream>

using namespace std;

namespace {

static const string s_default_ip("0.0.0.0");
static const string s_local_ip("0.0.0.0");
static const string s_eol("\r\n");
static const string s_version("HTTP/1.0");
static const uint32_t s_remote_port = 0;
static const uint32_t s_local_port  = 0;
// 1 host
// 2 request
// 3 response
// 4 referer
// 5 user-agent
static const boost::regex s_re_line(
    "^(.+?) .+?\"(.+?)\" (.+?) .+?\"(.+?)\" \"(.+?)\"$"
);
static const boost::regex s_re_ip(
    "^\\d+\\.\\d+\\.\\d+\\.\\d+$"
);

}

namespace IronBee {
namespace CLIPP {

struct ApacheGenerator::State
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

ApacheGenerator::ApacheGenerator()
{
    // nop
}

ApacheGenerator::ApacheGenerator(const std::string& input_path) :
    m_state(boost::make_shared<State>(input_path))
{
    // nop
}

namespace  {

struct data_t
{
    string request;
    string response;
    string remote_ip;
};

typedef boost::shared_ptr<data_t> data_p;

}

bool ApacheGenerator::operator()(Input::input_p& input)
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
        Input::Buffer remote_ip;
        if (regex_match(match.str(1), s_re_ip)) {
            data->remote_ip = match.str(1);
            remote_ip = Input::Buffer(data->remote_ip);
        }
        else {
            remote_ip = Input::Buffer(s_default_ip);
        }
        input->connection.connection_opened(
            Input::Buffer(s_local_ip),
            s_local_port,
            remote_ip,
            s_remote_port
        );

        data->request =  match.str(2)                  + s_eol;
        data->request += "Referer: " + match.str(4)    + s_eol;
        data->request += "User-Agent: " + match.str(5) + s_eol;
        data->request += s_eol;
        data->response = s_version + " " + match.str(3) + s_eol;

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
