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
#include <predicate/functional.hpp>
#include <predicate/meta_call.hpp>
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


/**
 * Returns var with name given by child.
 *
 * Long form has three children: name, initial phase, and final phase.
 **/
class Var :
    public Call
{
public:
    //! Constructor.
    Var();

    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::pre_eval()
    virtual void pre_eval(Environment environment, NodeReporter reporter);

protected:
    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

private:
    //! Hidden complex implementation details.
    struct data_t;

    //! Hidden complex implementation details.
    boost::scoped_ptr<data_t> m_data;
};

/**
 * Run IronBee operator.
 *
 * First child is name of operator, second is parameters, third is input.
 * First and second must be string literals.  Values are the capture
 * collections for any inputs values for which the operator returned 1.
 **/
class Operator :
    public MapCall
{
public:
    //! Constructor.
    Operator();

    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::pre_eval()
    virtual void pre_eval(Environment environment, NodeReporter reporter);

protected:
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
    virtual Value value_calculate(
        Value           v,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

    //! Hidden complex implementation details.
    struct data_t;

    //! Hidden complex implementation details.
    boost::scoped_ptr<data_t> m_data;
};

/**
 * Run IronBee operator as a filter.
 *
 * First child is name of operator, second is parameters, third is input.
 * First and second must be string literals.  Values are the input values
 * for which the operator returned 1.
 **/
class FOperator :
    public Operator
{
public:
    //! See Call:name()
    virtual std::string name() const;

protected:
    virtual Value value_calculate(
        Value           v,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * Run IronBee transformation.
 *
 * Execute an IronBee transformation.  The first child must be a string
 * literal naming the transformation.  The second child is the argument.  The
 * third is the input.
 **/
class Transformation :
    public Functional::Map
{
public:
    //! Constructor.
    Transformation() : Functional::Map(2, 1) {}

    //! See Functional::Base::validate_argument()
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0 || n == 1) {
            Validate::value_is_type(v, Value::STRING, reporter);
        }
    }

    //! See Functional::Base::prepare()
    bool prepare(
        MemoryManager                  mm,
        const Functional::value_vec_t& static_args,
        Environment                    environment,
        NodeReporter                   reporter
    )
    {
        if (! environment) {
            return false;
        }

        Value name = static_args[0];
        Value arg = static_args[1];

        m_transformation_instance = TransformationInstance::create(
            mm,
            ConstTransformation::lookup(
                environment.engine(),
                name.as_string().const_data(), name.as_string().length()
            ),
            arg.as_string().to_s().c_str()
        );

        return true;
    }

protected:
    //! See Functional::Map::eval_map()
    Value eval_map(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    map_state,
        Value                          subvalue
    ) const
    {
        return Value(
            m_transformation_instance.execute(
                mm,
                subvalue.to_field()
            )
        );
    }

private:
    //! Transformation instance.
    ConstTransformationInstance m_transformation_instance;
};

/**
 * Do no child evaluation until a certain phase.
 **/
class WaitPhase :
    public Call
{
public:
    //! Constructor.
    WaitPhase();

    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::pre_eval()
    virtual void pre_eval(Environment environment, NodeReporter reporter);

protected:
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

private:
    //! Hidden complex implementation details.
    struct data_t;

    //! Hidden complex implementation details.
    boost::scoped_ptr<data_t> m_data;
};

/**
 * Copy children's values but finish once given phase is reached.
 **/
class FinishPhase :
    public MapCall
{
public:
    //! Constructor.
    FinishPhase();

    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::pre_eval()
    virtual void pre_eval(Environment environment, NodeReporter reporter);

protected:
    virtual Value value_calculate(
        Value           v,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

private:
    //! Hidden complex implementation details.
    struct data_t;

    //! Hidden complex implementation details.
    boost::scoped_ptr<data_t> m_data;
};

/**
 * Ask a dynamic collection a question.
 **/
class Ask :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

protected:
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};


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
                phase = literal_value(*i).as_string().to_s();
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
    IronBee::ConstByteString key = key_field.as_string();

    m_data->source = VarSource::acquire(
        environment.engine().main_memory_mm(),
        environment.engine().var_config(),
        key.const_data(), key.length()
    );

    if (children().size() > 1) {
        node_list_t::const_iterator i = children().begin();

        ++i;
        m_data->wait_phase = phase_lookup(
            literal_value(*i).as_string().to_s()
        );
        ++i;
        m_data->final_phase = phase_lookup(
            literal_value(*i).as_string().to_s()
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
        value = Value(m_data->source.get(context.var_store()));
    }
    catch (enoent) {
        return;
    }

    if (
        value.to_field().is_dynamic() ||
        value.type() != Value::LIST
    ) {
        my_state.finish(value);
    }
    else {
        my_state.alias(value);
        if (time_to_finish) {
            my_state.finish();
        }
    }
}

struct Operator::data_t
{
    ScopedMemoryPoolLite mpl;
    ConstOperatorInstance instance;
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

    ConstByteString op_name = op_name_value.as_string();
    ConstByteString params  = params_value.as_string();
    ConstOperator op;

    if (! op_name) {
        reporter.error("Missing operator name.");
        return;
    }
    if (! params) {
        reporter.error("Missing parameters.");
        return;
    }

    try {
        op = ConstOperator::lookup(
            environment.engine(),
            op_name.to_s().c_str()
        );
    }
    catch (IronBee::enoent) {
        reporter.error("No such operator: " + op_name.to_s());
        return;
    }

    m_data->instance = OperatorInstance::create(
        m_data->mpl,
        environment,
        op,
        IB_OP_CAPABILITY_NONE,
        params.to_s().c_str()
    );
}

Value Operator::value_calculate(
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

    IronBee::Field capture = IronBee::Field::create_no_copy_list<void *>(
        context.memory_manager(),
        (v ? v.name() : ""), (v ? v.name_length() : 0),
        List<void *>::create(context.memory_manager())
    );

    int success = 0;
    try {
        success = m_data->instance.execute(
            context,
            (v ? v.to_field() : Field()),
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
        return Value(capture);
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
        success = m_data->instance.execute(
            context,
            v.to_field()
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
            literal_value(children().front()).as_string().to_s();
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
        literal_value(children().front()).as_string().to_s();
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
            literal_value(children().front()).as_string().to_s();
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
        literal_value(children().front()).as_string().to_s();
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
    IronBee::ConstByteString param = param_field.as_string();

    graph_eval_state.eval(children().back(), context);
    Value collection =
        graph_eval_state.final(children().back()->index()).value();

    if (collection.type() != Value::LIST) {
        my_state.finish();
    }
    else {
        if (collection.to_field().is_dynamic()) {
            ConstList<Value> result =
                collection.to_field().value_as_list<Value>(
                    param.const_data(), param.length()
                );
            if (! result || result.empty()) {
                my_state.finish();
            }
            else {
                my_state.finish(
                    Value::alias_list(context.memory_manager(), result)
                );
            }
        }
        else {
            // Fall back to namedi like behavior.
            my_state.setup_local_list(context.memory_manager());
            BOOST_FOREACH(const Value& v, collection.as_list()) {
                if (
                    v.name_length() == param.length() &&
                    equal(
                        v.name(), v.name() + v.name_length(),
                        param.const_data(),
                        ask_caseless_compare
                    )
                )
                {
                    my_state.append_to_list(v);
                }
            }
            my_state.finish();
        }
    }
}

} // Anonymous

void load_ironbee(CallFactory& to)
{
    to
        .add<Var>()
        .add<Operator>()
        .add<FOperator>()
        .add("transformation", Functional::generate<Transformation>)
        .add<WaitPhase>()
        .add<FinishPhase>()
        .add<Ask>()
        ;
}

} // Standard
} // Predicate
} // IronBee
