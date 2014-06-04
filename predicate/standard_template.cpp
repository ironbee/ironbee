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
 * @brief Predicate --- Standard Template Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/predicate/standard_template.hpp>

#include <ironbee/predicate/bfs.hpp>
#include <ironbee/predicate/call_helpers.hpp>
#include <ironbee/predicate/merge_graph.hpp>
#include <ironbee/predicate/tree_copy.hpp>
#include <ironbee/predicate/validate.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

/**
 * Reference to something else, see Template.
 *
 * Ref nodes exist only to reference something else.  They do not transform
 * and throw exceptions if calculated.  They are replaced by
 * Template::transform() when they appear in a template body.
 **/
class Ref :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::post_transform().
    void post_transform(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(GraphEvalState&, EvalContext) const;
};

/**
 * Call that transforms based on an expression and ref substitution.
 *
 * A template call is initialized with a body expression tree and an argument
 * list.  At transformation, the body is traversed and any ref nodes are
 * replaced by replacing a ref node whose name is at position @c i in the
 * argument list with the @c ith child.
 **/
class Template :
    public Call
{
public:
    /**
     * Constructor.
     *
     * @param[in] name Name of template.
     * @param[in] args List of arguments.  All ref nodes in @a body must
     *                 use one of these.
     * @param[in] body Body.  Any ref nodes in body will be replaced by
     *                 children of this node according to @a args.
     * @param[in] origin_prefix Prefix to add to all origins for body nodes.
     **/
    Template(
        const std::string&          name,
        const template_arg_list_t&  args,
        const node_cp&              body,
        const string&               origin_prefix
    );

    //! See Call::name()
    virtual std::string name() const;

   /**
    * See Node::transform().
    *
    * Will replace self with tree copy of body with ref nodes replace
    * according to children and @a args.
    **/
   virtual bool transform(
       MergeGraph&        merge_graph,
       const CallFactory& call_factory,
       Environment        environment,
       NodeReporter       reporter
   );

   //! See Node::validate().
   virtual bool validate(NodeReporter reporter) const;

   //! See Node::post_transform().
   void post_transform(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(GraphEvalState&, EvalContext) const;

private:
    //! Name.
    const std::string m_name;
    //! Arguments.
    const template_arg_list_t m_args;
    //! Body expression.
    const node_cp m_body;
    //! Prefix for all body origin information.
    const string m_origin_prefix;
};

string Ref::name() const
{
    return "ref";
}

void Ref::eval_calculate(GraphEvalState&, EvalContext) const
{
    BOOST_THROW_EXCEPTION(
        einval() << errinfo_what(
            "Ref node calculated.  "
            "Maybe transform skipped; or used outside of template body."
        )
    );
}

void Ref::post_transform(NodeReporter reporter) const
{
    reporter.error(
        "Ref node should not exist post-transform.  "
        "Maybe used outside of template body."
    );
}

bool Ref::validate(NodeReporter reporter) const
{
    bool result =
        Validate::n_children(reporter, 1) &&
        Validate::nth_child_is_string(reporter, 0)
        ;
    if (result) {
        string ref_param =
            literal_value(children().front())
            .as_string()
            .to_s()
            ;
        if (
            ref_param.empty() ||
            ref_param.find_first_not_of(
                "_0123456789"
                "abcdefghijklmnoprstuvwxyz"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            ) != string::npos
        ) {
            reporter.error(
                "Reference parameter \"" + ref_param +
                "\" is not legal."
            );
            result = false;
        }
    }
    return result;
}

Template::Template(
    const string&              name,
    const template_arg_list_t& args,
    const node_cp&             body,
    const string&              origin_prefix
) :
    m_name(name),
    m_args(args),
    m_body(body),
    m_origin_prefix(origin_prefix)
{
    // nop
}

string Template::name() const
{
    return m_name;
}

void Template::eval_calculate(GraphEvalState&, EvalContext) const
{
    BOOST_THROW_EXCEPTION(
        einval() << errinfo_what(
            "Template node calculated.  "
            "Must have skipped transform."
        )
    );
}

void Template::post_transform(NodeReporter reporter) const
{
    reporter.error(
        "Template node should not exist post-transform."
    );
}

bool Template::validate(NodeReporter reporter) const
{
    return
        Validate::n_children(reporter, m_args.size())
        ;
}

namespace {

string template_ref(const node_cp& node)
{
    const Ref* as_ref = dynamic_cast<const Ref*>(node.get());
    if (! as_ref) {
        return string();
    }

    return literal_value(as_ref->children().front())
        .as_string()
        .to_s()
        ;
}

}

bool Template::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        environment,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();

    // Construct map of argument name to children.
    typedef map<string, node_p> arg_map_t;
    arg_map_t arg_map;

    {
        template_arg_list_t::const_iterator arg_i = m_args.begin();
        node_list_t::const_iterator children_i = children().begin();

        while (arg_i != m_args.end() && children_i != children().end()) {
            arg_map.insert(make_pair(*arg_i, tree_copy(*children_i, call_factory)));
            ++arg_i;
            ++children_i;
        }

        if (arg_i != m_args.end() || children_i != children().end()) {
            reporter.error(
                "Number of children not equal to number of arguments.  "
                "Should have been caught in validation."
            );
            return false;
        }
    }

    // Construct copy of body to replace me with.
    node_p replacement = tree_copy(m_body, call_factory);

    // Special case.  Body might be itself a ref node.
    {
        string top_ref = template_ref(m_body);
        if (! top_ref.empty()) {
            arg_map_t::const_iterator arg_i = arg_map.find(top_ref);
            if (arg_i == arg_map.end()) {
                reporter.error(
                    "Reference to \"" + top_ref + "\" but not such "
                    "argument to template " + name() = "."
                );
                return false;
            }

            node_p replacement = arg_i->second;
            merge_graph.replace(me, replacement);
            merge_graph.add_origin(
                replacement,
                m_origin_prefix + m_body->to_s()
            );
            return true;
        }
    }

    // Replace with body.
    merge_graph.replace(me, replacement);

    // Make list of all descendants.  We don't want to iterate over the
    // replacements, so we make the entire list in advance.
    list<node_p> to_transform;
    bfs_down(replacement, back_inserter(to_transform));
    BOOST_FOREACH(const node_p& node, to_transform) {
        merge_graph.add_origin(node, m_origin_prefix + node->to_s());
    }
    BOOST_FOREACH(const node_p& node, to_transform) {
        node_list_t children = node->children();
        BOOST_FOREACH(const node_p& child, children) {
            if (! merge_graph.known(child)) {
                continue;
            }
            string ref_param = template_ref(child);
            if (! ref_param.empty()) {
                arg_map_t::const_iterator arg_i = arg_map.find(ref_param);
                if (arg_i == arg_map.end()) {
                    reporter.error(
                        "Reference to \"" + ref_param + "\" but not such "
                        "argument to template " + name() = "."
                    );
                    continue;
                }

                node_p arg = arg_i->second;
                merge_graph.replace(child, arg);
            }
        }
    }

    return true;
}

call_p define_template_creator(
    const std::string&        name,
    const template_arg_list_t args,
    const node_cp             body,
    const std::string         origin_prefix
)
{
    return call_p(new Template(name, args, body, origin_prefix));
}

} // Anonymous

CallFactory::generator_t define_template(
    const template_arg_list_t& args,
    const node_cp&             body,
    const string&              origin_prefix
)
{
    return bind(define_template_creator, _1, args, body, origin_prefix);
}

void load_template(CallFactory& to)
{
    to
        .add<Ref>()
        ;
}

} // Standard
} // Predicate
} // IronBee
