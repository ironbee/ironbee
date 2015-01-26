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
 * @brief Predicate --- Meta Call implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/predicate/meta_call.hpp>

#include <ironbee/predicate/call_factory.hpp>
#include <ironbee/predicate/eval.hpp>
#include <ironbee/predicate/less.hpp>
#include <ironbee/predicate/merge_graph.hpp>
#include <ironbee/predicate/reporter.hpp>

#include <boost/shared_ptr.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {

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
    Predicate::Call::add_child(child);
}

void AbelianCall::replace_child(const node_p& child, const node_p& with)
{
    Predicate::Call::replace_child(child, with);
}

bool AbelianCall::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        environment,
    NodeReporter       reporter
)
{
    bool parent_result = Predicate::Call::transform(
        merge_graph,
        call_factory,
        environment,
        reporter
    );

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

void MapCall::eval_initialize(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    Call::eval_initialize(graph_eval_state, context);
    NodeEvalState& my_state = graph_eval_state[index()];
    my_state.state() =
        boost::shared_ptr<input_locations_t>(new input_locations_t());
    my_state.setup_local_list(context.memory_manager());
}

void MapCall::map_calculate(
    const node_p&   input,
    GraphEvalState& graph_eval_state,
    EvalContext     context,
    bool            eval_input,
    bool            auto_finish
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];
    if (eval_input) {
        graph_eval_state.eval(input, context);
    }

    Value input_value = graph_eval_state.value(input->index());
    if (input_value.is_null()) {
        return;
    }
    if (input_value.type() == Value::LIST) {
        ConstList<Value> inputs = input_value.as_list();

        input_locations_t& input_locations =
            *boost::any_cast<boost::shared_ptr<input_locations_t> >(
                my_state.state()
            );

        // Check empty check is necessary as an empty list is allowed to change
        // to a different list to support values forwarding.
        if (inputs && ! inputs.empty()) {
            input_locations_t::iterator i = input_locations.lower_bound(input);
            if (i == input_locations.end() || i->first != input) {
                i = input_locations.insert(
                    i,
                    make_pair(input, inputs.begin())
                );
                Value result = value_calculate(inputs.front(), graph_eval_state, context);

                if (result) {
                    my_state.append_to_list(result);
                }
            }

            ConstList<Value>::const_iterator end = inputs.end();
            // current will always be the last element successfully processed.
            ConstList<Value>::const_iterator current = i->second;

            for (
                // consider will be the element after current.
                ConstList<Value>::const_iterator consider = boost::next(current);
                consider != end;
                current = consider, consider = boost::next(current)
            ) {
                Value v = *consider;
                Value result = value_calculate(v, graph_eval_state, context);
                if (result) {
                    my_state.append_to_list(result);
                }
            }
            i->second = current;
        }
        if (auto_finish && graph_eval_state.is_finished(input->index())) {
            my_state.finish();
        }
    }
    else {
        assert(graph_eval_state.is_finished(input->index()));
        Value my_value =
            value_calculate(input_value, graph_eval_state, context);
        my_state.finish(my_value);
    }
}

AliasCall::AliasCall(const std::string& into) :
    m_into(into)
{
    // nop
}

bool AliasCall::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        environment,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    node_p replacement = call_factory(m_into);

    BOOST_FOREACH(const node_p& child, children()) {
        replacement->add_child(child);
    }

    merge_graph.replace(me, replacement);

    return true;
}

void AliasCall::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    BOOST_THROW_EXCEPTION(
        IronBee::einval() << errinfo_what(
            "Cannot evaluate AliasCall.  Did you forget transform?"
        )
    );
}

} // Predicate
} // IronBee
