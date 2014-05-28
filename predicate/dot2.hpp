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
 * @brief Predicate --- Output DAG to GraphViz dot; alternative.
 *
 * Alternative graphviz renderer.
 *
 * These renderers are designed to generate pretty and useful graphs for
 * consumption by predicate expression writers.  In contrast, to_dot() is a
 * more low level routine designed for use by Predicate developers.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__DOT2__
#define __PREDICATE__DOT2__

#include <predicate/dag.hpp>
#include <predicate/validate_graph.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/function.hpp>

namespace IronBee {
namespace Predicate {

/**
 * Function to translate root nodes into root names.
 *
 * Should return empty list if not a root.
 **/
typedef boost::function<std::list<std::string>(const node_cp&)> root_namer_t;

/**
 * Write graph out to @a out in GraphViz format.
 *
 * @tparam Iterator Type of begin and end.
 * @param[in] out        Where to write output.
 * @param[in] begin      Beginning of roos.
 * @param[in] end        End of roots.
 * @param[in] root_namer Detects roots and provides names.
 **/
template <typename Iterator>
void to_dot2(
    std::ostream& out,
    Iterator      begin,
    Iterator      end,
    root_namer_t  root_namer = root_namer_t()
);

/**
 * Write graph out to @a out in GraphViz format with validation results.
 *
 * @tparam Iterator Type of begin and end.
 * @param[in] out        Where to write output.
 * @param[in] begin      Beginning of roos.
 * @param[in] end        End of roots.
 * @param[in] validate   What, if any, validation to do.  Validation results
 *                       will color their respective nodes and attach the
 *                       messages to the side of the node.
 * @param[in] root_namer Detects roots and provides names.
 **/
template <typename Iterator>
void to_dot2_validate(
    std::ostream& out,
    Iterator      begin,
    Iterator      end,
    validation_e  validate,
    root_namer_t  root_namer = root_namer_t()
);

/**
 * Write graph out to @a out in GraphViz format with values.
 *
 * @tparam Iterator Type of begin and end.
 * @param[in] out        Where to write output.
 * @param[in] begin      Beginning of roos.
 * @param[in] end        End of roots.
 * @param[in] graph_eval_state Evaluation state of graph to render.
 * @param[in] root_namer Detects roots and provides names.
 **/
template <typename Iterator>
void to_dot2_value(
    std::ostream&         out,
    Iterator              begin,
    Iterator              end,
    const GraphEvalState& graph_eval_state,
    root_namer_t          root_namer
);

/* Implementation. */
/// @cond Internal

namespace Dot2Internal {

/**
 * Determine if @a node can be absorbed.
 *
 * An absorbable node will be included in its parents label and will not be
 * rendered as a discrete node.
 *
 * @param[in] node       Node to check.
 * @param[in] root_namer Root namer; used to check if one is defined.
 * @return true iff @a node should be absorbed into parent rendering.
 **/
bool is_absorbable(
    const node_cp& node,
    root_namer_t   root_namer
);

//! Construct unicode glyph for a circled number @a n (@a n <= 20).
std::string circle_n(
    unsigned int n
);

//! Render a literal.
void render_literal(
    std::ostream&  out,
    const node_cp& node
);

//! Render a node.
void render_node(
    std::ostream&      out,
    const node_cp&     node,
    const std::string& attrs
);

//! Render an edge.
void render_edge(
    std::ostream&               out,
    const node_cp&              from,
    const node_cp&              to,
    const std::string&  label = std::string()
);

//! Render roots.
void render_roots(
    std::ostream&  out,
    const node_cp& node,
    root_namer_t   root_namer
);

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
std::string escape_html(
    const std::string& src
);

/**
 * Node hook.
 *
 * First argument is output stream to output additional dot *before* node.
 * Second argument is a string of additional node properties.
 * Third argument is is the node itself.
 **/
typedef boost::function<
    void(
        std::ostream&,
        std::string&,
        const node_cp&
    )
> node_hook_t;

/**
 * Base to_dot2() routine.
 *
 * @tparam Iterator Type of begin and end.
 * @param[in] out        Where to write output.
 * @param[in] begin      Beginning of roos.
 * @param[in] end        End of roots.
 * @param[in] root_namer How to name roots.
 * @param[in] node_hook  Additional rendering logic.
 **/
template <typename Iterator>
void to_dot2_base(
    std::ostream& out,
    Iterator      begin,
    Iterator      end,
    root_namer_t  root_namer,
    node_hook_t   node_hook
)
{
    typedef std::set<node_cp> node_cset_t;
    node_clist_t queue;
    node_cset_t skip;

    copy(begin, end, std::back_inserter(queue));

    // Header
    out << "digraph G {" << std::endl;
    out << "  ordering = out;" << std::endl;
    out << "  edge [arrowsize=0.5, fontsize=9];" << std::endl;
    out << "  node [fontname=Courier, penwidth=0.2, shape=rect, height=0.4];"
        << std::endl;

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
            std::string extra;

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
                std::vector<std::string> name;
                name.push_back("<b>" + call->name() + "</b>");
                unsigned int placeholder = 0;

                BOOST_FOREACH(const node_cp& child, node->children()) {
                    if (is_absorbable(child, root_namer)) {
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

        render_roots(out, node, root_namer);
    }

    // Footer
    out << "}" << std::endl;
}

//! Node Hook: Value
void nh_value(
    const GraphEvalState& graph_eval_state,
    std::ostream&         out,
    std::string&          extra,
    const node_cp&        node
);

//! Node Hook: Validate
void nh_validate(
    validation_e   validate,
    std::ostream&  out,
    std::string&   extra,
    const node_cp& node
);

} // Dot2Internal

template <typename Iterator>
void to_dot2(
    std::ostream& out,
    Iterator      begin,
    Iterator      end,
    root_namer_t  root_namer
)
{
    Dot2Internal::to_dot2_base(
        out, begin, end, root_namer,
        Dot2Internal::node_hook_t()
    );
}

template <typename Iterator>
void to_dot2_validate(
    std::ostream& out,
    Iterator      begin,
    Iterator      end,
    validation_e  validate,
    root_namer_t  root_namer
)
{
    Dot2Internal::to_dot2_base(
        out, begin, end, root_namer,
        bind(
            Dot2Internal::nh_validate,
            validate,
            _1, _2, _3
        )
    );
}

template <typename Iterator>
void to_dot2_value(
    std::ostream&         out,
    Iterator              begin,
    Iterator              end,
    const GraphEvalState& graph_eval_state,
    root_namer_t          root_namer
)
{
    Dot2Internal::to_dot2_base(
        out, begin, end, root_namer,
        boost::bind(
            Dot2Internal::nh_value,
            boost::cref(graph_eval_state),
            _1, _2, _3
        )
    );
}

/// @endcond

} // Predicate
} // IronBee

#endif
