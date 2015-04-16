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
 * @brief Predicate --- Eval Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

// Define EVAL_TRACE to output information to cout at beginning and end
// of every eval() call.
//#define EVAL_TRACE

#include <ironbee/predicate/eval.hpp>

#include <ironbee/rule_engine.h>

using namespace std;

namespace IronBee {
namespace Predicate {

namespace {
static IronBee::ScopedMemoryPoolLite c_static_pool;
static const Value
    c_empty_string(
        IronBee::Field::create_byte_string(
            c_static_pool,
            "", 0,
            IronBee::ByteString::create(c_static_pool)
        )
    );

}

// NodeEvalState

NodeEvalState::NodeEvalState() :
    m_finished(false),
    m_phase(IB_PHASE_NONE)
{
    // nop
}

void NodeEvalState::forward(const node_p& to)
{
    if (is_forwarding()) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't forward a forwarded node."
            )
        );
    }
    if (is_aliased()) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't forward an aliased node."
            )
        );
    }
    if (is_finished()) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't finish an already finished node."
            )
        );
    }
    if (m_local_values) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't combine existing values with local values."
            )
        );
    }
    m_forward = to;
}

void NodeEvalState::set_phase(ib_rule_phase_num_t phase)
{
    m_phase = phase;
}

void NodeEvalState::setup_local_list(MemoryManager mm)
{
    return setup_local_list(mm, "", 0);
}

void NodeEvalState::setup_local_list(
    MemoryManager mm,
    const char*   name,
    size_t        name_length
)
{
    if (is_aliased()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Cannot setup local values on aliased node."
            )
        );
    }
    if (is_forwarding()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Cannot setup local values on forwarded node."
            )
        );
    }

    if (m_value) {
        // do nothing
        return;
    }

    m_local_values = List<Value>::create(mm);
    m_value = Value::alias_list(mm, name, name_length, m_local_values);
}

void NodeEvalState::append_to_list(Value value)
{
    if (is_forwarding()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Can't add value to forwarded node."
            )
        );
    }
    if (is_finished()) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't add value to finished node."
            )
        );
    }
    if (! m_local_values) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Attempting to add value before setting up local list."
            )
        );
    }
    m_local_values.push_back(value);
}

void NodeEvalState::finish()
{
    if (is_forwarding()) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't finish a forwarded node."
            )
        );
    }
    if (is_finished()) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't finish an already finished node."
            )
        );
    }
    m_finished = true;
}

void NodeEvalState::finish(Value v)
{
    if (m_value) {
        BOOST_THROW_EXCEPTION(
            IronBee::einval() << errinfo_what(
                "Can't finish a valued node with a value."
            )
        );
    }
    // Call finish first to do normal finish checks.
    finish();
    m_value = v;
}

void NodeEvalState::alias(Value other)
{
    if (is_forwarding()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Can't alias a forwarded node."
            )
        );
    }
    if (is_finished()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Can't alias a finished node."
            )
        );
    }
    if (m_local_values) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Can't alias a local valued node."
            )
        );
    }
    if (m_value) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Can't alias an aliased node."
            )
        );
    }
    m_value = other;
}

void NodeEvalState::finish_true(EvalContext eval_context)
{
    finish(c_empty_string);
}

// GraphEvalProfileData
void GraphEvalProfileData::mark_start() {
    m_eval_start = ib_clock_precise_get_time();
}

void GraphEvalProfileData::mark_finish() {
    m_eval_finish = ib_clock_precise_get_time();
}

uint32_t GraphEvalProfileData::duration() const {
    return (m_eval_finish - m_eval_start);
}

// GraphEvalState

GraphEvalState::GraphEvalState(size_t index_limit) :
    m_vector(index_limit),
    m_profile(false)
{
    // nop
}

const NodeEvalState& GraphEvalState::final(size_t index) const
{
    while (m_vector[index].is_forwarding()) {
        index = m_vector[index].forwarded_to()->index();
    }

    return m_vector[index];
}

Value GraphEvalState::value(size_t index) const
{
    return final(index).value();
}

ib_rule_phase_num_t GraphEvalState::phase(size_t index) const
{
    return final(index).phase();
}

