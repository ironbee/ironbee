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
 * @brief Predicate --- Standard Boolean implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_boolean.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/validate.hpp>

#include <ironbeepp/operator.hpp>
#include <ironbeepp/transformation.hpp>

#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

static const node_p c_true(new String(""));
static const node_p c_false(new Null());

}

string False::name() const
{
    return "false";
}

bool False::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    node_p replacement = c_false;
    merge_graph.replace(me, replacement);

    return true;
}

void False::calculate(EvalContext)
{
    BOOST_THROW_EXCEPTION(
        einval() << errinfo_what(
            "False evaluated; did you not transform?"
        )
    );
}

bool False::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 0);
}

string True::name() const
{
    return "true";
}

bool True::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    node_p replacement = c_true;
    merge_graph.replace(me, replacement);

    return true;
}

void True::calculate(EvalContext)
{
    BOOST_THROW_EXCEPTION(
        einval() << errinfo_what(
            "True evaluated; did you not transform?"
        )
    );
}

bool True::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 0);
}

string Or::name() const
{
    return "or";
}

void Or::calculate(EvalContext context)
{
    assert(children().size() >= 2);
    bool unfinished_child = false;
    BOOST_FOREACH(const node_p& child, children()) {
        if (! child->eval(context).empty()) {
            finish_true();
            return;
        }
        if (! child->is_finished()) {
            unfinished_child = true;
        }
    }
    if (! unfinished_child) {
        finish_false();
    }
}

bool Or::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    bool result = false;

    node_list_t to_remove;
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            if (! child->eval(EvalContext()).empty()) {
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
        AbelianCall::transform(merge_graph, call_factory, reporter) ||
        result;

}

bool Or::validate(NodeReporter reporter) const
{
    return Validate::n_or_more_children(reporter, 2);
}

string And::name() const
{
    return "and";
}

void And::calculate(EvalContext context)
{
    assert(children().size() >= 2);
    bool unfinished_child = false;
    BOOST_FOREACH(const node_p& child, children()) {
        child->eval(context);
        if (child->is_finished() && child->values().empty()) {
            finish_false();
            return;
        }
        if (! child->is_finished()) {
            unfinished_child = true;
        }
    }
    if (! unfinished_child) {
        // No unfinished children; no empty children.
        finish_true();
    }
}

bool And::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    bool result = false;

    node_list_t to_remove;
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            if (child->eval(EvalContext()).empty()) {
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
        AbelianCall::transform(merge_graph, call_factory, reporter) ||
        result;
}

bool And::validate(NodeReporter reporter) const
{
    return Validate::n_or_more_children(reporter, 2);
}

string Not::name() const
{
    return "not";
}

void Not::calculate(EvalContext context)
{
    assert(children().size() == 1);
    const node_p& child = children().front();
    child->eval(context);
    if (! child->values().empty()) {
        assert(values().empty());
        finish_false();
    }
    else if (child->is_finished()) {
        finish_true();
    }
}

bool Not::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    assert(children().size() == 1);
    const node_p& child = children().front();
    const node_cp& me = shared_from_this();

    if (child->is_literal()) {
        node_p replacement;
        if (! child->eval(EvalContext()).empty()) {
            replacement = c_false;
        }
        else {
            replacement = c_true;
        }
        merge_graph.replace(me, replacement);
        return true;
    }
    else {
        return false;
    }
}

bool Not::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 1);
}

string If::name() const
{
    return "if";
}

void If::calculate(EvalContext context)
{
    assert(children().size() == 3);
    node_list_t::const_iterator i;
    i = children().begin();
    const node_p& pred = *i;
    ++i;
    const node_p& true_value = *i;
    ++i;
    const node_p& false_value = *i;
    pred->eval(context);
    if (! pred->values().empty()) {
        true_value->eval(context);
        forward(true_value);
    }
    else if (pred->is_finished()) {
        false_value->eval(context);
        forward(false_value);
    }
}

bool If::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    assert(children().size() == 3);
    const node_cp& me = shared_from_this();
    node_list_t::const_iterator i;
    i = children().begin();
    const node_p& pred = *i;
    ++i;
    const node_p& true_value = *i;
    ++i;
    const node_p& false_value = *i;

    if (pred->is_literal()) {
        node_p replacement;
        if (! pred->eval(EvalContext()).empty()) {
            replacement = true_value;
        }
        else {
            replacement = false_value;
        }
        merge_graph.replace(me, replacement);
        return true;
    }
    else {
        return false;
    }
}

bool If::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 3);
}

string OrSC::name() const
{
    return "orSC";
}

void OrSC::calculate(EvalContext context)
{
    assert(children().size() >= 2);
    BOOST_FOREACH(const node_p& child, children()) {
        if (! child->eval(context).empty()) {
            finish_true();
            return;
        }
        if (! child->is_finished()) {
            // Don't evaluate further children until we know this one is
            // false.
            return;
        }
    }
    // Only reach here if all children are finished and false.
    finish_false();
}

bool OrSC::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    bool result = false;

    node_list_t to_remove;
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            if (! child->eval(EvalContext()).empty()) {
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

    return result;
}

bool OrSC::validate(NodeReporter reporter) const
{
    return Validate::n_or_more_children(reporter, 2);
}

string AndSC::name() const
{
    return "andSC";
}

void AndSC::calculate(EvalContext context)
{
    assert(children().size() >= 2);
    BOOST_FOREACH(const node_p& child, children()) {
        child->eval(context);
        if (child->is_finished() && child->values().empty()) {
            finish_false();
            return;
        }
        if (child->values().empty()) {
            // Do not proceed until child is known to be truthy.
            return;
        }
    }
    // Only reached if all children are truthy.
    finish_true();
}

bool AndSC::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    bool result = false;

    node_list_t to_remove;
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            if (child->eval(EvalContext()).empty()) {
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

    return result;
}

bool AndSC::validate(NodeReporter reporter) const
{
    return Validate::n_or_more_children(reporter, 2);
}

void load_boolean(CallFactory& to)
{
    to
        .add<False>()
        .add<True>()
        .add<Or>()
        .add<And>()
        .add<Not>()
        .add<If>()
        .add<OrSC>()
        .add<AndSC>()
        ;
}

} // Standard
} // Predicate
} // IronBee
