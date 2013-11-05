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

Value SetName::value_calculate(Value v, EvalContext context)
{
    Value name = literal_value(children().front());
    ConstByteString name_bs = name.value_as_byte_string();

    return v.dup(v.memory_pool(), name_bs.const_data(), name_bs.length());
}

void SetName::calculate(EvalContext context)
{
    map_calculate(children().back(), context);
}

bool SetName::validate(NodeReporter reporter) const
{
    return
        Validate::n_children(reporter, 2) &&
        Validate::nth_child_is_string(reporter, 0) &&
        Validate::nth_child_is_not_null(reporter, 1)
        ;
}

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
class Cat::impl_t
{
private:
    //! Per thread data.
    struct data_t
    {
        //! Last unfinished child processed.
        node_list_t::const_iterator last_unfinished;
        /**
         * Last value added from @ref last_unfinished.
         *
         * A singular value means no children of @ref last_unfinished have
         * been added.
         **/
        ValueList::const_iterator last_value_added;
    };

    //! Data.
    boost::thread_specific_ptr<data_t> m_data;

public:
    /**
     * Reset.
     *
     * Set @ref last_unfinished to be the first child and
     * @ref last_value_added to be singular.
     **/
    void reset(Cat& me)
    {
        if (m_data.get()) {
            *m_data = data_t();
        }
        else {
            m_data.reset(new data_t());
        }
        m_data->last_unfinished = me.children().begin();
    }

    /**
     * Calculate.
     *
     * After this, @ref last_unfinished and @ref last_value_added will be
     * updated.
     **/
    void calculate(Cat& me, EvalContext context)
    {
        // Cache to avoid repeated thread local lookups.
        data_t& data = *m_data;

        // Add any new children from last_unfinished.
        add_from_current(me, data, context);
        // If last_unfinished is still unfinished, nothing more to do.
        if (! (*data.last_unfinished)->is_finished()) {
            return;
        }

        // Need to find new leftmost unfinished child.  Do so, adding any
        // values from finished children along the way.
        add_until_next_unfinished(me, data, context);

        // If no new leftmost unfinished child, all done.  Finish.
        if (data.last_unfinished == me.children().end()) {
            me.finish();
        }
        // Otherwise, need to add children from the new last_unfinished.
        else {
            data.last_value_added = ValueList::const_iterator();
            add_from_current(me, data, context);
        }
    }

private:
    /**
     * Add all children from @ref last_unfinished after @ref last_value_added.
     *
     * Updates @ref last_value_added.
     **/
    static
    void add_from_current(
        Cat&        me,
        data_t&     data,
        EvalContext context
    )
    {
        const ValueList& values = (*data.last_unfinished)->eval(context);

        if (values.empty() && ! (*data.last_unfinished)->is_finished()) {
            return;
        }
        else if (! values.empty()) {
            if (data.last_value_added == ValueList::const_iterator()) {
                me.add_value(values.front());
                data.last_value_added = values.begin();
            }
            ValueList::const_iterator n = data.last_value_added;
            ValueList::const_iterator end = values.end();
            for (;;) {
                ++n;
                if (n == end) {
                    break;
                }
                me.add_value(*n);
                data.last_value_added = n;
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
    static
    void add_until_next_unfinished(
        Cat&        me,
        data_t&     data,
        EvalContext context
    )
    {
        assert((*data.last_unfinished)->is_finished());
        for (
            ++data.last_unfinished;
            data.last_unfinished != me.children().end();
            ++data.last_unfinished
        ) {
            const ValueList& values = (*data.last_unfinished)->eval(context);
            if (! (*data.last_unfinished)->is_finished()) {
                break;
            }
            BOOST_FOREACH(Value v, values) {
                me.add_value(v);
            }
        }
    }
};

Cat::Cat() :
    m_impl(new impl_t())
{
    // nop
}

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

void Cat::reset()
{
    m_impl->reset(*this);
}

void Cat::calculate(EvalContext context)
{
    m_impl->calculate(*this, context);
}

string First::name() const
{
    return "first";
}

void First::calculate(EvalContext context)
{
    const node_p& child = children().front();
    ValueList values = child->eval(context);
    if (! values.empty()) {
        add_value(values.front());
        finish();
    }
    else if (child->is_finished()) {
        finish();
    }
}

bool First::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 1);
}

struct Rest::data_t
{
    boost::thread_specific_ptr<ValueList::const_iterator> location;
};

Rest::Rest() :
    m_data(new data_t())
{
    // nop
}

string Rest::name() const
{
    return "rest";
}

void Rest::reset()
{
    if (m_data->location.get()) {
        *(m_data->location) = ValueList::const_iterator();
    }
    else {
        m_data->location.reset(new ValueList::const_iterator());
    }
}

void Rest::calculate(EvalContext context)
{
    const node_p& child = children().front();
    ValueList values = child->eval(context);
    ValueList::const_iterator& location = *(m_data->location.get());

    // Special case if no values yet.
    if (values.empty()) {
        if (child->is_finished()) {
            finish();
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
        add_value(*next_location);
        location = next_location;
        ++next_location;
    }

    if (child->is_finished()) {
        finish();
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

void Nth::calculate(EvalContext context)
{
    int64_t n = literal_value(children().front()).value_as_number();

    if (n <= 0) {
        finish();
        return;
    }

    const node_p& child = children().back();
    ValueList values = child->eval(context);

    if (values.size() < size_t(n)) {
        if (child->is_finished()) {
            finish();
        }
        return;
    }

    ValueList::const_iterator i = values.begin();
    advance(i, n - 1);

    add_value(*i);
    finish();
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

void Scatter::calculate(EvalContext context)
{
    const node_p& child = children().front();
    child->eval(context);

    if (! child->is_finished()) {
        return;
    }

    Value value = simple_value(child);
    if (value) {
        BOOST_FOREACH(Value v, value.value_as_list<Value>()) {
            add_value(v);
        }
        finish();
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

void Gather::calculate(EvalContext context)
{
    const node_p& child = children().front();
    child->eval(context);

    if (! child->is_finished()) {
        return;
    }

    List<Value> values =
        List<Value>::create(context.memory_pool());

    copy(
        child->values().begin(), child->values().end(),
        back_inserter(values)
    );

    add_value(Field::create_no_copy_list(
        context.memory_pool(),
        "", 0,
        values
    ));

    finish();
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
