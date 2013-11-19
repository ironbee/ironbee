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
 * @brief Predicate --- Standard ValueList Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_valuelist.hpp>

#include <predicate/call_factory.hpp>
#include <predicate/call_helpers.hpp>
#include <predicate/validate.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

static const node_p c_false(new Null());

}

string SetName::name() const
{
    return "setName";
}

Value SetName::value_calculate(
    Value           v,
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    Value name = literal_value(children().front());
    ConstByteString name_bs = name.value_as_byte_string();

    return v.dup(v.memory_pool(), name_bs.const_data(), name_bs.length());
}

void SetName::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    map_calculate(children().back(), graph_eval_state, context);
}

bool SetName::validate(NodeReporter reporter) const
{
    return
        Validate::n_children(reporter, 2) &&
        Validate::nth_child_is_string(reporter, 0) &&
        Validate::nth_child_is_not_null(reporter, 1)
        ;
}

namespace {

/**
 * Implementation details of Cat.
 *
 * To implement Cat, we track two iterators (per thread):
 * - last_unfinished is the child we last processed.  That is, the last time
 *   calculate was run, we added all children of last_unfinished but it was
 *   unfinished so we did not advance to the next child.
 * - last_value_added is the last value of last_unfinished.  That is, the
 *   last time calculate was run, we added all children of last_unfinished,
 *   the last of which was last_value_added.
 *
 * Thus, our task on calculate is to add any remaining children of
 * last_unfinished and check if it is now finished.  If it is, we go on to
 * add any subsequent finished children.  If that consumes all children, we
 * are done and can finish.  Otherwise, we have arrived at a new leftmost
 * unfinished child.  We must add all of its current children, and then
 * wait for the next calculate.
 *
 * This task is handled by add_from_current() and add_until_next_unfinished().
 *
 * This class is a friend of Cat and all routines take a `me` argument being
 * the Cat instance they are working on behalf of.
 **/
class cat_impl_t
{
public:
    /**
     * Constructor.
     *
     * Set @ref last_unfinished to be the first child and
     * @ref last_value_added to be singular.
     **/
    explicit
    cat_impl_t(const Cat& me)
    {
        m_last_unfinished = me.children().begin();
    }

    /**
     * Calculate.
     *
     * After this, @ref last_unfinished and @ref last_value_added will be
     * updated.
     **/
    void eval_calculate(
        const Cat&      me,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    )
    {
        // Add any new children from last_unfinished.
        add_from_current(me, graph_eval_state, context);
        // If last_unfinished is still unfinished, nothing more to do.
        if (! graph_eval_state.is_finished((*m_last_unfinished)->index())) {
            return;
        }

        // Need to find new leftmost unfinished child.  Do so, adding any
        // values from finished children along the way.
        add_until_next_unfinished(me, graph_eval_state, context);

        // If no new leftmost unfinished child, all done.  Finish.
        if (m_last_unfinished == me.children().end()) {
            graph_eval_state[me.index()].finish();
        }
        // Otherwise, need to add children from the new last_unfinished.
        else {
            m_last_value_added = ValueList::const_iterator();
            add_from_current(me, graph_eval_state, context);
        }
    }

private:
    /**
     * Add all children from @ref last_unfinished after @ref last_value_added.
     *
     * Updates @ref last_value_added.
     **/
    void add_from_current(
        const Cat&      me,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    )
    {
        const ValueList& values = graph_eval_state.eval(
            *m_last_unfinished,
            context
        );

        if ((! values || values.empty()) &&
            ! graph_eval_state.is_finished((*m_last_unfinished)->index())
        ) {
            return;
        }
        else if (values && ! values.empty()) {
            if (m_last_value_added == ValueList::const_iterator()) {
                graph_eval_state[me.index()].add_value(values.front());
                m_last_value_added = values.begin();
            }
            ValueList::const_iterator n = m_last_value_added;
            ValueList::const_iterator end = values.end();
            for (;;) {
                ++n;
                if (n == end) {
                    break;
                }
                graph_eval_state[me.index()].add_value(*n);
                m_last_value_added = n;
            }
        }
    }

    /**
     * Advanced @ref last_unfinished to new leftmost unfinished child.
     *
     * Adds values of finished children along the way.
     * If no unfinished children, @ref last_unfinished will end up as
     * `me.children().end()`.
     **/
    void add_until_next_unfinished(
        const Cat&      me,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    )
    {
        assert(graph_eval_state.is_finished((*m_last_unfinished)->index()));
        NodeEvalState& my_state = graph_eval_state[me.index()];
        for (
            ++m_last_unfinished;
            m_last_unfinished != me.children().end();
            ++m_last_unfinished
        ) {
            const ValueList& values =
                graph_eval_state.eval(*m_last_unfinished, context);
            if (
                ! graph_eval_state.is_finished((*m_last_unfinished)->index())
            ) {
                break;
            }
            if (values) {
                BOOST_FOREACH(Value v, values) {
                    my_state.add_value(v);
                }
            }
        }
    }

    //! Last unfinished child processed.
    node_list_t::const_iterator m_last_unfinished;

    /**
     * Last value added from @ref last_unfinished.
     *
     * A singular value means no children of @ref m_last_unfinished have
     * been added.
     **/
    ValueList::const_iterator m_last_value_added;
};

} // cat_impl_t

