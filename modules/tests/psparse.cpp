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
 * @brief IronBee --- ParserSuite Command Line Tool
 *
 * XXX
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "parser_suite.hpp"

#include <boost/scoped_array.hpp>

#include <fstream>

using namespace std;
using namespace IronBee::ParserSuite;

using parser_t = function<void(ostream&, span_t&)>;

template <typename F>
parser_t simple_parser(F f)
{
    return [f](ostream& o, span_t& input) {o << f(input);};
};

void read_all(istream& in, vector<char>& data)
{
    // XXX find a better way to do this.
    constexpr size_t c_buffer_size = 1024;
    data.clear();
    while (in) {
        size_t pre_size = data.size();
        data.resize(pre_size + c_buffer_size);
        in.read(data.data() + pre_size, c_buffer_size);
        data.resize(pre_size + in.gcount());
    }
}

int main(int argc, char **argv)
{
    const map<string, parser_t> parsers {
        {"uri",           simple_parser(&parse_uri)},
        {"request_line",  simple_parser(&parse_request_line)},
        {"response_line", simple_parser(&parse_response_line)},
        {"headers",       simple_parser(&parse_headers)},
        {"request",       simple_parser(&parse_request)},
        {"response",      simple_parser(&parse_response)}
    };

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <parser>" << endl;
        cerr << "Submit input on stdin." << endl;
        return 1;
    }

    auto i = parsers.find(argv[1]);
    if (i == parsers.end()) {
        cerr << "No such parser: " << argv[1] << endl;
        return 1;
    }

    vector<char> raw_data;
    read_all(cin, raw_data);
    span_t input(raw_data.data(), raw_data.data() + raw_data.size());

    try {
        i->second(cout, input);
    }
    catch (const IronBee::ParserSuite::error& e) {
        cout << "Error: " << endl;
        cout << diagnostic_information(e);
        return 1;
    }

    return 0;
}
