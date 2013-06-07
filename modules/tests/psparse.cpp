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
 * Simple command line tool to feed input through ParserSuite.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "parser_suite.hpp"

#include <boost/scoped_array.hpp>

#include <chrono>
#include <fstream>

using namespace std;
using namespace IronBee::ParserSuite;

using parser_t = function<void(ostream&, span_t&, uint64_t&)>;

template <typename F>
parser_t simple_parser(F f)
{
    return [f](ostream& o, span_t& input, uint64_t& elapsed)
    {
        chrono::time_point<chrono::high_resolution_clock> start;
        chrono::time_point<chrono::high_resolution_clock> end;
        start = chrono::high_resolution_clock::now();
        auto result = f(input);
        end = chrono::high_resolution_clock::now();

        elapsed +=
            chrono::duration_cast<chrono::microseconds>(end-start).count();
        o <<  result;
    };
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
    using namespace placeholders;

    static const map<string, parser_t> parsers {
        {"uri",           simple_parser(&parse_uri)},
        {"request_line",  simple_parser(&parse_request_line)},
        {"response_line", simple_parser(&parse_response_line)},
        {"headers",       simple_parser(&parse_headers)},
        {"request",       simple_parser(&parse_request)},
        {"response",      simple_parser(&parse_response)},
        {"authority",     simple_parser(&parse_authority)},
        {"path",          simple_parser(bind(&parse_path, _1, '/', '.'))}
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

    const uint64_t total_bytes = input.end() - input.begin();
    uint64_t total_elapsed = 0;
    uint64_t num_runs = 0;
    const char* old_begin = input.begin();
    while (! input.empty()) {
        uint64_t elapsed = 0;
        try {
            i->second(cout, input, elapsed);
        }
        catch (const IronBee::ParserSuite::error& e) {
            cout << "Error: " << endl;
            cout << diagnostic_information(e);
            return 1;
        }
        ++num_runs;
        total_elapsed += elapsed;
        cout << "elapsed: " << elapsed << " us" << endl;
        cout << "consumed: " << input.begin() - old_begin << " bytes" << endl;
        old_begin = input.begin();
    }
    cout << "total_elapsed: " << total_elapsed << " us" << endl;
    cout << "mean_elapsed: " << total_elapsed / num_runs << " us" << endl;
    cout << "rate: " << total_bytes / (double(total_elapsed) / 1e6) << " bps" << endl;
    return 0;
}
