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
#include <ironautomata/intermediate.hpp>
#include <ironautomata/optimize_edges.hpp>
#include <ironautomata/generator/aho_corasick.hpp>

#include <boost/lexical_cast.hpp>

using namespace std;

//! Main
int main(int argc, char** argv)
{
    namespace ia = IronAutomata;

    if (argc < 1 || argc > 2) {
        cout << "Usage: ac_generator [<chunk_size>]" << endl;
        return 1;
    }

    size_t chunk_size = 0;
    if (argc == 2) {
        chunk_size = boost::lexical_cast<size_t>(argv[1]);
    }

    ia::Intermediate::Automata a;
    ia::Generator::aho_corasick_begin(a);

    string s;
    while (cin) {
        getline(cin, s);
        if (! s.empty()) {
            ia::Generator::aho_corasick_add_length(a, s);
        }
    }

    ia::Generator::aho_corasick_finish(a);

    ia::Intermediate::breadth_first(a, ia::Intermediate::optimize_edges);
    ia::Intermediate::deduplicate_outputs(a);

    ia::Intermediate::write_automata(a, cout, chunk_size);
}
