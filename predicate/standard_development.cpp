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
 * @brief Predicate --- Standard Development implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_development.hpp>

#include <predicate/call_helpers.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/validate.hpp>
#include <predicate/value.hpp>

#include <ironbee/log.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>

using namespace std;
using boost::bind;
using boost::ref;

namespace IronBee {
namespace Predicate {
namespace Standard {

string P::name() const
{
    return "p";
}

void P::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    list<string> value_strings;
    BOOST_FOREACH(const node_p& n, children()) {
        value_strings.push_back(
            valuelist_to_string(
                graph_eval_state.eval(n, context)
            )
        );
    }

    cerr << boost::algorithm::join(value_strings, "; ") << endl;
    map_calculate(children().back(), graph_eval_state, context);
}

Value P::value_calculate(Value v, GraphEvalState&, EvalContext) const
{
    return v;
}

bool P::validate(NodeReporter reporter) const
{
    return Validate::n_or_more_children(reporter, 1);
}

string Identity::name() const
{
    return "identity";
}

void Identity::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    map_calculate(children().front(), graph_eval_state, context);
}

Value Identity::value_calculate(Value v, GraphEvalState&, EvalContext) const
{
    return v;
}

bool Identity::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 1);
}

string Sequence::name() const
{
    return "sequence";
}

void Sequence::eval_initialize(
    NodeEvalState& node_eval_state,
    EvalContext    context
) const
{
    node_eval_state.state() =
        literal_value(children().front()).value_as_number();
    node_eval_state.setup_local_values(context);
}

bool Sequence::validate(NodeReporter reporter) const
{
    bool result = true;

    result = Validate::n_or_more_children(reporter, 1) && result;
    result = Validate::n_or_fewer_children(reporter, 3) && result;
    result = Validate::nth_child_is_integer(reporter, 0) && result;
    if (children().size() > 1) {
        result = Validate::nth_child_is_integer(reporter, 1) && result;
    }
    if (children().size() > 2) {
        result = Validate::nth_child_is_integer(reporter, 2) && result;
    }
    return result;
}

void Sequence::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];

    // Figure out parameters.
    int64_t start;
    int64_t end = -1;
    int64_t step = 1;

    node_list_t::const_iterator i = children().begin();
    start = literal_value(*i).value_as_number();
    ++i;
    if (i != children().end()) {
        end = literal_value(*i).value_as_number();
        ++i;
        if (i != children().end()) {
            step = literal_value(*i).value_as_number();
        }
    }
    else {
        end = start - 1;
    }

    // Output current.
    ib_num_t current = boost::any_cast<ib_num_t>(my_state.state());
    my_state.add_value(
        Field::create_number(context.memory_manager(), "", 0, current)
    );

    // Advance current.
    current += step;
    my_state.state() = current;

    // Figure out if infinite.
    if (
        (step > 0 && start > end ) ||
        (step < 0 && end > start )
    ) {
        return;
    }

    // Figure out if done.  Note >/< and not >=/<=.
    // Also note never finished if step == 0.
    if (
        (step > 0 && current > end)  ||
        (step < 0 && current < end)
    ) {
        my_state.finish();
    }
}

void load_development(CallFactory& to)
{
    to
        .add<P>()
        .add<Identity>()
        .add<Sequence>()
        ;
}

} // Standard
} // Predicate
} // IronBee
