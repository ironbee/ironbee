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

#include <ironbee/predicate/standard_predicate.hpp>

#include <ironbee/predicate/call_factory.hpp>
#include <ironbee/predicate/call_helpers.hpp>
#include <ironbee/predicate/functional.hpp>
#include <ironbee/predicate/merge_graph.hpp>
#include <ironbee/predicate/validate.hpp>

#include <ironbeepp/list.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

const string CALL_NAME_ISLITERAL("isLiteral");
const string CALL_FINISH_ALL("finishAll");
const string CALL_FINISH_SOME("finishAny");

//! Scoped Memory Pool Lite
static ScopedMemoryPoolLite s_mpl;
//! True Value
static const Value c_true_value =
    Value::create_string(s_mpl, ByteString::create(s_mpl, ""));
//! True literal.
static const node_p c_true(new Literal(c_true_value));
//! False literal.
static const node_p c_false(new Literal());

/**
 * Is argument a literal?
 **/
class IsLiteral :
    public Call
{
public:
    //! See Call::name()
    const std::string& name() const
    {
        return CALL_NAME_ISLITERAL;
    }

    /**
     * See Node::transform().
     *
     * Will replace self with true or false based on child.
     **/
    bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        Environment        environment,
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

    //! See Node::eval_calculate()
    void eval_calculate(GraphEvalState&, EvalContext) const
    {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "IsLiteral evaluated.  Did you not transform?"
            )
        );
    }

    //! See Node::validate().
    bool validate(NodeReporter reporter) const
    {
        return Validate::n_children(reporter, 1);
    }
};

/**
 * Is argument finished?
 **/
class IsFinished :
    public Functional::Primary
{
public:
    //! Constructor.
    IsFinished() : Functional::Primary(0, 1) {}

protected:
    //! See Functional::Primary::eval_primary().
    void eval_primary(
        MemoryManager                  mm,
        const node_cp&                 me,
        boost::any&                    substate,
        NodeEvalState&                 my_state,
        const Functional::value_vec_t& secondary_args,
        const NodeEvalState&           primary_arg
    ) const
    {
        if (primary_arg.is_finished()) {
            my_state.finish(c_true_value);
        }
    }
};

/**
 * Is primary argument a list longer than specified length.
 **/
class IsLonger :
    public Functional::Primary
{
public:
    //! Constructor.
    IsLonger() : Functional::Primary(0, 2) {}

protected:
    //! See Functional::Base::validate_argument().
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            Validate::value_is_type(v, Value::NUMBER, reporter);
        }
    }

    //! See Functional::Primary::eval_primary().
    void eval_primary(
        MemoryManager                  mm,
        const node_cp&                 me,
        boost::any&                    substate,
        NodeEvalState&                 my_state,
        const Functional::value_vec_t& secondary_args,
        const NodeEvalState&           primary_arg
    ) const
    {
        if (! primary_arg.value()) {
            return;
        }
        if (primary_arg.value().type() != Value::LIST) {
            my_state.finish();
            return;
        }
        if (
            primary_arg.value().as_list().size() >
            size_t(secondary_args[0].as_number())
        ) {
            my_state.finish(c_true_value);
            return;
        }
        if (primary_arg.is_finished()) {
            my_state.finish();
        }
    }
};

/**
 * Is argument a list?
 **/
class IsList :
    public Functional::Primary
{
public:
    //! Constructor.
    IsList() : Functional::Primary(0, 1) {}

protected:
    //! See Functional::Primary::eval_primary().
    void eval_primary(
        MemoryManager                  mm,
        const node_cp&                 me,
        boost::any&                    substate,
        NodeEvalState&                 my_state,
        const Functional::value_vec_t& secondary_args,
        const NodeEvalState&           primary_arg
    ) const
    {
        if (! primary_arg.value().is_null()) {
            if (primary_arg.value().type() == Value::LIST) {
                my_state.finish(c_true_value);
            }
            else {
                my_state.finish();
            }
        }
    }
};

/**
 * Finish with the value of the first child that finishes.
 *
 * This is unlike OR in that the requirements are less. A node only need
 * finish, not finish true.
 **/
 class FinishAny :
    public Call
{
public:
    //! See Call:name()
    virtual const std::string& name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    /**
     * If any node is a literal or transformed to finished,
     * this node replaces itself with that node.
     *
     * @sa See Node::trasform()
     */
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        Environment        environment,
        NodeReporter       reporter
    );

