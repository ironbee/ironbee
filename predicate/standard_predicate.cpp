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
 * @brief Predicate --- Standard Predicate Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_predicate.hpp>

#include <predicate/call_factory.hpp>
#include <predicate/call_helpers.hpp>
#include <predicate/validate.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

static const node_p c_true(new String(""));
static const node_p c_false(new Null());

bool transform_to_true_if_literal(MergeGraph& merge_graph, const node_p& node)
{
    if (node->children().front()->is_literal()) {
        node_p replacement = c_true;
        merge_graph.replace(node, replacement);
        return true;
    }
    return false;
}

}

string IsLonger::name() const
{
    return "isLonger";
}

bool IsLonger::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    if (children().back()->is_literal()) {
        int64_t n = literal_value(children().front()).value_as_number();
        // @todo Simplify this after adding validation that n >= 1.
        if (n >= 1) {
            node_p me = shared_from_this();
            node_p replacement = c_false;
            merge_graph.replace(me, replacement);
            return true;
        }
    }
    return false;
}

void IsLonger::calculate(EvalContext context)
{
    int64_t n = literal_value(children().front()).value_as_number();

    // @todo Move this to validation and change below to an assert.
    if (n <= 0) {
        finish_true();
    }

    const node_p& child = children().back();
    ValueList values = child->eval(context);

    if (values.size() > size_t(n)) {
        finish_true();
    }
    else if (child->is_finished()) {
        finish_false();
    }
}

bool IsLonger::validate(NodeReporter reporter) const
{
    return
        Validate::n_children(reporter, 2) &&
        Validate::nth_child_is_integer_above(reporter, 0, -1)
        ;
}

string IsLiteral::name() const
{
    return "isLiteral";
}

bool IsLiteral::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    node_p replacement = c_false;
    if (children().front()->is_literal()) {
        replacement = c_true;
    }
    merge_graph.replace(me, replacement);

    return true;
}

void IsLiteral::calculate(EvalContext context)
{
    BOOST_THROW_EXCEPTION(
        einval() << errinfo_what(
            "IsLiteral evaluated.  Did you not transform?"
        )
    );
}

bool IsLiteral::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 1);
}

string IsSimple::name() const
{
    return "isSimple";
}

bool IsSimple::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    return transform_to_true_if_literal(merge_graph, shared_from_this());
}

void IsSimple::calculate(EvalContext context)
{
    const node_p& child = children().back();
    ValueList values = child->eval(context);

    if (values.size() > 1) {
        finish_false();
    }
    else if (child->is_finished()) {
        finish_true();
    }
}

bool IsSimple::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 1);
}

string IsFinished::name() const
{
    return "isFinished";
}

bool IsFinished::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    return transform_to_true_if_literal(merge_graph, shared_from_this());
}

void IsFinished::calculate(EvalContext context)
{
    const node_p& child = children().back();
    child->eval(context);
    if (child->is_finished()) {
        finish_true();
    }
}

bool IsFinished::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 1);
}

struct IsHomogeneous::data_t
{
    boost::thread_specific_ptr<ValueList::const_iterator> location;
};

IsHomogeneous::IsHomogeneous() :
    m_data(new data_t())
{
    // nop
}

string IsHomogeneous::name() const
{
    return "isHomogeneous";
}

bool IsHomogeneous::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    return transform_to_true_if_literal(merge_graph, shared_from_this());
}

void IsHomogeneous::reset()
{
    if (m_data->location.get()) {
        *(m_data->location) = ValueList::const_iterator();
    }
    else {
        m_data->location.reset(new ValueList::const_iterator());
    }
}

void IsHomogeneous::calculate(EvalContext context)
{
    const node_p& child = children().back();
    ValueList values = child->eval(context);
    ValueList::const_iterator& location = *m_data->location.get();

    // Special case if no values yet.
    if (values.empty()) {
        if (child->is_finished()) {
            finish_true();
        }
        return;
    }

    if (location == ValueList::const_iterator()) {
        location = values.begin();
    }

    // Assuming all elements from begin to location are the same type.
    Value::type_e type = location->type();
    ValueList::const_iterator end = values.end();
    ++location;

    while (location != end) {
        if (location->type() != type) {
            finish_false();
            return;
        }
        ++location;
    }

    if (child->is_finished()) {
        finish_true();
    }
}

bool IsHomogeneous::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 1);
}

void load_predicate(CallFactory& to)
{
    to
        .add<IsLonger>()
        .add<IsLiteral>()
        .add<IsSimple>()
        .add<IsFinished>()
        .add<IsHomogeneous>()
        ;
}

} // Standard
} // Predicate
} // IronBee
