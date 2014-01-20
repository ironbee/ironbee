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
 * @brief Predicate --- Standard IronBee Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_ironbee.hpp>

#include <predicate/call_factory.hpp>
#include <predicate/call_helpers.hpp>
#include <predicate/validate.hpp>

#include <ironbeepp/operator.hpp>
#include <ironbeepp/transformation.hpp>
#include <ironbeepp/var.hpp>

#include <ironbee/rule_engine.h>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

ib_rule_phase_num_t phase_lookup(const string& phase_string)
{
    ib_rule_phase_num_t result;

    result = ib_rule_lookup_phase(phase_string.c_str(), true);
    if (result == IB_PHASE_INVALID) {
        result = ib_rule_lookup_phase(phase_string.c_str(), false);
    }
    return result;
}

}

struct Var::data_t
{
    data_t() :
        wait_phase(IB_PHASE_NONE),
        final_phase(IB_PHASE_NONE)
    {}

    VarSource source;
    ib_rule_phase_num_t wait_phase;
    ib_rule_phase_num_t final_phase;
};

Var::Var() :
    m_data(new data_t)
{
    // nop
}

string Var::name() const
{
    return "var";
}

bool Var::validate(NodeReporter reporter) const
{
    bool result = true;

    if (children().size() == 1) {
        result = Validate::n_children(reporter, 1) && result;
        result = Validate::nth_child_is_string(reporter, 0) && result;
    }
    else if (children().size() == 3) {
        result = Validate::n_children(reporter, 3) && result;
        result = Validate::nth_child_is_string(reporter, 0) && result;
        result = Validate::nth_child_is_string(reporter, 1) && result;
        result = Validate::nth_child_is_string(reporter, 2) && result;

        if (result) {
            string phase;
            node_list_t::const_iterator i = children().begin();
            for (++i; i != children().end(); ++i) {
                phase = literal_value(*i).value_as_byte_string().to_s();
                if (phase_lookup(phase) == IB_PHASE_INVALID) {
                    reporter.error("Invalid phase: " + phase);
                    result = false;
                }
            }
        }
    }
    else {
        reporter.error(name() + " must have 1 or 3 children.");
        result = false;
    }

    return result;
}

void Var::pre_eval(Environment environment, NodeReporter reporter)
{
    // Key must be static.
    Value key_field = literal_value(children().front());
    IronBee::ConstByteString key = key_field.value_as_byte_string();

    m_data->source = VarSource::acquire(
        environment.main_memory_pool(),
        environment.var_config(),
        key.const_data(), key.length()
    );

    if (children().size() > 1) {
        node_list_t::const_iterator i = children().begin();

        ++i;
        m_data->wait_phase = phase_lookup(
            literal_value(*i).value_as_byte_string().to_s()
        );
        ++i;
        m_data->final_phase = phase_lookup(
            literal_value(*i).value_as_byte_string().to_s()
        );
    }
}

void Var::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];
    Value value;
    bool time_to_finish = false;
    ib_rule_phase_num_t current_phase = context.ib()->rule_exec->phase;
    ib_rule_phase_num_t initial_phase = m_data->source.initial_phase();
    ib_rule_phase_num_t finish_phase = m_data->source.final_phase();

    if (
        initial_phase != IB_PHASE_NONE &&
        current_phase < initial_phase
    ) {
        // Nothing to do, yet.
        return;
    }
    if (
        m_data->wait_phase != IB_PHASE_NONE &&
        current_phase < m_data->wait_phase
    ) {
        // User wants us to do nothing, yet.
        return;
    }

    if (
        finish_phase != IB_PHASE_NONE &&
        finish_phase <= current_phase
    ) {
        // Var says it's done.
        time_to_finish = true;
    }
    if (
        m_data->final_phase != IB_PHASE_NONE &&
        m_data->final_phase <= current_phase
    ) {
        // Users says var is done.
        time_to_finish = true;
    }

    if (my_state.is_aliased()) {
        if (time_to_finish) {
            my_state.finish();
        }
        return;
    }

    try {
        value = m_data->source.get(context.var_store());
    }
    catch (enoent) {
        return;
    }

    if (
        value.is_dynamic() ||
        value.type() != Value::LIST
    ) {
        my_state.setup_local_values(context);
        my_state.add_value(value);
        my_state.finish();
    }
    else {
        my_state.alias(value.value_as_list<Value>());
        if (time_to_finish) {
            my_state.finish();
        }
    }
}

