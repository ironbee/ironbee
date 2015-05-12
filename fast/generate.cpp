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
 * @brief IronBee --- Build fast automata.
 *
 * This builds a fast automata file from a manifest.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/buffer.hpp>
#include <ironautomata/deduplicate_outputs.hpp>
#include <ironautomata/generator/aho_corasick.hpp>
#include <ironautomata/intermediate.hpp>
#include <ironautomata/optimize_edges.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/foreach.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <fstream>
#include <iostream>
#include <map>

using namespace std;

int main(int argc, char **)
{
    namespace ia = IronAutomata;

    if (argc != 1) {
        cerr << "Usage: generate < input > output" << endl;
        return 1;
    }

    try {
        ia::Intermediate::Automata a;
        vector<string> id_vector;
        {
            ia::buffer_t index_data;
            ia::BufferAssembler index_assembler(index_data);
            typedef map<string, size_t> id_index_map_t;
            id_index_map_t id_index_map;
            string line;
            string pattern;
            string id;
            ia::Generator::aho_corasick_begin(a);
            while (cin) {
                getline(cin, line);
                if (line.empty()) {
                    continue;
                }
                size_t first_space = line.find_first_of(' ');
                if (first_space == string::npos) {
                    cerr << "Invalid manifest line: " << line << endl;
                    return 1;
                }
                pattern = line.substr(0, first_space);
                id      = line.substr(first_space + 1, string::npos);

                id_index_map_t::iterator iter = id_index_map.lower_bound(id);
                if (iter == id_index_map.end() || iter->first != id) {
                    iter = id_index_map.insert(iter, make_pair(id, id_vector.size()));
                    id_vector.push_back(id);
                }

                size_t index = iter->second;
                if (index > numeric_limits<uint32_t>::max()) {
                    cerr << "More than 2^32 indices!  Can't handle that many." << endl;
                    return 1;
                }
                index_data.clear();
                index_assembler.append_object(uint32_t(index));

                ia::Generator::aho_corasick_add_pattern(a, pattern, index_data);
            }

            ia::Generator::aho_corasick_finish(a);
        }

        ia::Intermediate::breadth_first(a, ia::Intermediate::optimize_edges);
        ia::Intermediate::deduplicate_outputs(a);

        a.metadata()["Output-Type"] = "integer";

        {
            ia::buffer_t index;
            ia::BufferAssembler assembler(index);

            BOOST_FOREACH(const string& id, id_vector) {
                // Note appending trailing NUL.
                assembler.append_bytes(
                    reinterpret_cast<const uint8_t *>(id.c_str()),
                    id.length() + 1
                );
            }
            a.metadata()["Index"] = string(index.begin(), index.end());
        }
        {
            ia::buffer_t index_size;
            ia::BufferAssembler assembler(index_size);
            assembler.append_object(uint32_t(id_vector.size()));
            a.metadata()["IndexSize"] = string(index_size.begin(), index_size.end());
        }

        ia::Intermediate::write_automata(a, cout, 0);
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
