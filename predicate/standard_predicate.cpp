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
const string CALL_LABEL("label");
const string CALL_CALL("call");
const string CALL_TAGNODE("tag");
const string CALL_CALLTAGGEDNODES("callTagged");

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
    NodeEvalState& my_state = graph_eval_state[index()];
    node_list_t::const_iterator last_unfinished = children().begin();
    my_state.state() = last_unfinished;
    my_state.setup_local_list(context.memory_manager());
}

void FinishAll::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];

    node_list_t::const_iterator last_unfinished = children().end();

    for (
        node_list_t::const_iterator i =
            boost::any_cast<node_list_t::const_iterator>(my_state.state());
        i != children().end();
        ++i
    )
    {
        size_t index = (*i)->index();

        // We may re-check a node that is already done on subsequent evals.
        if (graph_eval_state.is_finished(index)) {
            continue;
        }

        // If the node is not finished, eval it.
        graph_eval_state.eval(*i, context);

        // If the value is finished, record its value.
        if (graph_eval_state.is_finished(index)) {
            Value v = graph_eval_state.value(index);
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
        graph_eval_state.eval(*i, context);

        if (graph_eval_state.is_finished((*i)->index())) {
            Value v = graph_eval_state.value((*i)->index());
            NodeEvalState& my_state = graph_eval_state[index()];
            my_state.finish(v);
            return;
        }
    }
}

class Label : public Call {

public:
    Label();

    virtual void eval_initialize(
        GraphEvalState &graph_eval_state,
        EvalContext context
    ) const;

    virtual void eval_calculate(
        GraphEvalState &graph_eval_state,
        EvalContext context
    ) const;

    void apply_label(GraphEvalState &graph_eval_state, const std::string& label);

    virtual const std::string& name() const;

};

Label::Label() : Call() {}

const std::string& Label::name() const { return CALL_LABEL; }

void Label::eval_initialize(
    GraphEvalState &graph_eval_state,
    EvalContext context
) const
{
    node_cp child1 = children().front();

    if (!child1->is_literal()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Argument 1 must be a literal for label nodes."
            )
        );
    }

    // Literal values aren't ready until after their initialize phase.
    // Because we want the literal value "early" we cast down the pointer
    // and grab the value directly.
    Value v = reinterpret_cast<const Literal*>(child1.get())->literal_value();

    ConstByteString bs = v.as_string();

    std::string label(bs.const_data(), bs.length());

    const_cast<Label *>(this)->apply_label(graph_eval_state, label);
}

void Label::apply_label(
    GraphEvalState &graph_eval_state,
    const std::string &label
) {
    graph_eval_state.label_node(shared_from_this(), label);
}


void Label::eval_calculate(
    GraphEvalState &graph_eval_state,
    EvalContext context
) const {

    // Get a child iterator.
    node_list_t::const_iterator child_i = children().begin();

    // Skip the first value. This is the label name.
    ++child_i;

    MemoryManager mm = context.memory_manager();

    /* If there are many returned values, collect them this way. */
    if (children().size() > 2) {
        graph_eval_state[index()].setup_local_list(mm);

        for (; child_i != children().end(); ++child_i) {
            node_cp c = *child_i;

            if (!graph_eval_state.is_finished(c->index())) {
                graph_eval_state.eval(c, context);

                if (!graph_eval_state.is_finished(c->index())) {
                    return;
                }

                graph_eval_state[index()].append_to_list(
                    graph_eval_state.value(c->index()));
            }
        }

        graph_eval_state[index()].finish();
    }
    else {
        node_cp c = *child_i;

        if (!graph_eval_state.is_finished(c->index())) {
            graph_eval_state.eval(c, context);

            if (!graph_eval_state.is_finished(c->index())) {
                return;
            }

            graph_eval_state[index()].finish(
                graph_eval_state.value(c->index()));
        }
    }
}

class CallLabeledNode : public Call {

public:
    CallLabeledNode();

    void eval_calculate(
        GraphEvalState &graph_eval_state,
        EvalContext context
    ) const;

    void eval_initialize(
        GraphEvalState &graph_eval_state,
        EvalContext context
    ) const;

    void forward(
        GraphEvalState &graph_eval_state,
        const std::string &label
    ) const;

    virtual const std::string& name() const;
};

CallLabeledNode::CallLabeledNode() : Call() {}

const std::string& CallLabeledNode::name() const { return CALL_CALL; }

void CallLabeledNode::forward(
    GraphEvalState &graph_eval_state,
    const std::string &label
) const {
    node_p n = graph_eval_state.node_by_label(label);
    graph_eval_state[index()].forward(n);
}

void CallLabeledNode::eval_initialize(
    GraphEvalState &graph_eval_state,
    EvalContext context
) const
{

    node_cp cp = children().front();

    if (! cp->is_literal()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Argument 1 must be a literal for label nodes."
            )
        );
    }

    // Literal values aren't ready until after their initialize phase.
    // Because we want the literal value "early" we cast down the pointer
    // and grab the value directly.
    Value v = reinterpret_cast<const Literal*>(cp.get())->literal_value();

    ConstByteString bs = v.as_string();

    std::string label(bs.const_data(), bs.length());

    forward(graph_eval_state, label);
}

void CallLabeledNode::eval_calculate(
    GraphEvalState &graph_eval_state,
    EvalContext context
) const
{
    /* If this ever executes we haven't forwarded to the other node yet.
     * This is not good. Setup forwarding now. */
    Value v = graph_eval_state.value(children().front()->index());

    ConstByteString bs = v.as_string();

    std::string label = std::string(bs.const_data(), bs.length());

    forward(graph_eval_state, label);
}

