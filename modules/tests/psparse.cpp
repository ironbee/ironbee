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

#include <ironbee/module/parser_suite.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>
#include <boost/chrono.hpp>
#include <boost/function.hpp>
#include <boost/scoped_array.hpp>
#include <boost/type_traits.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <fstream>

#include <stdint.h>

using namespace std;
using namespace IronBee::ParserSuite;

typedef boost::function<void(ostream&, span_t&, uint64_t&)> parser_t;

template <typename R>
void simple_parser_func(
    boost::function<R(span_t&)> f,
    ostream&                    o,
    span_t&                     input,
    uint64_t&                   elapsed
)
{
    using namespace boost::chrono;

    time_point<high_resolution_clock> start;
    time_point<high_resolution_clock> end;
    start = high_resolution_clock::now();
    R result = f(input);
    end = high_resolution_clock::now();

    elapsed += duration_cast<microseconds>(end-start).count();
    o <<  result;
}

template <typename F>
parser_t simple_parser(F f)
{
    // Note use of protect.  Otherwise, bind will have type errors as it
    // fails to disambiguate the outer _1 from the inner _1.
    return boost::bind(
        simple_parser_func<typename F::result_type>,
        boost::protect(f),
        _1, _2, _3
    );
};

template <typename R>
parser_t simple_parser(R (*f)(span_t&))
{
    return simple_parser(boost::function<R(span_t&)>(f));
}

void read_all(istream& in, vector<char>& data)
{
    static const size_t c_buffer_size = 1024;
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
    typedef map<string, parser_t> parsers_t;
    parsers_t parsers;

    parsers["uri"]           = simple_parser(parse_uri);
    parsers["request_line"]  = simple_parser(parse_request_line);
    parsers["response_line"] = simple_parser(parse_response_line);
    parsers["headers"]       = simple_parser(parse_headers);
    parsers["request"]       = simple_parser(parse_request);
    parsers["response"]      = simple_parser(parse_response);
    parsers["authority"]     = simple_parser(parse_authority);
    parsers["path"] =
        simple_parser(boost::bind(parse_path, _1, '/', '.'));

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <parser>" << endl;
        cerr << "Submit input on stdin." << endl;
        return 1;
    }

    parsers_t::const_iterator i = parsers.find(argv[1]);
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
        catch (const boost::exception& e) {
            cout << "Error: " << endl;
            cout << diagnostic_information(e);
            return 1;
        }
        catch (const exception& e) {
            cout << "Error: " << e.what() << endl;
            return 1;
        }
        if (input.begin() == old_begin) {
            cout << "Error: No progress made." << endl;
            return 1;
        }

        ++num_runs;
        total_elapsed += elapsed;
        cout << "elapsed: " << elapsed << " us" << endl;
        cout << "consumed: " << input.begin() - old_begin << " bytes" << endl;
        old_begin = input.begin();
    }
    cout << "total_elapsed: " << total_elapsed << " us" << endl;
    if (num_runs > 0) {
        cout << "mean_elapsed: " << total_elapsed / num_runs << " us" << endl;
    }
    cout << "rate: " << total_bytes / (double(total_elapsed) / 1e6) << " bps" << endl;
    return 0;
}
