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

#include <ironbee/predicate/standard_ironbee.hpp>

#include <ironbee/predicate/call_factory.hpp>
#include <ironbee/predicate/call_helpers.hpp>
#include <ironbee/predicate/functional.hpp>
#include <ironbee/predicate/meta_call.hpp>
#include <ironbee/predicate/validate.hpp>

#include <ironbeepp/logevent.hpp>
#include <ironbeepp/operator.hpp>
#include <ironbeepp/transformation.hpp>
#include <ironbeepp/var.hpp>

#include <ironbee/rule_engine.h>
#include <ironbee/type_convert.h>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/algorithm/string/predicate.hpp>
#include <boost/scoped_ptr.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

const string CALL_NAME_VAR("var");
const string CALL_NAME_OPERATOR("operator");
const string CALL_NAME_FOPERATOR("foperator");
const string CALL_NAME_WAITPHASE("waitPhase");
const string CALL_NAME_ASK("ask");
const string CALL_NAME_FINISHPHASE("finishPhase");
const string CALL_NAME_GENEVENT("genEvent");
const string CLALL_NAME_RULEMSG("ruleMsg");

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
    virtual const std::string& name() const;

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
    virtual const std::string& name() const;

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
    virtual const std::string& name() const;

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
    virtual const std::string& name() const;

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
    virtual const std::string& name() const;

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
    virtual const std::string& name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

protected:
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * Generate an event if the list of tags is not empty.
 **/
class GenEvent :
    public Call
{
public:
    //! Construct.
    GenEvent();

    //! See Call::name()
    virtual const std::string& name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::transform()
    bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        Environment        context,
        NodeReporter       reporter
    );

protected:

    //! See Call::eval_calculate().
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

private:
    /**
     * String original and VarExpands for child nodes that are literals.
     *
     * The VarExpand is stored at the child node's index. Child
     * number 3, if a string literal, and able to be expanded, is stored at
     * index 2.
     */
    vector<pair<string, VarExpand> > m_expansions;

    static std::string expand(
        const ConstVarExpand& var_expand,
        MemoryManager         mm,
        VarStore              var_store,
        const string&         onerror
    );
};