Field::Field() :
    AliasCall("var")
{
    // nop
}

string Field::name() const
{
    return "field";
}

struct Operator::data_t
{
    ConstOperator op;
    void*         instance_data;
};

Operator::Operator() :
    m_data(new data_t())
{
    // nop
}

string Operator::name() const
{
    return "operator";
}

bool Operator::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 3) && result;
    result = Validate::nth_child_is_string(reporter, 0) && result;
    result = Validate::nth_child_is_string(reporter, 1) && result;

    return result;
}

void Operator::pre_eval(Environment environment, NodeReporter reporter)
{
    // Validation guarantees that the first two children are string
    // literals and thus can be evaluated with default EvalContext.

    node_list_t::const_iterator child_i = children().begin();
    Value op_name_value = literal_value(*child_i);
    ++child_i;
    Value params_value = literal_value(*child_i);

    ConstByteString op_name = op_name_value.value_as_byte_string();
    ConstByteString params  = params_value.value_as_byte_string();

    if (! op_name) {
        reporter.error("Missing operator name.");
        return;
    }
    if (! params) {
        reporter.error("Missing parameters.");
        return;
    }

    try {
        m_data->op =
            ConstOperator::lookup(environment, op_name.to_s().c_str());
    }
    catch (IronBee::enoent) {
        reporter.error("No such operator: " + op_name.to_s());
        return;
    }

    if (! (m_data->op.capabilities() & IB_OP_CAPABILITY_NON_STREAM)) {
        reporter.error("Only non-stream operator currently supported.");
        return;
    }

    m_data->instance_data = m_data->op.create_instance(
        environment.main_context(),
        IB_OP_CAPABILITY_NON_STREAM,
        params.to_s().c_str()
    );
}

Value Operator::value_calculate(
    Value           v,
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    static string c_capture_name("capture");

    if (! m_data) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Evaluation without pre evaluation!"
            )
        );
    }

    IronBee::Field capture = IronBee::Field::create_no_copy_list<void *>(
        context.memory_pool(),
        c_capture_name.data(), c_capture_name.length(),
        List<void *>::create(context.memory_pool())
    );

    int success = 0;
    try {
        success = m_data->op.execute_instance(
            m_data->instance_data,
            context,
            v,
            capture
        );
    }
    catch (const error& e) {
        string old_what = *boost::get_error_info<errinfo_what>(e);
        e << errinfo_what(
            "Predicate operator failure for " +
            to_s() + " : " + old_what
        );
        throw e;
    }

    if (success) {
        return capture;
    }
    else {
        return Value();
    }
}

void Operator::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    const node_p& input_node = children().back();

    map_calculate(input_node, graph_eval_state, context);
}

string FOperator::name() const
{
    return "foperator";
}

Value FOperator::value_calculate(
    Value           v,
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    if (! m_data) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Evaluation without pre evaluation!"
            )
        );
    }

    int success = 0;
    try {
        success = m_data->op.execute_instance(
            m_data->instance_data,
            context,
            v
        );
    }
    catch (const error& e) {
        string old_what = *boost::get_error_info<errinfo_what>(e);
        e << errinfo_what(
            "Predicate foperator failure for " +
            to_s() + " : " + old_what
        );
        throw e;
    }

    if (success) {
        return v;
    }
    else {
        return Value();
    }
}

struct Transformation::data_t
{
    ConstTransformation transformation;
};

Transformation::Transformation() :
    m_data(new data_t())
{
    // nop
}

string Transformation::name() const
{
    return "transformation";
}

bool Transformation::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_string(reporter, 0) && result;

    return result;
}

void Transformation::pre_eval(Environment environment, NodeReporter reporter)
{
    // Validation guarantees that the first child is a string interval
    // and thus can be evaluated with default EvalContext.

    Value name_value = literal_value(children().front());
    ConstByteString name = name_value.value_as_byte_string();

    if (! name) {
        reporter.error("Missing transformation name.");
        return;
    }

    m_data->transformation = ConstTransformation::lookup(
        environment, string(name.const_data(), name.length()).c_str()
    );
}

