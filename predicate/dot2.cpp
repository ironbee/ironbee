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

#include <predicate/dot2.hpp>
#include <predicate/eval.hpp>
#include <predicate/merge_graph.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {

namespace {

/**
 * Determine if @a node can be absorbed.
 *
 * An absorbable node will be included in its parents label and will not be
 * rendered as a discrete node.
 *
 * @param[in] node       Node to check.
 * @param[in] G          Graph; used to check if @a node is a root.
 * @param[in] root_namer Root namer; used to check if one is defined.
 * @return true iff @a node should be absorbed into parent rendering.
 **/
bool is_absorbable(
    const node_cp&    node,
    const MergeGraph& G,
    root_namer_t      root_namer
)
{
    if (! node->is_literal()) {
        return false;
    }

    try {
        G.root_indices(node);
    }
    catch (enoent) {
        // Not a root.
        return node->is_literal();
    }
    // Root
    return ! root_namer;
}

/**
 * Generic HTML escaping routine.
 *
 * Turns various HTML special characters into their HTML escapes.  This
 * routine should be used for any text that comes from the rest of Predicate,
 * especially user defined sexpressions that may include literals with HTML
 * escapes.
 *
 * @param[in] src Text to escape.
 * @return @a src with special characters escaped.
 **/
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

//! Construct unicode glyph for a circled number @a n (@a n <= 20).
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

//! Render a node.
void render_node(
    ostream&       out,
    const node_cp& node,
    const string&  attrs
)
{
    out << "  \"" << node << "\" [" << attrs << "];" << endl;
}

//! Render a literal.
void render_literal(
    ostream&       out,
    const node_cp& node
)
{
    render_node(out, node, "label=<" + escape_html(node->to_s()) + ">");
}

//! Render an edge.
void render_edge(
    ostream&       out,
    const node_cp& from,
    const node_cp& to,
    const string&  label = string()
)
{
    out << "  \"" << from << "\" -> \"" << to << "\"";
    if (! label.empty()) {
        out << " [label=<" << label << ">]";
    }
    out << ";" << endl;
}

//! Render roots.
void render_roots(
    ostream&          out,
    const node_cp&    node,
    const MergeGraph& G,
    root_namer_t      root_namer
)
{
    if (! root_namer) {
        return;
    }

    try {
        BOOST_FOREACH(size_t index, G.root_indices(node)) {
            string name = root_namer(index);

            out << "  \"root-" << index << "\" ["
                << "fontname=\"Times-Roman\", shape=none, label=<"
                << escape_html(name) << ">];" << endl;
            out << "  \"root-" << index << "\" -> \"" << node << "\" ["
                << "style=dotted, dir=none];" << endl;
        }
    }
    catch (enoent) {}
}

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
 * Render a Value.
 *
 * @param[out] out   Where to write.
 * @param[in]  value What to write.
 **/
void render_value(
    ostream&     out,
    const Value& value
);

/**
 * Render a list of values.
 *
 * @param[out] out    Where to write.
 * @param[in]  values What to write.
 **/
void render_valuelist(
    ostream&         out,
    const ValueList& values
)
{
    out << "<table border=\"0\">";
    BOOST_FOREACH(const Value& value, values) {
        out << "<tr><td align=\"right\">" << escape_html(value.name_as_s())
            << "</td><td align=\"left\">";
        render_value(out, value);
        out << "</td></tr>";
    }
    out << "</table>";
}

void render_value(
    ostream&     out,
    const Value& value
)
{
    if (value.type() != Value::LIST) {
        out << escape_html(value.to_s());
    }
    else {
        render_valuelist(out, value.value_as_list<Value>());
    }
}

/**
 * Render values of a node.
 *
 * @param[out] out              Where to write.
 * @param[in]  graph_eval_state Graph evaluation state.
 * @param[in]  node             Node to write values of.
 **/
void render_values(
    ostream&              out,
    const GraphEvalState& graph_eval_state,
    const node_cp&        node
)
{
    out << "  { rank = same; \"" << node
        << "\" \"value-" << node << "\" }" << endl
        << "  \"" << node << "\" -> \"value-" << node
        << "\" [weight=1000, dir=none, penwidth=0.5];\n"
        << "  \"value-" << node << "\" ["
        << "fontsize=10, shape=none, label=<";
    render_valuelist(out, graph_eval_state.values(node->index()));
    out << ">];" << endl;
}

/**
 * Node hook.
 *
 * First argument is output stream to output additional dot *before* node.
 * Second argument is a string of additional node properties.
 * Third argument is is the node itself.
 **/
typedef boost::function<void(ostream&, string&, const node_cp&)>
    node_hook_t;

/**
 * Base to_dot2() routine.
 *
 * @param[in] out        Where to write dot.
 * @param[in] G          MergeGraph, used to detect roots.
 * @param[in] initial    Initial vector for search.  If empty, will default
 *                       to all nodes in graph.
 * @param[in] root_namer How to name roots.
 * @param[in] node_hook  Additional rendering logic.
  **/
void to_dot2_base(
    ostream&            out,
    const MergeGraph&   G,
    const node_clist_t& initial,
    root_namer_t        root_namer,
    node_hook_t         node_hook
)
{
    typedef set<node_cp> node_cset_t;
    node_clist_t queue;
    node_cset_t skip;

    if (! initial.empty()) {
        queue = initial;
    }
    else {
        copy(G.roots().first, G.roots().second, back_inserter(queue));
    }

    // Header
    out << "digraph G {" << endl;
    out << "  ordering = out;" << endl;
    out << "  edge [arrowsize=0.5, fontsize=9];" << endl;
    out << "  node [fontname=Courier, penwidth=0.2, shape=rect, height=0.4];"
        << endl;

    // Body
    while (! queue.empty()) {
        node_cp node = queue.front();
        queue.pop_front();
        if (skip.count(node)) {
            continue;
        }
        skip.insert(node);

        // If node is a literal...
        if (node->is_literal()) {
            render_literal(out, node);
        }
        else {
            boost::shared_ptr<const Call> call =
                boost::dynamic_pointer_cast<const Call>(node);
            assert(call);
            string extra;

            // Let node hook run.
            if (node_hook) {
                node_hook(out, extra, node);
            }

            // Otherwise node is a call.
            if (node->children().size() > 5) {
                // High degree nodes, have no absorbption.
                render_node(out, node,
                    "label=<" + escape_html(call->name()) + ">"
                );
                BOOST_FOREACH(const node_cp& child, node->children()) {
                    render_edge(out, node, child);
                    queue.push_back(child);
                }
            }
            else {
                // Try to absorb children.
                vector<string> name;
                name.push_back("<b>" + call->name() + "</b>");
                unsigned int placeholder = 0;

                BOOST_FOREACH(const node_cp& child, node->children()) {
                    if (is_absorbable(child, G, root_namer)) {
                        if (child->to_s()[0] == '\'') {
                            name.push_back(
                                "<i>" + escape_html(child->to_s()) + "</i>"
                            );
                        }
                        else {
                            name.push_back(
                                "<font>" + escape_html(child->to_s()) +
                                "</font>"
                            );
                        }
                    }
                    else {
                        ++placeholder;
                        name.push_back(
                            "<font>" + circle_n(placeholder) + "</font>"
                        );
                        render_edge(out, node, child, circle_n(placeholder));
                        queue.push_back(child);
                    }
                }
                render_node(out, node,
                    "label=<" +
                    boost::algorithm::join(name, " ") + ">" +
                    extra
                );
            }
        }

        render_roots(out, node, G, root_namer);
    }

    // Footer
    out << "}" << endl;
}

//! Node Hook: Validate
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
                ), node, false
            ));
            break;
        case VALIDATE_POST:
            node->post_transform(NodeReporter(
                boost::bind(
                    dot_reporter,
                    boost::ref(status),
                    boost::ref(report),
                    _1, _2
                ), node, false
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

//! Node Hook: Value
void nh_value(
    const GraphEvalState& graph_eval_state,
    ostream&              out,
    string&               extra,
    const node_cp&        node
)
{
    size_t index = node->index();
    const ValueList& values = graph_eval_state.values(index);
    bool finished = graph_eval_state.is_finished(index);
    list<string> styles;

    if (finished) {
        styles.push_back("diagonals");
    }

    if (! values.empty()) {
        styles.push_back("filled");
        extra += ", fillcolor=\"#BDECB6\"";
        render_values(out, graph_eval_state, node);
    }

    if (! styles.empty()) {
        extra += ", style=\"" + boost::algorithm::join(styles, ",") + "\"";
    }
}

}

void to_dot2(
    ostream&          out,
    const MergeGraph& G,
    root_namer_t      root_namer
)
{
    to_dot2_base(out, G, node_clist_t(), root_namer, node_hook_t());
}

void to_dot2_validate(
    ostream&          out,
    const MergeGraph& G,
    validation_e      validate,
    root_namer_t      root_namer
)
{
    to_dot2_base(out, G, node_clist_t(), root_namer,
        bind(nh_validate, validate, _1, _2, _3)
    );
}

void to_dot2_value(
    ostream&              out,
    const MergeGraph&     G,
    const GraphEvalState& graph_eval_state,
    const node_clist_t&   initial,
    root_namer_t          root_namer
)
{
    to_dot2_base(
        out, G, initial, root_namer,
        boost::bind(nh_value, boost::cref(graph_eval_state), _1, _2, _3)
    );
}

} // Predicate
} // IronBee
