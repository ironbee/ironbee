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
 * @brief IronBee --- Output Intermediate Format as DOT.
 *
 * This utility streams protobuf.  There is a very similar routine,
 * intermediate_to_dot() which writes out an Automata.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/intermediate.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#include <fstream>

using namespace std;
using namespace IronAutomata::Intermediate;

/**
 * Convert a string to a vector of its bytes.
 *
 * @param[in]  bytes String of bytes.
 * @param[out] vec   Vector to write to.
 */
void bytes_to_vec(const std::string& bytes, vector<uint8_t>& vec)
{
    vec.clear();
    vec.reserve(bytes.size());
    vec.insert(vec.begin(), bytes.begin(), bytes.end());
}

/**
 * Limited conversion of a PB::Edge to edge_t.
 *
 * The conversion is minimal and should only be used to follow up with
 * edge value iterators.
 *
 * @param[in]  pb_edge Protobuf Edge.
 * @param[out] edge    Intermediate edge.
 */
void pb_edge_to_edge(const PB::Edge& pb_edge, Edge& edge)
{
    edge.clear();

    if (pb_edge.has_values_bm() && pb_edge.has_values()) {
        throw runtime_error(
            "Edge in chunk with both values and values bitmap."
        );
    }

    if (pb_edge.has_values_bm()) {
        bytes_to_vec(pb_edge.values_bm(), edge.bitmap());
    }
    else if (pb_edge.has_values()) {
        bytes_to_vec(pb_edge.values(), edge.vector());
    }
    else {
        throw runtime_error(
            "Edge in chunk with neither values or values_bm."
        );
    }
}

/**
 * Write the content of a string with escaping.
 *
 * Any non-printable character will be printed as decimal values in angle
 * brackets.
 *
 * @param[in] o Ostream to write to.
 * @param[in] v Vector to write.
 */
void output_content(ostream& o, const string& v)
{
    BOOST_FOREACH(char c, v) {
        switch (c) {
        case '&': o << "&amp;"; break;
        case '"': o << "&quot;"; break;
        case '\'': o << "&apos;"; break;
        case '<': o << "&lt;"; break;
        case '>': o << "&gt;"; break;
        case '\\': o << "\\\\"; break;
        default:
            if (c < 32 || c > 126) {
                o << boost::format("&lang;%d&rang;") % uint32_t(c);
            }
            else {
                o << boost::format("%c") % c;
            }
        }
    }
}

//! Main.
int main(int argc, char** argv)
{
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <automata>" << endl;
        return 1;
    }

    ifstream input(argv[1]);
    if (! input) {
        cerr << "Error opening " << argv[1] << " for reading." << endl;
        return 1;
    }

    try {
        cout << "digraph A {" << endl;
        PB::Chunk chunk;
        bool first_node = true;
        for (;;) {
            bool at_eof = false;
            try {
                at_eof = ! read_chunk(input, chunk);
            }
            catch (const exception& e) {
                cerr << e.what() << endl;
                return 1;
            }

            if (at_eof) {
                break;
            }

            Edge edge;
            BOOST_FOREACH(const PB::Node& node, chunk.nodes()) {
                cout << "  " << node.id() << " [label=\"" << node.id() << "\"";

                if (first_node) {
                    first_node = false;
                    cout << ", shape=diamond";
                }
                cout << "];" << endl;

                BOOST_FOREACH(const PB::Edge& pb_edge, node.edges()) {
                    try {
                        pb_edge_to_edge(pb_edge, edge);
                    }
                    catch (const exception& e) {
                        cerr << e.what() << endl;
                        return 1;
                    }
                    cout << "  " << node.id() << " -> " << pb_edge.target()
                         << " [weight=1000, label=\"";
                    if (edge.epsilon()) {
                        cout << "&epsilon;";
                    }
                    else {
                        BOOST_FOREACH(uint8_t c, edge) {
                            output_content(cout, (boost::format("%c") % c).str());
                        }
                    }
                    if (pb_edge.has_advance() && ! pb_edge.advance()) {
                        cout << "\", color=red";
                    }
                    cout << "\"];" << endl;
                }

                if (node.has_default_target()) {
                    cout << "  " << node.id() << " -> " << node.default_target()
                         << " [style=dashed, label=\"default\"";
                    if (
                        node.has_advance_on_default() &&
                        ! node.advance_on_default()
                    ) {
                        cout << ", color=red";
                    }
                    cout << "];" << endl;
                }

                if (node.has_first_output()) {
                    cout << "  " << node.id() << " -> output"
                         << node.first_output() << " [style=dotted];" << endl;
                }
            }

            BOOST_FOREACH(const PB::Output& output, chunk.outputs()) {
                cout << "  output" << output.id() << " [shape=box, label=\"";
                output_content(cout, output.content());
                cout << "\"];" << endl;
                if (output.has_next()) {
                    cout << "  output" << output.id() << " -> output"
                         << output.next() << " [style=dotted];" << endl;
                }
            }
        }
        cout << "}" << endl;
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
