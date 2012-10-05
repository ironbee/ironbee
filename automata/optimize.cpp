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
 * @brief IronBee --- Optimize Automata
 *
 * Apply optimizations to automata.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/intermediate.hpp>
#include <ironautomata/translate_nonadvancing.hpp>

#include <boost/program_options.hpp>

using namespace std;
using namespace IronAutomata;

int main(int argc, char **argv)
{
    namespace po = boost::program_options;

    size_t chunk_size = 0;
    enum do_translate_nonadvancing_e {
        TN_NONE,
        TN_CONSERVATIVE,
        TN_AGGRESSIVE
    };
    do_translate_nonadvancing_e do_translate_nonadvancing = TN_NONE;

    po::options_description desc("Options:");
    desc.add_options()
        ("help", "display help and exit")
        ("fast", "optimize for speed")
        ("chunk-size,s X",
            po::value<size_t>(&chunk_size),
            "set chunk size of output to X")
        ("translate-nonadvancing",
            "translate non-advancing edges [fast]")
        ("translate-nonadvancing-aggressive",
            "translate non-advancing edges, aggressive version")
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
        return 1;
    }

    if (vm.count("fast")) {
        do_translate_nonadvancing = TN_CONSERVATIVE;
    }
    if (vm.count("translate-nonadvancing")) {
        do_translate_nonadvancing = TN_CONSERVATIVE;
    }
    if (vm.count("translate-nonadvancing-aggressive")) {
        do_translate_nonadvancing = TN_AGGRESSIVE;
    }

    Intermediate::Automata automata;
    ostream_logger logger(cerr);

    Intermediate::read_automata(automata, cin, logger);

    if (do_translate_nonadvancing != TN_NONE) {
        cerr << "Compact Nonadvancing"
            << (do_translate_nonadvancing == TN_CONSERVATIVE ?
                " conservative" :
                " aggressive"
            )
            << ": ";
        cerr.flush();
        size_t num_fixes = Intermediate::translate_nonadvancing(
            automata,
            do_translate_nonadvancing == TN_AGGRESSIVE
        );
        cerr << num_fixes << endl;
    }

    Intermediate::write_automata(automata, cout);

    return 0;
}
