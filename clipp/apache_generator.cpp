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
 * @brief IronBee &mdash; CLIPP Apache Generator Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "apache_generator.hpp"

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
using boost::make_shared;

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
        input(path.c_str(), ios::binary),
        line_number(0)
    {
        if (! input) {
            throw runtime_error("Could not open " + path + " for reading.");
        }
    }

    string prefix;
    ifstream input;
    size_t line_number;
};

ApacheGenerator::ApacheGenerator()
{
    // nop
}

ApacheGenerator::ApacheGenerator(const std::string& input_path) :
    m_state(make_shared<State>(input_path))
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

buffer_t s_to_buf(const string& s)
{
    return buffer_t(s.data(), s.length());
}

}

bool ApacheGenerator::operator()(input_t& input)
{
    if (! m_state->input) {
        return false;
    }

    ++m_state->line_number;

    input.id = m_state->prefix + ":" +
        boost::lexical_cast<string>(m_state->line_number);

    input.local_ip    = s_to_buf(s_local_ip);
    input.local_port  = s_local_port;
    input.remote_port = s_remote_port;

    data_p data = boost::make_shared<data_t>();
    input.source = data;

    boost::smatch match;
    string line;

    getline(m_state->input, line);
    if (! m_state->input) {
        return false;
    }

    if (regex_match(line, match, s_re_line)) {
        data->request =  match.str(2)                  + s_eol;
        data->request += "Referer: " + match.str(4)    + s_eol;
        data->request += "User-Agent: " + match.str(5) + s_eol;
        data->request += s_eol;

        if (regex_match(match.str(1), s_re_ip)) {
            data->remote_ip = match.str(1);
            input.remote_ip = s_to_buf(data->remote_ip);
        }
        else {
            input.remote_ip = s_to_buf(s_default_ip);
        }

        data->response = s_version + " " + match.str(3) + s_eol;

        input.transactions.clear();
        input.transactions.push_back(
            input_t::transaction_t(
                s_to_buf(data->request),
                s_to_buf(data->response)
            )
        );
    }
    else {
        throw runtime_error("Unparsed line: " + line);
    }

    return true;
}

} // CLIPP
} // IronBee
