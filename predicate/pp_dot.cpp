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
 * @brief Predicate -- Predicate Playground Dot Generator.
 *
 * This is a simple little utility to read predicate expressions and display
 * the resulting parse trees and DAG.  Input is:
 * @code
 * line   := expression | label SP expression | define
 * define := 'Define' SP name SP arglist SP body
 * @endcode
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/bfs.hpp>
#include <predicate/dag.hpp>
#include <predicate/dot2.hpp>
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

bool handle_define(
    P::CallFactory& call_factory,
    const string& name,
    const string& args,
    const string& body
);

bool handle_expr(
    const P::CallFactory& call_factory,
    const string&         expr,
    bool                  no_post_validation
);

P::node_p parse_expr(
    const P::CallFactory& call_factory,
    const string&         expr
);

void validate_graph(
    const P::MergeGraph& G,
    P::validation_e      which
);

void transform_graph(
    P::MergeGraph&        G,
    const P::CallFactory& call_factory
);

bool handle_graph_line(
    const P::CallFactory& call_factory,
    P::MergeGraph&        G,
    root_names_t&         root_names,
    const string&         expr
);

bool handle_graph_finish(
    const P::CallFactory& call_factory,
    P::MergeGraph&        G,
    root_names_t&         root_names,
    bool                  no_post_validation
);

string lookup_root_name(const root_names_t& root_names, size_t index);

struct abort_error : runtime_error
{
    abort_error() : runtime_error("abort") {}
};

