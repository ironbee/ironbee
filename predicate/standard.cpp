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
 * @brief Predicate --- Standard implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "standard.hpp"
#include "merge_graph.hpp"

#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace  {

static const node_p c_true(new String(""));
static const node_p c_false(new Null());

}

string False::name() const
{
    return "false";
}

bool False::transform(
    NodeReporter       reporter,
    MergeGraph&        merge_graph,
    const CallFactory& call_factory
)
{
    node_p me = shared_from_this();
    node_p replacement = c_false;
    merge_graph.replace(me, replacement);

    return true;
}

Value False::calculate(Context)
{
    return Value();
}

string True::name() const
{
    return "true";
}

bool True::transform(
    NodeReporter       reporter,
    MergeGraph&        merge_graph,
    const CallFactory& call_factory
)
{
    node_p me = shared_from_this();
    node_p replacement = c_true;
    merge_graph.replace(me, replacement);

    return true;
}

Value True::calculate(Context)
{
    static node_p s_true_literal;
    if (! s_true_literal) {
        s_true_literal = node_p(new String(""));
        s_true_literal->eval(Context());
    }

    return s_true_literal->value();
}

AbelianCall::AbelianCall() :
    m_ordered(false)
{
    // nop
}

void AbelianCall::add_child(const node_p& child)
{
    if (
        m_ordered &&
        ! less_sexpr()(children().back()->to_s(), child->to_s())
    ) {
        m_ordered = false;
    }
    parent_t::add_child(child);
}

void AbelianCall::replace_child(const node_p& child, const node_p& with)
{
    parent_t::replace_child(child, with);
}

bool AbelianCall::transform(
    NodeReporter       reporter,
    MergeGraph&        merge_graph,
    const CallFactory& call_factory
)
{
    bool parent_result =
        parent_t::transform(reporter, merge_graph, call_factory);

    if (m_ordered) {
        return parent_result;
    }

    vector<node_p> new_children(children().begin(), children().end());
    sort(new_children.begin(), new_children.end(), less_node_by_sexpr());
    assert(new_children.size() == children().size());
    if (
        equal(
            new_children.begin(), new_children.end(),
            children().begin()
        )
    ) {
        m_ordered = true;
        return parent_result;
    }

    node_p replacement = call_factory(name());
    boost::shared_ptr<AbelianCall> replacement_as_ac =
        boost::dynamic_pointer_cast<AbelianCall>(replacement);
    if (! replacement_as_ac) {
        // Insanity error so throw exception.
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "CallFactory produced a node of unexpected lineage."
            )
        );
    }
    BOOST_FOREACH(const node_p& child, new_children) {
        replacement->add_child(child);
    }
    replacement_as_ac->m_ordered = true;

    node_p me = shared_from_this();
    merge_graph.replace(me, replacement);

    return true;
}

string Or::name() const
{
    return "or";
}

Value Or::calculate(Context context)
{
    if (children().size() < 2) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "or requires two or more arguments."
            )
        );
    }
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->eval(context)) {
            // We are true.
            return True().eval(context);
        }
    }
    return False().eval(context);
}

bool Or::transform(
    NodeReporter       reporter,
    MergeGraph&        merge_graph,
    const CallFactory& call_factory
)
{
    node_p me = shared_from_this();
    bool result = false;

    node_list_t to_remove;
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            if (child->eval(Context())) {
                node_p replacement = c_true;
                merge_graph.replace(me, replacement);
                return true;
            }
            else {
                to_remove.push_back(child);
            }
        }
    }

    BOOST_FOREACH(const node_p& child, to_remove) {
        result = true;
        merge_graph.remove(me, child);
    }

    if (children().size() == 1) {
        node_p replacement = children().front();
        merge_graph.replace(me, replacement);
        return true;
    }

    if (children().size() == 0) {
        node_p replacement = c_false;
        merge_graph.replace(me, replacement);
        return true;
    }

    return
        AbelianCall::transform(reporter, merge_graph, call_factory) ||
        result;

}

string And::name() const
{
    return "and";
}

Value And::calculate(Context context)
{
    if (children().size() < 2) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "or requires two or more arguments."
            )
        );
    }
    BOOST_FOREACH(const node_p& child, children()) {
        if (! child->eval(context)) {
            // We are false.
            return False().eval(context);
        }
    }
    return True().eval(context);
}

bool And::transform(
    NodeReporter       reporter,
    MergeGraph&        merge_graph,
    const CallFactory& call_factory
)
{
    node_p me = shared_from_this();
    bool result = false;

    node_list_t to_remove;
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            if (! child->eval(Context())) {
                node_p replacement = c_false;
                merge_graph.replace(me, replacement);
                return true;
            }
            else {
                to_remove.push_back(child);
            }
        }
    }

    BOOST_FOREACH(const node_p& child, to_remove) {
        result = true;
        merge_graph.remove(me, child);
    }

    if (children().size() == 1) {
        node_p replacement = children().front();
        merge_graph.replace(me, replacement);
        return true;
    }

    if (children().size() == 0) {
        node_p replacement = c_true;
        merge_graph.replace(me, replacement);
        return true;
    }

    return
        AbelianCall::transform(reporter, merge_graph, call_factory) ||
        result;
}

string Not::name() const
{
    return "not";
}

Value Not::calculate(Context context)
{
    if (children().size() != 1) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "not requires exactly one argument."
            )
        );
    }
    if (children().front()->eval(context)) {
        return False().eval(context);
    }
    else {
        return True().eval(context);
    }
}

bool Not::transform(
    NodeReporter       reporter,
    MergeGraph&        merge_graph,
    const CallFactory& call_factory
)
{
    assert(children().size() == 1);
    const node_p& child = children().front();
    const node_cp& me = shared_from_this();

    if (child->is_literal()) {
        node_p replacement;
        if (child->eval(Context())) {
            replacement.reset(new Null());
        }
        else {
            replacement.reset(new String(""));
        }
        merge_graph.replace(me, replacement);
        return true;
    }
    else {
        return false;
    }
}

void load(CallFactory& to)
{
    to
        .add<False>()
        .add<True>()
        .add<Or>()
        .add<And>()
        .add<Not>()
    ;
}

} // Standard
} // Predicate
} // IronBee
