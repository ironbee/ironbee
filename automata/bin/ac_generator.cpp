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
 * @brief IronAutomata --- Aho-Corasick Generator
 *
 * This is a simple Aho-Corasick generator.  It is intended as an example and
 * test rather than a serious generator.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/deduplicate_outputs.hpp>
#include <ironautomata/generator/aho_corasick.hpp>
#include <ironautomata/intermediate.hpp>
#include <ironautomata/optimize_edges.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

using namespace std;

static const char* c_patterns_help =
    "Patterns provide a variety of fixed width operators that are shortcuts for\n"
    "a byte or span of bytes.  E.g., \"foo\\dbar\" is a pattern for \"foo0bar\",\n"
    "\"foo1bar\", ..., \"foo9bar\".\n"
    "\n"
    "Single Shortcuts:\n"
    "- \\\\ -- Backslash.\n"
    "- \\t -- Horizontal tab.\n"
    "- \\v -- Vertical tab.\n"
    "- \\n -- New line\n"
    "- \\r -- Carriage return.\n"
    "- \\f -- Form feed.\n"
    "- \\0 -- Null.\n"
    "- \\e -- Escape.\n"
    "- \\[ -- Left bracket.\n"
    "- \\] -- Right bracket.\n"
    "\n"
    "Parameterized Single Shortcuts:\n"
    "- \\^X -- Control character, where X is A-Z, [, \\, ], ^, _, or ?.\n"
    "- \\xXX -- Byte XX in hex.\n"
    "- \\iX -- Match lower case of X and upper case of X where X is A-Za-z.\n"
    "\n"
    "Multiple Shortcuts:\n"
    "- \\d -- Digit -- 0-9\n"
    "- \\D -- Non-Digit -- all but 0-9\n"
    "- \\h -- Hexadecimal digit -- A-Fa-f0-9\n"
    "- \\w -- Word Character -- A-Za-z0-9\n"
    "- \\W -- Non-Word Character -- All but A-Za-z0-9\n"
    "- \\a -- Alphabetic character -- A-Za-z\n"
    "- \\l -- Lowercase letters -- a-z\n"
    "- \\u -- Uppercase letters -- A-Z\n"
    "- \\s -- White space -- space, \\t\\r\\n\\v\\f\n"
    "- \\S -- Non-white space -- All but space, \\t\\r\\n\\v\\f\n"
    "- \\$ -- End of line -- \\r\\f\n"
    "- \\p -- Printable character, ASCII hex 20 through 7E.\n"
    "- \\. -- Any character.\n"
    "\n"
    " Union Shortcuts:\n"
    " - [...] -- Union of all shortcuts inside brackets.  Hyphens are treated\n"
    "            differently in unions.  A hyphen must either appear at the\n"
    "            beginning of the union or as part of a range A-B where A < B.\n"
    "            A and B may be single shortcuts.  An initial hyphen indicates\n"
    "            that a hyphen should be part of the union.\n"
    " - [^...] -- As above, but negated.\n"
    "\n"
    "Pattern based use string outputs; non-pattern based use length.\n"
    ;

//! Main
int main(int argc, char** argv)
{
    namespace po = boost::program_options;
    namespace ia = IronAutomata;

    const static string c_output_type_key("Output-Type");
    const static string c_output_type_string("string");
    const static string c_output_type_length("length");

    size_t chunk_size = 0;
    bool pattern = false;

    po::options_description desc("Options:");
    desc.add_options()
        ("help", "display help and exit")
        ("chunk-size,s X",
            po::value<size_t>(&chunk_size),
            "set chunk size of output to X")
        ("pattern,p",
            po::bool_switch(&pattern),
            "interpret inputs as AC patterns")
        ;

    po::variables_map vm;
    po::store(
        po::command_line_parser(argc, argv)
            .options(desc)
            .run(),
        vm
    );
    po::notify(vm);

    if (vm.count("help")) {
        cout << desc << endl;
        cout << c_patterns_help << endl;
        return 1;
    }

    try {
        ia::Intermediate::Automata a;
        ia::Generator::aho_corasick_begin(a);

        string s;
        while (cin) {
            getline(cin, s);
            if (! s.empty()) {
                if (! pattern) {
                    ia::Generator::aho_corasick_add_length(a, s);
                }
                else {
                    ia::Intermediate::byte_vector_t data;
                    copy(s.begin(), s.end(), back_inserter(data));
                    ia::Generator::aho_corasick_add_pattern(a, s, data);
                }
            }
        }

        ia::Generator::aho_corasick_finish(a);

        ia::Intermediate::breadth_first(a, ia::Intermediate::optimize_edges);
        ia::Intermediate::deduplicate_outputs(a);

        if (pattern) {
            a.metadata()[c_output_type_key] = c_output_type_string;
        }
        else {
            a.metadata()[c_output_type_key] = c_output_type_length;
        }


        ia::Intermediate::write_automata(a, cout, chunk_size);
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
