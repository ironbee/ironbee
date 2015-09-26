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
 * @brief Predicate --- Dot 2 implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/predicate/dot2.hpp>
#include <ironbee/predicate/eval.hpp>
#include <ironbee/predicate/merge_graph.hpp>

#include <boost/algorithm/string/join.hpp>
#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

using namespace std;

namespace IronBee {
namespace Predicate {

namespace {

using namespace Dot2Internal;

//! Render a validation report.
void render_report(
    ostream&       out,
    const string&  report,
    const node_cp& node
)
{
    out << "  { rank = same; \"" << node
        << "\" \"report-" << node << "\" }" << endl;
    out << "  \"report-" << node << "\" ["
        << "fontsize=10, shape=none, "
        << "label=<<table border=\"0\" cellborder=\"0\">"
        << report << "</table>>];\n";
    out << "  \"" << node << "\" -> \"report-" << node << "\" ["
        << " weight=1000, dir=none, penwidth=0.5];\n";
}

//! Validation status of a node.
enum status_t {
    STATUS_OK,
    STATUS_WARN,
    STATUS_ERROR
};

/**
 * Reporter; generates Dot reports for use with render_report().
 *
 * @param[out] status   Status of node.
 * @param[out] report   Report for node.
 * @param[in]  is_error Is an error being reported?
 * @param[in]  message  Message.
 **/
void dot_reporter(
    status_t&     status,
    string&       report,
    bool          is_error,
    const string& message
)
{
    status = is_error ? STATUS_ERROR : STATUS_WARN;
    report += string("<tr><td><font color=\"") +
              (is_error ? "red" : "orange") + "\">" +
              escape_html(message) + "</font></td></tr>";
}

/**
 * Render value of a node.
 *
 * @param[out] out              Where to write.
 * @param[in]  graph_eval_state Graph evaluation state.
 * @param[in]  node             Node to write values of.
 **/
void render_value(
    ostream&              out,
    GraphEvalState&       graph_eval_state,
    const node_cp&        node
)
{
    EvalContext context;
    out << "  { rank = same; \"" << node
        << "\" \"value-" << node << "\" }" << endl
        << "  \"" << node << "\" -> \"value-" << node
        << "\" [weight=1000, dir=none, penwidth=0.5];\n"
        << "  \"value-" << node << "\" ["
        << "fontsize=10, shape=none, label=<";
    out << escape_html(graph_eval_state.value(node.get(), context).to_s());
    out << ">];" << endl;
}

} // Anonymous

namespace Dot2Internal {

void render_roots(
    ostream&          out,
    const node_cp&    node,
    root_namer_t      root_namer
)
{
    if (! root_namer) {
        return;
    }

    size_t subid = 0;
    BOOST_FOREACH(const std::string& name, root_namer(node)) {
        string id = (boost::format("%d.%p") % subid % node).str();
        out << "  \"root-" << id << "\" ["
            << "fontname=\"Times-Roman\", shape=none, label=<"
            << escape_html(name) << ">];" << endl;
        out << "  \"root-" << id << "\" -> \"" << node << "\" ["
            << "style=dotted, dir=none];" << endl;
    }
}

bool is_absorbable(
    const node_cp& node,
    root_namer_t   root_namer
)
{
    if (! node->is_literal()) {
        return false;
    }

    if (root_namer) {
        if (root_namer(node).empty()) {
            // Not a root or no root
            return node->is_literal();
        }
    }
    return true;
}

string circle_n(
    unsigned int n
)
{
    if (n == 0) {
        return "%#9450;";
    }
    else if (n > 20) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what("Cannot circle numbers above 20.")
        );
    }
    else {
        return (boost::format("&#%d;") % (9311 + n)).str();
    }
}

string escape_html(
    const string& src
)
{
    string result;
    BOOST_FOREACH(char c, src) {
        switch (c) {
            case '&':  result += "&amp;";  break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            case '<':  result += "&lt;";   break;
            case '>':  result += "&gt;";   break;
            case '\\': result += "\\\\";   break;
            default:
                result += (boost::format("%c") % c).str();
        }
    }
    return result;
}

void render_literal(
    ostream&       out,
    const node_cp& node
)
{
    render_node(out, node, "label=<" + escape_html(node->to_s()) + ">");
}

void render_node(
    ostream&       out,
    const node_cp& node,
    const string&  attrs
)
{
    out << "  \"" << node << "\" [" << attrs << "];" << endl;
}

void render_edge(
    ostream&       out,
    const node_cp& from,
    const node_cp& to,
    const string&  label
)
{
    out << "  \"" << from << "\" -> \"" << to << "\"";
    if (! label.empty()) {
        out << " [label=<" << label << ">]";
    }
    out << ";" << endl;
}

void nh_validate(
    validation_e   validate,
    ostream&       out,
    string&        extra,
    const node_cp& node
)
{
    status_t status = STATUS_OK;
    string report;

    switch (validate) {
        case VALIDATE_NONE: return;
        case VALIDATE_PRE:
            node->pre_transform(NodeReporter(
                boost::bind(
                    dot_reporter,
                    boost::ref(status),
                    boost::ref(report),
                    _1, _2
                ), node
            ));
            break;
        case VALIDATE_POST:
            node->post_transform(NodeReporter(
                boost::bind(
                    dot_reporter,
                    boost::ref(status),
                    boost::ref(report),
                    _1, _2
                ), node
            ));
            break;
    };
    switch (status) {
        case STATUS_OK:
            break;
        case STATUS_WARN:
            extra = ", style=filled, fillcolor=orange";
            break;
        case STATUS_ERROR:
            extra = ", style=filled, fillcolor=red";
            break;
    };
    if (status != STATUS_OK) {
        render_report(out, report, node);
    }
}

void nh_value(
    GraphEvalState& graph_eval_state,
    ostream&        out,
    string&         extra,
    const node_cp&  node
)
{
    EvalContext context;
    const Value value = graph_eval_state.value(node.get(), context);
    bool finished = graph_eval_state.is_finished(node.get(), context);
    list<string> styles;

    if (finished) {
        styles.push_back("diagonals");
    }

    if (value) {
        styles.push_back("filled");
        extra += ", fillcolor=\"#BDECB6\"";
        render_value(out, graph_eval_state, node);
    }

    if (! styles.empty()) {
        extra += ", style=\"" + boost::algorithm::join(styles, ",") + "\"";
    }
}

} // Dot2Internal

} // Predicate
} // IronBee
