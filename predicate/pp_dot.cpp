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
 * @brief Predicate -- Predicate Playground
 *
 * This is a simple little utility to read predicate expressions and display
 * the resulting parse trees and DAG.  Input is one predicate expression per
 * line with an optional label: LABEL EXPRESSION.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/bfs.hpp>
#include <predicate/dag.hpp>
#include <predicate/dot.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/parse.hpp>
#include <predicate/pre_eval_graph.hpp>
#include <predicate/standard.hpp>
#include <predicate/standard_template.hpp>
#include <predicate/transform_graph.hpp>
#include <predicate/validate_graph.hpp>

#include <boost/format.hpp>
#include <boost/program_options.hpp>

using namespace std;
namespace P = IronBee::Predicate;

//! Map of root index to name.
typedef map<size_t, string> root_names_t;

/**
 * Node decorator for to_dot().
 *
 * Labels root nodes with all names.
 *
 * @sa Predicate::dot_node_decorator_t
 *
 * @param[in] names Root name map.
 * @param[in] G     Merge graph.  Need to fetch indices of @a node.
 * @param[in] node  Node to decorate.
 * @return Decoration string.
 **/
string decorate_node(
    const root_names_t&                names,
    const P::MergeGraph&               G,
    const IronBee::Predicate::node_cp& node
);

/**
 * Validation reporter.
 *
 * @param[out] should_abort Set to true on error.
 * @param[in]  is_error     Is this an error?
 * @param[in]  message      Message.
 **/
void report(
    bool&         should_abort,
    bool          is_error,
    const string& message
);

int main(int argc, char **argv)
{
    using boost::bind;

    // Command line options
    namespace po = boost::program_options;

    bool parse_only = false;

    po::options_description desc("Options:");
    desc.add_options()
        ("help", "display help and exit")
        ("parse", po::bool_switch(&parse_only), "only display parse trees")
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
    // End command line options

    P::MergeGraph G;
    P::CallFactory call_factory;
    string expr;
    P::node_list_t roots;
    root_names_t root_names;
    P::dot_node_decorator_t decorator =
        bind(decorate_node, boost::ref(root_names), boost::ref(G), _1);

    P::Standard::load(call_factory);

    while (getline(cin, expr)) {
        string label;

        cout << "Read " << expr << endl;
        size_t i = expr.find_first_of(" ");
        size_t j = expr.find_first_of("(");
        if (i == string::npos || j < i) {
            i = 0;
        }
        else {
            label = expr.substr(0, i);
            ++i;
        }

        P::node_p parse_tree;
        try {
            parse_tree = P::parse_call(expr, i, call_factory);
        }
        catch (const IronBee::error& e) {
            cerr << "ERROR: "
                 << *boost::get_error_info<IronBee::errinfo_what>(e)
                 << endl;
            continue;
        }
        if (i != expr.length() - 1) {
            // Parse failed.
            size_t pre_length  = max(i+1,               size_t(10));
            size_t post_length = max(expr.length() - i, size_t(10));
            cerr
                << boost::format(
                    "ERROR: Incomplete parse: %s --ERROR-- %s"
                )
                % expr.substr(i-pre_length, pre_length).c_str()
                % expr.substr(i+1, post_length).c_str()
                << endl;
        }

        cout << "Parsed to:" << endl;
        P::to_dot(cout, parse_tree);

        if (parse_only) {
            continue;
        }

        size_t index = G.add_root(parse_tree);
        roots.push_back(parse_tree);
        root_names[index] = label;

        cout << "Added as index " << index << " with label " << label << endl;
        P::to_dot(
            cout,
            G.roots().first, G.roots().second,
            decorator
        );
    }

    if (parse_only) {
        return 0;
    }

    cout << "Validating..." << endl;
    {
        bool should_abort = false;
        P::validate_graph(
            P::PRE_TRANSFORM,
            bind(report, boost::ref(should_abort), _1, _2),
            G
        );
        if (should_abort) {
            return 1;
        }
    }

    cout << "Transforming..." << endl;
    {
        bool needs_transform = true;
        bool should_abort = false;
        size_t pass_number = 0;
        while (needs_transform) {
            ++pass_number;
            cout << "Pass " << pass_number << endl;
            needs_transform =
                P::transform_graph(
                    bind(report, boost::ref(should_abort), _1, _2),
                    G,
                    call_factory
                );
            if (should_abort) {
                return 1;
            }
            if (needs_transform) {
                P::to_dot(
                    cout,
                    G.roots().first, G.roots().second,
                    decorator
                );
            }
            else {
                cout << "No change." << endl;
            }
        }
    }

    cout << "Validating..." << endl;
    {
        bool should_abort = false;
        P::validate_graph(
            P::POST_TRANSFORM,
            bind(report, boost::ref(should_abort), _1, _2),
            G
        );
        if (should_abort) {
            return 1;
        }
    }

    return 0;
}

string decorate_node(
    const root_names_t&                names,
    const P::MergeGraph&               G,
    const IronBee::Predicate::node_cp& node
)
{
    string label;

    try {
        BOOST_FOREACH(size_t index, G.root_indices(node)) {
            root_names_t::const_iterator i = names.find(index);
            if (i != names.end()) {
                label += i->second + "\\n";
            }
        }
    }
    catch (IronBee::enoent) {}

    P::call_cp as_call = boost::dynamic_pointer_cast<const P::Call>(node);
    if (as_call) {
        label += as_call->name();
    }
    else {
        label += node->to_s();
    }
    return "label=\"" + label + "\"";
}

void report(
    bool&         should_abort,
    bool          is_error,
    const string& message
)
{
    cout << (is_error ? "ERROR" : "WARNING") << ": " << message << endl;
    if (is_error) {
        should_abort = true;
    }
}