class RuleMsg :
    public Call
{
public:
    //! See Call::name().
    virtual const std::string& name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

protected:

    // See Call::eval_calculate().
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

const string& Var::name() const
{
    return CALL_NAME_VAR;
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

const string& Operator::name() const
{
    return CALL_NAME_OPERATOR;
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
        throw;
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

const string& FOperator::name() const
{
    return CALL_NAME_FOPERATOR;
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
        throw;
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

const string& WaitPhase::name() const
{
    return CALL_NAME_WAITPHASE;
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

const string& FinishPhase::name() const
{
    return CALL_NAME_FINISHPHASE;
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

const string& Ask::name() const
{
    return CALL_NAME_ASK;
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

GenEvent::GenEvent() : m_expansions(8)
{
}

const std::string& GenEvent::name() const
{
    return CALL_NAME_GENEVENT;
}

bool GenEvent::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 8) && result;

    // 1. Rule ID.
    result = Validate::nth_child_is_string(reporter, 0) && result;

    // 2. Rule version.
    result = Validate::nth_child_is_integer(reporter, 1) && result;

    // 3. Event type.
    result = Validate::nth_child_is_string(reporter, 2) && result;

    // 4. Event suggested action.
    result = Validate::nth_child_is_string(reporter, 3) && result;

    // 5. Event confidence.
    // No validation as it may be a string, float or int.

    // 6. Event severity.
    // No validation as it may be a string, float or int.

    // 7. Event message.
    // No validation.

    // 8. Event tags list.
    // No validation.

    return result;
}

bool GenEvent::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        context,
    NodeReporter       reporter
)
{
    //! When not done transforming return true, do no work.
    if ( Call::transform(merge_graph, call_factory, context, reporter) )
    {
        return true;
    }

    // We're done transforming. Collect expansion information.
    Engine    engine     = context.engine();
    VarConfig var_config = engine.var_config();
    size_t    child_idx  = 0;


    for (
        node_list_t::const_iterator child = children().begin();
        child != children().end();
        ++child,
        ++child_idx
    )
    {
        // Skip non-literals.
        if ( ! (*child)->is_literal() ) {
            continue;
        }

        // If we have a literal, get the value.
        Value v = reinterpret_cast<const Literal *>(child->get())->
            literal_value();

        // If it's not a string, we can't expand it. Continue.
        if (v.type() != Value::STRING) {
            continue;
        }

        ConstByteString str = v.as_string();

        // If we can't expand the string constant, continue.
        if (!VarExpand::test(str.to_s())) {
            continue;
        }

        // Record that we can expand this!
        string tmp_str = str.to_s();
        m_expansions[child_idx].first = tmp_str;
        m_expansions[child_idx].second =
            VarExpand::acquire(engine.main_memory_mm(), tmp_str, var_config);
    }

    return false;
}

std::string GenEvent::expand(
    const ConstVarExpand& var_expand,
    MemoryManager         mm,
    VarStore              var_store,
    const string&         onerror
) {
    try {
        return var_expand.execute_s(mm, var_store);
    }
    catch (const enoent& e) {
        return onerror;
    }
}

void GenEvent::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    node_p tag_node = children().back();

    // Evaluate the node that gives us a tag list.
    graph_eval_state.eval(tag_node, context);

    if (graph_eval_state.is_finished(tag_node->index())) {
        Value          tagVal    = graph_eval_state.value(tag_node->index());
        NodeEvalState& my_state  = graph_eval_state[index()];
        MemoryManager  mm        = context.memory_manager();
        VarStore       var_store = context.var_store();

        boost::function<
            std::string(const ConstVarExpand&, const string&)
        > expand_fn = boost::bind(GenEvent::expand, _1, mm, var_store, _2);

        // If tags is falsey, no action is taken. We are done.
        if (! tagVal || tagVal.is_null()) {
            my_state.finish();
            return;
        }

        // Arguments to LogEvent::create.
        // Each block assings a value to one of these or returns if a value
        // is not ready.
        std::string rule_id;
        uint64_t rule_version;
        LogEvent::type_e type;
        LogEvent::action_e action;
        uint8_t confidence;
        uint8_t severity;
        std::string msg;
        node_list_t::const_iterator child_i = children().begin();

        // Child 1 - Rule ID
        {
            graph_eval_state.eval(*child_i, context);
            if (! graph_eval_state.is_finished((*child_i)->index())) {
                return;
            }

            Value value = graph_eval_state.value((*child_i)->index());
            if (value.type() == Value::STRING) {
                rule_id = value.as_string().to_s();
            }
            else {
                BOOST_THROW_EXCEPTION(
                    einval() << errinfo_what(
                        "GenEvent argument 1 (rule_id) must be a string."
                    )
                );
            }
        }

        // Child 2 - Rule Version
        {
            ++child_i;
            graph_eval_state.eval(*child_i, context);
            if (! graph_eval_state.is_finished((*child_i)->index())) {
                return;
            }

            Value value = graph_eval_state.value((*child_i)->index());
            if (value.type() == Value::NUMBER) {
                rule_version = value.as_number();
            }
            else if (value.type() == Value::STRING) {
                ConstByteString bs = value.as_string();
                ib_num_t result;
                ib_status_t rc;

                rc = ib_type_atoi_ex(bs.const_data(), bs.length(), 10, &result);
                if (rc != IB_OK) {
                    BOOST_THROW_EXCEPTION(
                        einval() << errinfo_what(
                            "GenEvent argument 2 (rule_version) was "
                            "a string that could not be converted to a number."
                        )
                    );
                }

                rule_version = static_cast<uint64_t>(result);

            }
            else {
                BOOST_THROW_EXCEPTION(
                    einval() << errinfo_what(
                        "GenEvent argument 2 (rule_version) must be a number."
                    )
                );
            }
        }

        // Child 3 - type
        {
            ++child_i;

            // If there is an expansions, use it.
            if (m_expansions[2].second) {
                type = LogEvent::type_from_string(
                    expand_fn(
                        m_expansions[2].second,
                        m_expansions[2].first));
            }
            else {
                graph_eval_state.eval(*child_i, context);
                if (! graph_eval_state.is_finished((*child_i)->index())) {
                    return;
                }

                Value value = graph_eval_state.value((*child_i)->index());
                if (value.type() == Value::STRING) {
                    type = LogEvent::type_from_string(value.as_string().to_s());
                }
                else if (value.type() == Value::NUMBER) {
                    type = static_cast<LogEvent::type_e>(value.as_number());
                }
                else {
                    BOOST_THROW_EXCEPTION(
                        einval() << errinfo_what(
                            "GenEvent argument 3 (rule_id) must be a string "
                            "of OBSERVATION or ALERT."
                        )
                    );
                }
            }
        }

        // Child 4 - action
        {
            ++child_i;

            if (m_expansions[3].second) {
                action = LogEvent::action_from_string(
                    expand_fn(m_expansions[3].second, m_expansions[3].first));
            }
            else {
                graph_eval_state.eval(*child_i, context);
                if (! graph_eval_state.is_finished((*child_i)->index())) {
                    return;
                }

                Value value = graph_eval_state.value((*child_i)->index());
                if (value.type() == Value::STRING) {
                    action = LogEvent::action_from_string(
                        value.as_string().to_s());
                }
                else if (value.type() == Value::NUMBER) {
                    action = static_cast<LogEvent::action_e>(value.as_number());
                }
                else {
                    BOOST_THROW_EXCEPTION(
                        einval() << errinfo_what(
                            "GenEvent argument 4 (action) must be a string "
                            "of LOG, BLOCK, IGNORE or ALLOW."
                        )
                    );
                }
            }
        }

        // Child 5 - confidence
        {
            ++child_i;

            if (m_expansions[4].second) {
                std::string s = expand_fn(
                    m_expansions[4].second, m_expansions[4].first);
                ib_status_t rc;
                ib_float_t  flt;

                rc = ib_type_atof_ex(s.data(), s.length(), &flt);
                if (rc != IB_OK) {
                    ib_log_error_tx(
                        context.ib(),
                        "Confidence \"%.*s\" did not expand to number.",
                        static_cast<int>(s.length()),
                        s.data()
                    );
                    confidence = 0;
                }
                else {
                    confidence = static_cast<uint8_t>(flt);
                }
            }
            else {
                graph_eval_state.eval(*child_i, context);
                if (! graph_eval_state.is_finished((*child_i)->index())) {
                    return;
                }

                Value value = graph_eval_state.value((*child_i)->index());
                if (value.type() == Value::NUMBER) {
                    confidence = static_cast<uint8_t>(value.as_number());
                }
                else {
                    BOOST_THROW_EXCEPTION(
                        einval() << errinfo_what(
                            "GenEvent argument 5 (confidence) must be a number."
                        )
                    );
                }
            }
        }

        // Child 6 - severity
        {
            ++child_i;

            if (m_expansions[5].second) {
                std::string s = expand_fn(
                    m_expansions[5].second, m_expansions[5].first);
                ib_status_t rc;
                ib_float_t  flt;

                rc = ib_type_atof_ex(s.data(), s.length(), &flt);
                if (rc != IB_OK) {
                    ib_log_error_tx(
                        context.ib(),
                        "Severity \"%.*s\" did not expand to number.",
                        static_cast<int>(s.length()),
                        s.data()
                    );
                    severity = 0;
                }
                else {
                    severity = static_cast<uint8_t>(flt);
                }
            }
            else {
                graph_eval_state.eval(*child_i, context);
                if (! graph_eval_state.is_finished((*child_i)->index())) {
                    return;
                }

                Value value = graph_eval_state.value((*child_i)->index());
                if (value.type() == Value::NUMBER) {
                    severity = static_cast<uint8_t>(value.as_number());
                }
                else {
                    BOOST_THROW_EXCEPTION(
                        einval() << errinfo_what(
                            "GenEvent argument 6 (severity) must be a number."
                        )
                    );
                }
            }
        }

        // Child 7 - message
        {
            ++child_i;

            if (m_expansions[6].second) {
                msg = expand_fn(m_expansions[6].second, m_expansions[6].first);
            }
            else {
                graph_eval_state.eval(*child_i, context);
                if (! graph_eval_state.is_finished((*child_i)->index())) {
                    return;
                }

                Value value = graph_eval_state.value((*child_i)->index());
                if (value.type() == Value::STRING) {
                    msg = value.as_string().to_s();
                }
                else {
                    BOOST_THROW_EXCEPTION(
                        einval() << errinfo_what(
                            "GenEvent argument 7 (message) must be a string."
                        )
                    );
                }
            }
        }

        ib_log_debug_tx(
            context.ib(),
            "Predicate GenEvent creating log event for rule %.*s:%d",
            static_cast<int>(rule_id.size()),
            rule_id.data(),
            static_cast<int>(rule_version)
        );

        // Actually create the log event.
        LogEvent logEvent = LogEvent::create(
            context.memory_manager(),
            rule_id,
            type,
            action,
            confidence,
            severity,
            msg
        );

        // Child 8 - tags
        // Note - We've already evaluated and extracted the tags.
        //        This block of work is just to expand the tags
        //        and add them to the generated event.
        {
            // Add tags to logevent if tagVal is a string.
            if (tagVal.type() == Value::STRING) {
                ConstByteString bs = tagVal.as_string();

                std::string s = bs.to_s();

                if (VarExpand::test(s)) {
                    VarExpand ve = VarExpand::acquire(
                        mm, s, context.engine().var_config()
                    );
                    s = expand_fn(ve, s);
                }

                logEvent.tag_add(s);
            }

            // Add tags to logevent if the tagVal is a list of strings.
            else if (tagVal.type() == Value::LIST) {
                ConstList<Value> tags = tagVal.as_list();

                BOOST_FOREACH(Value v, tags) {
                    if (v.type() == Value::STRING) {
                        std::string s = v.as_string().to_s();

                        if (VarExpand::test(s)) {
                            MemoryManager mm = context.memory_manager();

                            VarExpand ve = VarExpand::acquire(
                                mm, s, context.engine().var_config()
                            );

                            s = expand_fn(ve, s);
                        }

                        logEvent.tag_add(s);
                    }
                }
            }
            else {
                BOOST_THROW_EXCEPTION(
                    einval() <<errinfo_what(
                        "GenEvent argument 8 must be a string or list of strings."
                    )
                );
            }
        }

        // Finaly, add the logevent to the transaction.
        throw_if_error(
            ib_logevent_add(context.ib(), logEvent.ib())
        );

        my_state.finish_true(context);
    }
}

const std::string& RuleMsg::name() const
{
    return CLALL_NAME_RULEMSG;
}

bool RuleMsg::validate(NodeReporter reporter) const
{
    bool result = true;
    result = Validate::n_children(reporter, 1) && result;

    // 1. Rule ID.
    result = Validate::nth_child_is_string(reporter, 0) && result;

    return result;
}

void RuleMsg::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    ib_rule_t    *rule;
    size_t        child_idx = children().front()->index();
    string        rule_id;
    string        rule_msg;
    MemoryManager mm = context.memory_manager();

    /* NOTE: Because we require the first child to be a string
     *       literal, we know it is finished. Just get the value. */
    rule_id = graph_eval_state.value(child_idx).as_string().to_s();

    try {
        throw_if_error(
            ib_rule_lookup(
                context.engine().ib(),
                context.context().ib(),
                rule_id.c_str(),
                &rule
            )
        );

        if (rule->meta.msg != NULL) {
            VarExpand ve(rule->meta.msg);
            rule_msg = ve.execute_s(mm, context.var_store());
        }
        else {
            rule_msg = "<no message expansion for rule ";
            rule_msg += rule_id + " (" + rule->meta.full_id + ")>";
        }
    }
    catch (const enoent& e) {
        rule_msg = "<unable to expand rule message for rule ";
        rule_msg += rule_id + " (" + rule->meta.full_id + ")>";
    }

    graph_eval_state[index()].finish(
        Value::create_string(
            mm,
            ByteString::create(mm, rule_msg.data(), rule_msg.length())
        )
    );
}

} // Anonymous

void load_ironbee(CallFactory& to)
{
    to
        .add<Var>()
        .add<Operator>()
        .add<FOperator>()
        .add<GenEvent>()
        .add<RuleMsg>()
        .add("transformation", Functional::generate<Transformation>)
        .add<WaitPhase>()
        .add<FinishPhase>()
        .add<Ask>()
        ;
}

} // Standard
} // Predicate
} // IronBee