/***************************************************************************
 * Call a list of tagged nodes.
 ***************************************************************************/

class CallTaggedNodes : public Call {

public:

    void eval_calculate(
        GraphEvalState &graph_eval_state,
        EvalContext context
    ) const;

    void eval_initialize(
        GraphEvalState &graph_eval_state,
        EvalContext context
    ) const;

    virtual const std::string& name() const;
};

void CallTaggedNodes::eval_initialize(
    GraphEvalState &graph_eval_state,
    EvalContext context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];

    node_list_t::const_iterator last_unfinished = children().begin();

    my_state.state() = last_unfinished;
    my_state.setup_local_list(context.memory_manager());
}

void CallTaggedNodes::eval_calculate(
    GraphEvalState &graph_eval_state,
    EvalContext context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];

    // From our last known unfinished node until the end, try to evaluate.
    for (
        node_list_t::const_iterator i =
            boost::any_cast<node_list_t::const_iterator>(my_state.state());
        i != children().end();
        ++i
    )
    {
        /* Try to finish last-unfinished.
         * If it doesn't finish, exit. */
        size_t index = (*i)->index();

        // See if this node is finished.
        if (!graph_eval_state.is_finished(index)) {
            // If the node is not finished, eval it.
            graph_eval_state.eval(*i, context);
        }

        // If the node is finished now, add it and keep going.
        if (graph_eval_state.is_finished(index)) {
            Value v = graph_eval_state.value(index);
            my_state.append_to_list(v);
        }
        // else stop and record where we are.
        else {
            my_state.state() = i;
            return;
        }
    }

    // If we get here, we're done.
    my_state.state() = children().end();
    my_state.finish();
}

const std::string& CallTaggedNodes::name() const
{
    return CALL_CALLTAGGEDNODES;
}

/***************************************************************************
 * Tag a node in the graph eval state.
 ***************************************************************************/
class CallTagNode : public Call {
public:
    virtual const std::string& name() const;

    void eval_initialize(
        GraphEvalState &graph_eval_state,
        EvalContext context
    ) const;

    void eval_calculate(
        GraphEvalState &graph_eval_state,
        EvalContext context
    ) const;

    void tag_children(
        GraphEvalState &graph_eval_state,
        Value&          v
    ) const;
};

const std::string& CallTagNode::name() const {
    return CALL_TAGNODE;
}

void CallTagNode::tag_children(
    GraphEvalState& graph_eval_state,
    Value&          v
) const {


    switch (v.type()) {
        case Value::NUMBER:
        case Value::FLOAT:
        case Value::STRING:
        {
            node_list_t::const_iterator i = children().begin();
            ConstByteString bstag = v.as_string();
            std::string tag(bstag.const_data(), bstag.length());

            for (++i; i != children().end(); ++i)
            {
                graph_eval_state.tag_node(*i, tag);
            }
            break;
        }
        // When the value is a list, unwrap it and recurse.
        case Value::LIST:
            ConstList<Value> l = v.as_list();
            BOOST_FOREACH(Value v, l) {
                tag_children(graph_eval_state, v);
            }
            break;
    }
}

void CallTagNode::eval_initialize(
    GraphEvalState &graph_eval_state,
    EvalContext context
) const
{

    node_list_t::const_iterator i = children().begin();

    if (i == children().end()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Tag requires two children. A tag and at least 1 child."
            )
        );
    }

    if (! (*i)->is_literal()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Argument 1 must be a literal for tagging nodes."
            )
        );
    }

    // Literal values aren't ready until after their initialize phase.
    // Because we want the literal value "early" we cast down the pointer
    // and grab the value directly.
    Value v = reinterpret_cast<const Literal*>(i->get())->literal_value();

    tag_children(graph_eval_state, v);

}

void CallTagNode::eval_calculate(
    GraphEvalState &graph_eval_state,
    EvalContext context
) const {

    node_list_t::const_iterator i = children().begin();
    MemoryManager mm = context.memory_manager();

    /* Make sure we are not at the end of the children. */
    if (i == children().end()) {
        BOOST_THROW_EXCEPTION(
            einval() << errinfo_what(
                "Tag requires two children. A tag and at least 1 child."
            )
        );
    }

    graph_eval_state[index()].setup_local_list(mm);

    bool unfinished = false;

    /* For all children but the first one, evaluate and add to a list. */
    for (++i; i != children().end(); ++i)
    {
        size_t idx = (*i)->index();

        if (! graph_eval_state[idx].is_finished()) {
            graph_eval_state.eval(*i, context);

            // If we don't finish a node. Record it an continue.
            if (!graph_eval_state[idx].is_finished()) {
                unfinished |= true;
                continue;
            }
            // When we do finish a node, record the value.
            else {
                Value v = graph_eval_state[idx].value();

                graph_eval_state[index()].append_to_list(v);
            }
        }
    }

    if (!unfinished) {
        graph_eval_state[index()].finish();
    }
}

} // Anonymous


void load_predicate(CallFactory& to)
{
    to
        .add<IsLiteral>()
        .add<FinishAll>()
        .add<FinishAny>()
        .add<Label>()
        .add<CallLabeledNode>()
        .add<CallTagNode>()
        .add<CallTaggedNodes>()
        .add("isFinished", Functional::generate<IsFinished>)
        .add("isLonger", Functional::generate<IsLonger>)
        .add("isList", Functional::generate<IsList>)
        ;
}

} // Standard
} // Predicate
} // IronBee