bool GraphEvalState::is_finished(size_t index) const
{
    return final(index).is_finished();
}

void GraphEvalState::initialize(const node_cp& node, EvalContext context)
{
    assert(! m_vector[node->index()].is_forwarding());

    if (m_profile) {
        GraphEvalProfileData& gpd = profiler_mark(node);
        node->eval_initialize(*this, context);
        profiler_record(gpd);
    }
    else {
        node->eval_initialize(*this, context);
    }
}

void GraphEvalState::eval(const node_cp& node, EvalContext context)
{
#ifdef EVAL_TRACE
    cout << "EVAL " << node->to_s() << endl;
#endif

    // In certain cases, e.g., literals, we run without a context or
    // rule_exec.  Then, always calculate.
    ib_rule_phase_num_t phase = IB_PHASE_NONE;
    if (context.ib() && context.ib()->rule_exec) {
        phase = context.ib()->rule_exec->phase;
    }

    // Handle forwarding.
    node_cp final_node = node;
    while (m_vector[final_node->index()].is_forwarding()) {
        final_node = m_vector[final_node->index()].forwarded_to();
    }

    NodeEvalState& node_eval_state = m_vector[final_node->index()];
    assert(! node_eval_state.is_forwarding());

    if (
        ! node_eval_state.is_finished() &&
        (node_eval_state.phase() != phase || phase == IB_PHASE_NONE)
    ) {
        if (m_profile) {
            GraphEvalProfileData& gpd = profiler_mark(final_node);
            node_eval_state.set_phase(phase);
            final_node->eval_calculate(*this, context);
            profiler_record(gpd);
        }
        else {
            node_eval_state.set_phase(phase);
            final_node->eval_calculate(*this, context);
        }
    }

#ifdef EVAL_TRACE
    cout << "VALUE " << node->to_s() << " = " << value(node->index()) << endl;
#endif
}

GraphEvalProfileData::GraphEvalProfileData(const std::string& name)
:
    m_node_name(name),
    m_eval_start(0),
    m_eval_finish(0)
{
}

GraphEvalProfileData& GraphEvalState::profiler_mark(node_cp node)
{
    GraphEvalProfileData data(node->to_s());

    data.mark_start();

    m_profile_data.push_back(data);

    return m_profile_data.back();
}

void GraphEvalState::profiler_record(GraphEvalProfileData& data)
{
    data.mark_finish();
}

GraphEvalState::profiler_data_list_t& GraphEvalState::profiler_data()
{
    return m_profile_data;
}

const GraphEvalState::profiler_data_list_t&
GraphEvalState::profiler_data() const
{
    return m_profile_data;
}

void GraphEvalState::profiler_clear()
{
    m_profile_data.clear();
}

void GraphEvalState::profiler_enabled(bool enabled)
{
    m_profile = enabled;
}


// Doxygen confused by this code.
#ifndef DOXYGEN_SKIP
namespace Impl {

make_indexer_helper_t::make_indexer_helper_t(size_t& index_limit) :
    m_index_limit(index_limit)
{
    // nop
}

void make_indexer_helper_t::operator()(const node_p& node)
{
    node->set_index(m_index_limit);
    ++m_index_limit;
}

make_initializer_helper_t::make_initializer_helper_t(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) :
    m_graph_eval_state(graph_eval_state),
    m_context(context)
{
    // nop
}

void make_initializer_helper_t::operator()(const node_cp& node)
{
    m_graph_eval_state.initialize(node, m_context);
}

}
#endif

boost::function_output_iterator<Impl::make_indexer_helper_t>
make_indexer(size_t& index_limit)
{
    index_limit = 0;
    return boost::function_output_iterator<Impl::make_indexer_helper_t>(
        Impl::make_indexer_helper_t(index_limit)
    );
}

boost::function_output_iterator<Impl::make_initializer_helper_t>
make_initializer(GraphEvalState& graph_eval_state, EvalContext context)
{
    return boost::function_output_iterator<Impl::make_initializer_helper_t>(
        Impl::make_initializer_helper_t(graph_eval_state, context)
    );
}

} // Predicate
} // IronBee