protected:
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * Finish with a list of the values. This finishes when all values are finished.
 *
 * This is effectively list.
 **/
 class FinishAll :
    public Call
{
public:
    //! See Call:name()
    virtual const std::string& name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    /**
     * If all nodesa are a literal or transformed to finished,
     * this node replaces itself with a constant list of those values.
     *
     * @sa See Node::trasform()
     */
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        Environment        environment,
        NodeReporter       reporter
    );

protected:

    virtual void eval_initialize(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

bool FinishAll::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        environment,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();

    if (children().size() == 0) {
        node_p replacement(new Literal());
        merge_graph.replace(me, replacement);
        return true;
    }

    {
        boost::shared_ptr<ScopedMemoryPoolLite> mpl(
            new ScopedMemoryPoolLite()
        );
        IronBee::List<Value> my_value = IronBee::List<Value>::create(*mpl);
        bool replace = true;

        BOOST_FOREACH(const node_p& child, children()) {
            if (! child->is_literal()) {
                replace = false;
                break;
            }
            Value v = literal_value(child);
            my_value.push_back(v);
        }

        if (replace) {
            node_p replacement(new Literal(mpl,
                Value::alias_list(*mpl, my_value)
            ));
            merge_graph.replace(me, replacement);
            return true;
        }
    }

    return false;
}

const std::string& FinishAll::name() const
{
    return CALL_FINISH_ALL;
}

bool FinishAll::validate(NodeReporter reporter) const
{
    return true;
}

void FinishAll::eval_initialize(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state.node_eval_state(this->index());
    node_list_t::const_iterator last_unfinished = children().begin();
    my_state.state() = last_unfinished;
    my_state.setup_local_list(context.memory_manager());
}

void FinishAll::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state.node_eval_state(this, context);

    node_list_t::const_iterator last_unfinished = children().end();

    for (
        node_list_t::const_iterator i =
            boost::any_cast<node_list_t::const_iterator>(my_state.state());
        i != children().end();
        ++i
    )
    {
        const Node *n = i->get();

        // We may re-check a node that is already done on subsequent evals.
        if (graph_eval_state.is_finished(n, context)) {
            continue;
        }

        // If the node is not finished, eval it.
        graph_eval_state.eval(n, context);
        NodeEvalState& nes = graph_eval_state.final(n, context);

        // If the value is finished, record its value.
        if (nes.is_finished()) {
            Value v = nes.value();
            my_state.append_to_list(v);
        }
        // If i is not finished and last_unfinished == end, update it.
        else if (last_unfinished == children().end()) {
            last_unfinished = i;
        }
    }

    // If last_unfinished was never updated to an unfinished i, we are done!
    if (last_unfinished == children().end()) {
        my_state.finish();
    }

    // Record where we observed the first unfinished node.
    my_state.state() = last_unfinished;
}

bool FinishAny::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        environment,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();

    if (children().size() == 0) {
        node_p replacement(new Literal());
        merge_graph.replace(me, replacement);
        return true;
    }

    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            node_p c(child);
            merge_graph.replace(me, c);
            return true;
        }
    }

    return false;
}

const std::string& FinishAny::name() const
{
    return CALL_FINISH_SOME;
}

bool FinishAny::validate(NodeReporter reporter) const
{
    return true;
}

void FinishAny::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    for (
        node_list_t::const_iterator i = children().begin();
        i != children().end();
        ++i
    )
    {
        const Node* n = i->get();
        graph_eval_state.eval(n, context);
        NodeEvalState& nes = graph_eval_state.final(n, context);

        if (nes.is_finished()) {
            Value v = nes.value();
            NodeEvalState& my_state = graph_eval_state.node_eval_state(this, context);
            my_state.finish(v);
            return;
        }
    }
}

} // Anonymous


void load_predicate(CallFactory& to)
{
    to
        .add<IsLiteral>()
        .add<FinishAll>()
        .add<FinishAny>()
        .add("isFinished", Functional::generate<IsFinished>)
        .add("isLonger", Functional::generate<IsLonger>)
        .add("isList", Functional::generate<IsList>)
        ;
}

} // Standard
} // Predicate
} // IronBee
