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

#include <predicate/meta_call.hpp>

#include <predicate/call_factory.hpp>
#include <predicate/less.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/reporter.hpp>

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
    NodeReporter       reporter
)
{
    bool parent_result =
        Predicate::Call::transform(merge_graph, call_factory, reporter);

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

void MapCall::reset()
{
    Call::reset();
    if (m_input_locations.get()) {
        m_input_locations->clear();
    }
    else {
        m_input_locations.reset(new input_locations_t());
    }
}

void MapCall::map_calculate(
    const node_p& input,
    EvalContext   eval_context,
    bool          eval_input,
    bool          auto_finish
)
{
    if (eval_input) {
        input->eval(eval_context);
    }

    ValueList inputs = input->values();
    input_locations_t& input_locations = *m_input_locations.get();

    // Check empty check is necessary as an empty list is allowed to change
    // to a different list to support values forwarding.
    if (! inputs.empty()) {
        input_locations_t::iterator i = input_locations.lower_bound(input);
        if (i == input_locations.end() || i->first != input) {
            i = input_locations.insert(
                i,
                make_pair(input, inputs.begin())
            );
        }
        ValueList::const_iterator current = i->second;
        ValueList::const_iterator end     = inputs.end();
        for (;current != end; ++current) {
            Value v = *current;
            Value result = value_calculate(v, eval_context);

            if (result) {
                add_value(result);
            }
        }
    }

    if (auto_finish && input->is_finished()) {
        finish();
    }
}

} // Predicate
} // IronBee