string Cat::name() const
{
    return "cat";
}

bool Cat::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    bool result = false;

    // Remove null children.
    {
        node_list_t to_remove;
        BOOST_FOREACH(const node_p& child, children()) {
            if (child->is_literal() && ! literal_value(child)) {
                to_remove.push_back(child);
            }
        }
        BOOST_FOREACH(const node_p& child, to_remove) {
            merge_graph.remove(me, child);
        }

        if (! to_remove.empty()) {
            result = true;
        }
    }

    // Become child if only one child.
    if (children().size() == 1) {
        node_p replacement = children().front();
        merge_graph.replace(me, replacement);
        return true;
    }

    // Become false if no children.
    if (children().size() == 0) {
        node_p replacement = c_false;
        merge_graph.replace(me, replacement);
        return true;
    }

    return result;
}

void Cat::eval_initialize(
    NodeEvalState& node_eval_state,
    EvalContext    context
) const
{
    node_eval_state.setup_local_values(context);
    node_eval_state.state() =
        boost::shared_ptr<cat_impl_t>(new cat_impl_t(*this));
}

void Cat::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    boost::any_cast<boost::shared_ptr<cat_impl_t> >(
        graph_eval_state[index()].state()
    )->eval_calculate(*this, graph_eval_state, context);
}

string First::name() const
{
    return "first";
}

void First::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];
    const node_p& child = children().front();
    ValueList values = graph_eval_state.eval(child, context);
    if (! values.empty()) {
        my_state.setup_local_values(context);
        my_state.add_value(values.front());
        my_state.finish();
    }
    else if (graph_eval_state.is_finished(child->index())) {
        my_state.finish_false(context);
    }
}

bool First::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 1);
}

string Rest::name() const
{
    return "rest";
}

void Rest::eval_initialize(
    NodeEvalState& node_eval_state,
    EvalContext    context
) const
{
    node_eval_state.state() = ValueList::const_iterator();
    node_eval_state.setup_local_values(context);
}

void Rest::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];

    const node_p& child = children().front();
    ValueList values = graph_eval_state.eval(child, context);
    ValueList::const_iterator location =
        boost::any_cast<ValueList::const_iterator>(my_state.state());

    // Special case if no values yet.
    if (values.empty()) {
        if (graph_eval_state.is_finished(child->index())) {
            my_state.finish();
        }
        return;
    }

    if (location == ValueList::const_iterator()) {
        location = values.begin();
    }

    // At this point, location refers to element before next one
    // to push.
    ValueList::const_iterator next_location = location;
    ++next_location;
    const ValueList::const_iterator end = values.end();
    while (next_location != end) {
        my_state.add_value(*next_location);
        location = next_location;
        ++next_location;
    }

    if (graph_eval_state.is_finished(child->index())) {
        my_state.finish();
    }
    else {
        my_state.state() = location;
    }
}

bool Rest::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 1);
}

string Nth::name() const
{
    return "nth";
}

void Nth::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];

    int64_t n = literal_value(children().front()).value_as_number();

    if (n <= 0) {
        my_state.finish_false(context);
        return;
    }

    const node_p& child = children().back();
    ValueList values = graph_eval_state.eval(child, context);

    if (values.size() < size_t(n)) {
        if (graph_eval_state.is_finished(child->index())) {
            my_state.finish_false(context);
        }
        return;
    }

    ValueList::const_iterator i = values.begin();
    advance(i, n - 1);

    my_state.setup_local_values(context);
    my_state.add_value(*i);
    my_state.finish();
}

bool Nth::validate(NodeReporter reporter) const
{
    return
        Validate::n_children(reporter, 2) &&
        Validate::nth_child_is_integer_above(reporter, 0, -1)
        ;
}

string Scatter::name() const
{
    return "scatter";
}

void Scatter::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];

    const node_p& child = children().front();

    graph_eval_state.eval(child, context);

    if (! graph_eval_state.is_finished(child->index())) {
        return;
    }

    Value value = simple_value(graph_eval_state.final(child->index()));
    if (value) {
        my_state.setup_local_values(context);
        BOOST_FOREACH(Value v, value.value_as_list<Value>()) {
            my_state.add_value(v);
        }
        my_state.finish();
    }
}

bool Scatter::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 1);
}

string Gather::name() const
{
    return "gather";
}

void Gather::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];

    const node_p& child = children().front();

    graph_eval_state.eval(child, context);

    if (! graph_eval_state.is_finished(child->index())) {
        return;
    }

    List<Value> values =
        List<Value>::create(context.memory_pool());
    ValueList child_values = graph_eval_state.values(child->index());
    copy(
        child_values.begin(), child_values.end(),
        back_inserter(values)
    );

    my_state.setup_local_values(context);
    my_state.add_value(Field::create_no_copy_list(
        context.memory_pool(),
        "", 0,
        values
    ));

    my_state.finish();
}

bool Gather::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 1);
}

void load_valuelist(CallFactory& to)
{
    to
        .add<SetName>()
        .add<Cat>()
        .add<First>()
        .add<Rest>()
        .add<Nth>()
        .add<Scatter>()
        .add<Gather>()
        ;
}

} // Standard
} // Predicate
} // IronBee