int main(int argc, char **argv)
{
    using boost::bind;
    static const string c_define("Define");

    // Command line options
    namespace po = boost::program_options;

    bool expr_mode = false;
    bool graph_mode = false;
    bool no_post_validation = false;

    po::options_description desc("Options:");
    desc.add_options()
        ("help", "display help and exit")
        ("expr", po::bool_switch(&expr_mode), "expression mode")
        ("graph", po::bool_switch(&graph_mode), "graph mode")
        ("no-post-validate", po::bool_switch(&no_post_validation), "no transform")
        ;

    po::variables_map vm;
    po::store(
        po::command_line_parser(argc, argv)
            .options(desc)
            .run(),
        vm
    );
    po::notify(vm);

    if (vm.count("help") || (graph_mode && expr_mode)) {
        cout << desc << endl;
        return 1;
    }
    // End command line options

    try {
        P::CallFactory call_factory;
        string line;

        P::Standard::load(call_factory);

        // For Graph Mode
        P::MergeGraph G;
        root_names_t root_names;

        while (getline(cin, line)) {
            // Handle a define line.
            if (
                line.length() > c_define.length() &&
                line.substr(0, c_define.length()) == c_define
            ) {
                // Define line.
                size_t name_at = line.find_first_of(" ") + 1;
                size_t args_at = line.find_first_of(" ", name_at) + 1;
                size_t body_at = line.find_first_of(" ", args_at) + 1;

                if (
                    name_at == string::npos ||
                    args_at == string::npos ||
                    body_at == string::npos
                ) {
                    cerr << "ERROR: Parsing define: " << line;
                    return 1;
                }

                bool success = handle_define(
                    call_factory,
                    line.substr(name_at, args_at - name_at - 1),
                    line.substr(args_at, body_at - args_at - 1),
                    line.substr(body_at)
                );
                if (! success) {
                    return 1;
                }

                continue;
            }

            // If in expression mode and not a define line, treat as a sexpr.
            if (expr_mode) {
                bool success = handle_expr(call_factory, line, no_post_validation);
                if (! success) {
                    return 1;
                }
            }
            else {
                // Graph mode.
                bool success = handle_graph_line(call_factory, G, root_names, line);
                if (! success) {
                    return 1;
                }
            }
        }

        if (graph_mode) {
            bool success = handle_graph_finish(call_factory, G, root_names, no_post_validation);
            if (! success) {
                return 1;
            }
        }
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
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

bool handle_define(
    P::CallFactory& call_factory,
    const string& name,
    const string& args,
    const string& body
)
{
    P::node_p body_node;
    try {
        size_t i = 0;
        if (body[0] == '(') {
            body_node = P::parse_call(body, i, call_factory);
        }
        else {
            body_node = P::parse_literal(body, i);
        }

        bool duplicate = true;
        try {
            call_factory(name);
        }
        catch (IronBee::enoent) {
            duplicate = false;
        }
        if (duplicate) {
            cerr << "ERROR: Already have function named " << name << endl;
            return false;
        }
    }
    catch (const IronBee::error& e) {
        cerr << "ERROR: Error parsing body: "
             << *boost::get_error_info<IronBee::errinfo_what>(e)
             << endl;
        return false;
    }

    P::Standard::template_arg_list_t arg_list;
    {
        size_t i = 0;
        while (i != string::npos) {
            size_t next_i = args.find_first_of(',', i);
            arg_list.push_back(args.substr(i, next_i - i));
            i = args.find_first_not_of(',', next_i);
        }
    }

    call_factory.add(
        name,
        P::Standard::define_template(arg_list, body_node)
    );

    return true;
}

bool handle_expr(
    const P::CallFactory& call_factory,
    const string&         expr,
    bool                  no_post_validation
)
{
    try {
        P::MergeGraph G;
        P::node_p node;

        node = parse_expr(call_factory, expr);
        G.add_root(node);

        P::to_dot2_validate(cout, G, P::VALIDATE_PRE);

        transform_graph(G, call_factory);

        P::to_dot2_validate(cout, G,
            (no_post_validation ? P::VALIDATE_NONE : P::VALIDATE_POST)
        );
    }
    catch (const IronBee::error& e) {
        cerr << "ERROR: Expression error: "
             << *boost::get_error_info<IronBee::errinfo_what>(e)
             << endl;
        return false;
    }
    catch (abort_error) {
        // Error already reported.
        return false;
    }

    return true;
}

P::node_p parse_expr(
    const P::CallFactory& call_factory,
    const string&         expr
)
{
    size_t i = 0;

    P::node_p node = P::parse_call(expr, i, call_factory);
    if (i != expr.length() - 1) {
        // Parse failed.
        size_t pre_length  = max(i+1,               size_t(10));
        size_t post_length = max(expr.length() - i, size_t(10));
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << IronBee::errinfo_what(
                (boost::format("ERROR: Incomplete parse: %s --ERROR-- %s")
                    % expr.substr(i-pre_length, pre_length).c_str()
                    % expr.substr(i+1, post_length).c_str()
                ).str()
            )
        );
    }

    return node;
}

void validate_graph(
    const P::MergeGraph& G,
    P::validation_e      which
)
{
    bool should_abort = false;
    P::validate_graph(
        which,
        bind(report, boost::ref(should_abort), _1, _2),
        G
    );
    if (should_abort) {
        throw abort_error();
    }
}

void transform_graph(
    P::MergeGraph&        G,
    const P::CallFactory& call_factory
)
{
    bool needs_transform = true;
    bool should_abort = false;
    while (needs_transform) {
        needs_transform =
            P::transform_graph(
                bind(report, boost::ref(should_abort), _1, _2),
                G,
                call_factory,
                P::Environment()
            );
        if (should_abort) {
            BOOST_THROW_EXCEPTION(
                IronBee::einval() << IronBee::errinfo_what(
                    "Transformation failed."
                )
            );
        }
    }
}

bool handle_graph_line(
    const P::CallFactory& call_factory,
    P::MergeGraph&        G,
    root_names_t&         root_names,
    const string&         expr
)
{
    try {
        P::node_p node;
        size_t space_at = expr.find_first_of(' ');
        size_t lp_at = expr.find_first_of('(');
        size_t expr_at = 0;
        string label;

        if (space_at != string::npos && space_at < lp_at) {
            // Has label.
            label = expr.substr(0, space_at);
            expr_at = space_at + 1;
        }

        node = parse_expr(call_factory, expr.substr(expr_at));
        size_t index = G.add_root(node);
        if (label.empty()) {
            label = boost::lexical_cast<string>(index);
        }
        root_names[index] = label;
    }
    catch (const IronBee::error& e) {
        cout << "ERROR: "
             << *boost::get_error_info<IronBee::errinfo_what>(e)
             << endl;
        return false;
    }

    return true;
}

bool handle_graph_finish(
    const P::CallFactory& call_factory,
    P::MergeGraph&        G,
    root_names_t&         root_names,
    bool                  no_post_validation
)
{
    P::root_namer_t root_namer =
        bind(lookup_root_name, boost::ref(root_names), _1);
    try {
        P::to_dot2_validate(cout, G, P::VALIDATE_PRE, root_namer);

        transform_graph(G, call_factory);

        P::to_dot2_validate(cout, G,
            (no_post_validation ? P::VALIDATE_NONE : P::VALIDATE_POST),
            root_namer
        );
    }
    catch (const IronBee::error& e) {
        cout << "ERROR: "
             << *boost::get_error_info<IronBee::errinfo_what>(e)
             << endl;
        return false;
    }
    catch (abort_error) {
        // Error already reported.
        return false;
    }

    return true;
}

string lookup_root_name(const root_names_t& root_names, size_t index)
{
    root_names_t::const_iterator i = root_names.find(index);
    if (i == root_names.end()) {
        return "undefined";
    }
    else {
        return i->second;
    }
}
