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

#include <predicate/eval.hpp>

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

void NodeEvalState::setup_local_list(EvalContext context)
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

	m_local_values = List<Value>::create(context.memory_manager());
	m_value = Field::create_no_copy_list<Value>(
		context.memory_manager(), "", 0, m_local_values
	);
}

void NodeEvalState::add_to_list(Value value)
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

// GraphEvalState

GraphEvalState::GraphEvalState(size_t index_limit) :
    m_vector(index_limit)
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

void GraphEvalState::initialize(const node_p& node, EvalContext context)
{
    assert(! m_vector[node->index()].is_forwarding());
    node->eval_initialize(m_vector[node->index()], context);
}

Value GraphEvalState::eval(const node_p& node, EvalContext context)
{
    // In certain cases, e.g., literals, we run without a context or
    // rule_exec.  Then, always calculate.
    ib_rule_phase_num_t phase = IB_PHASE_NONE;
    if (context.ib() && context.ib()->rule_exec) {
        phase = context.ib()->rule_exec->phase;
    }

    // Handle forwarding.
    node_p final_node = node;
    while (m_vector[final_node->index()].is_forwarding()) {
        final_node = m_vector[final_node->index()].forwarded_to();
    }

    NodeEvalState& node_eval_state = m_vector[final_node->index()];
    assert(! node_eval_state.is_forwarding());

    if (
        ! node_eval_state.is_finished() &&
        (node_eval_state.phase() != phase || phase == IB_PHASE_NONE)
    ) {
        node_eval_state.set_phase(phase);
        final_node->eval_calculate(*this, context);
    }

    return node_eval_state.value();
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

void make_initializer_helper_t::operator()(const node_p& node)
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