Value Transformation::value_calculate(
    Value           v,
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    if (! m_data) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Reset without pre evaluation!"
            )
        );
    }

    return m_data->transformation.execute(context.memory_pool(), v);
}

void Transformation::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    map_calculate(children().back(), graph_eval_state, context);
}

struct WaitPhase::data_t
{
    ib_rule_phase_num_t phase;
};

WaitPhase::WaitPhase() :
    m_data(new data_t())
{
    // nop
}

string WaitPhase::name() const
{
    return "waitPhase";
}

bool WaitPhase::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_string(reporter, 0) && result;

    if (result) {
        string phase_string =
            literal_value(children().front()).value_as_byte_string().to_s();
        if (phase_lookup(phase_string) == IB_PHASE_INVALID) {
            reporter.error("Invalid phase argument: " + phase_string);
            result = false;
        }
    }

    return result;
}

void WaitPhase::pre_eval(Environment environment, NodeReporter reporter)
{
    string phase_string =
        literal_value(children().front()).value_as_byte_string().to_s();
    m_data->phase = phase_lookup(phase_string);
}

void WaitPhase::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];
    if (context.ib()->rule_exec->phase == m_data->phase) {
        graph_eval_state.eval(children().back(), context);
        my_state.forward(children().back());
    }
}

struct FinishPhase::data_t
{
    ib_rule_phase_num_t phase;
};

FinishPhase::FinishPhase() :
    m_data(new data_t())
{
    // nop
}

string FinishPhase::name() const
{
    return "finishPhase";
}

bool FinishPhase::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_string(reporter, 0) && result;

    if (result) {
        string phase_string =
            literal_value(children().front()).value_as_byte_string().to_s();
        if (phase_lookup(phase_string) == IB_PHASE_INVALID) {
            reporter.error("Invalid phase argument: " + phase_string);
            result = false;
        }
    }

    return result;
}

void FinishPhase::pre_eval(Environment environment, NodeReporter reporter)
{
    string phase_string =
        literal_value(children().front()).value_as_byte_string().to_s();
    m_data->phase = phase_lookup(phase_string);
}

Value FinishPhase::value_calculate(
    Value           v,
    GraphEvalState& ,
    EvalContext
) const
{
    return v;
}

void FinishPhase::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];

    map_calculate(children().back(), graph_eval_state, context);

    if (context.ib()->rule_exec->phase == m_data->phase) {
        my_state.finish();
    }
}

namespace {

bool ask_caseless_compare(char a, char b)
{
    return (a == b || tolower(a) == tolower(b));
}

}

string Ask::name() const
{
    return "ask";
}

bool Ask::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 2) && result;
    result = Validate::nth_child_is_string(reporter, 0) && result;

    return result;
}

void Ask::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];

    Value param_field = literal_value(children().front());
    IronBee::ConstByteString param = param_field.value_as_byte_string();

    graph_eval_state.eval(children().back(), context);
    Value collection =
        simple_value(graph_eval_state.final(children().back()->index()));

    if (collection.type() != Value::LIST) {
        my_state.finish();
    }
    else {
        if (collection.is_dynamic()) {
            ConstList<Value> result = collection.value_as_list<Value>(
                param.const_data(), param.length()
            );
            if (! result || result.empty()) {
                my_state.finish();
            }
            else {
                my_state.alias(result);
                my_state.finish();
            }
        }
        else {
            // Fall back to namedi like behavior.
            my_state.setup_local_values(context);
            BOOST_FOREACH(const Value& v, collection.value_as_list<Value>()) {
                if (
                    v.name_length() == param.length() &&
                    equal(
                        v.name(), v.name() + v.name_length(),
                        param.const_data(),
                        ask_caseless_compare
                    )
                )
                {
                    my_state.add_value(v);
                }
            }
            my_state.finish();
        }
    }
}

void load_ironbee(CallFactory& to)
{
    to
        .add<Var>()
        .add<Field>()
        .add<Operator>()
        .add<FOperator>()
        .add<Transformation>()
        .add<WaitPhase>()
        .add<FinishPhase>()
        .add<Ask>()
        ;
}

} // Standard
} // Predicate
} // IronBee
