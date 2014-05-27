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
 * @brief Predicate --- Standard Boolean implementation.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_boolean.hpp>
#include <predicate/call_helpers.hpp>
#include <predicate/dag.hpp>
#include <predicate/functional.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/meta_call.hpp>
#include <predicate/validate.hpp>


#include <ironbeepp/operator.hpp>
#include <ironbeepp/transformation.hpp>

#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

static ScopedMemoryPoolLite s_literal_mpl;
static const Value c_true(Value::create_string(
    s_literal_mpl,
    ByteString::create(s_literal_mpl, "")
));

class False :
    public Functional::Constant
{
public:
    False() :
        Functional::Constant(Value())
    {
        // nop
    }
};

class True :
    public Functional::Constant
{
public:
    True() :
        Functional::Constant(c_true)
    {
        // nop
    }
};

/**
 * True iff any children are truthy.
 **/
class Or :
    public AbelianCall
{
public:
    //! See Call::name()
    virtual std::string name() const;

   /**
    * See Node::transform().
    *
    * Will replace self with '' if any child is true.
    * Will order children canonically.
    **/
   virtual bool transform(
       MergeGraph&        merge_graph,
       const CallFactory& call_factory,
       Environment        environment,
       NodeReporter       reporter
   );

   //! See Node::validate().
   virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate().
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * True iff all children are truthy.
 **/
class And :
    public AbelianCall
{
public:
    //! See Call::name()
    virtual std::string name() const;

   /**
    * See Node::transform().
    *
    * Will replace self with null if any child is false.
    * Will order children canonically.
    **/
   virtual bool transform(
       MergeGraph&        merge_graph,
       const CallFactory& call_factory,
       Environment        environment,
       NodeReporter       reporter
   );

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate().
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * True iff child is falsy.
 **/
class Not :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const;

    /**
     * See Node::transform().
     *
     * Will replace self with a true or false value if child is a literal.
     **/
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        Environment        environment,
        NodeReporter       reporter
    );

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate().
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * If first child is true, second child, else third child.
 **/
class If :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const;

    /**
     * See Node::transform().
     *
     * Will replace self with appropriate child if first child is literal.
     **/
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        Environment        environment,
        NodeReporter       reporter
    );

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate().
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * True iff any children are truthy (short-circuiting).
 **/
class OrSC :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const;

   /**
    * See Node::transform().
    *
    * Will replace self with '' if any child is true.
    * Will **not** order children canonically.
    **/
   virtual bool transform(
       MergeGraph&        merge_graph,
       const CallFactory& call_factory,
       Environment        environment,
       NodeReporter       reporter
   );

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate().
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * True iff all children are truthy (short-circuiting).
 **/
class AndSC :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const;

   /**
    * See Node::transform().
    *
    * Will replace self with null if any child is false.
    * Will **not** order children canonically.
    **/
   virtual bool transform(
       MergeGraph&        merge_graph,
       const CallFactory& call_factory,
       Environment        environment,
       NodeReporter       reporter
   );

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate().
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

string Or::name() const
{
    return "or";
}

void Or::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    assert(children().size() >= 2);
    NodeEvalState& my_state = graph_eval_state[index()];
    bool unfinished_child = false;
    BOOST_FOREACH(const node_p& child, children()) {
        size_t child_index = child->index();
        graph_eval_state.eval(child, context);
        if (graph_eval_state.value(child->index())) {
            my_state.finish_true(context);
            return;
        }
        if (! graph_eval_state.is_finished(child_index)) {
            unfinished_child = true;
        }
    }
    if (! unfinished_child) {
        my_state.finish();
    }
}

bool Or::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        environment,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();

    // reduced is me without my literal children.
    node_p reduced = call_factory(name());
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            if (literal_value(child)) {
                node_p replacement(new Literal(c_true));
                merge_graph.replace(me, replacement);
                return true;
            }
        }
        else {
            reduced->add_child(child);
        }
    }

    if (reduced->children().size() == 1) {
        node_p replacement = reduced->children().front();
        merge_graph.replace(me, replacement);
        return true;
    }

    if (reduced->children().size() == 0) {
        node_p replacement(new Literal());
        merge_graph.replace(me, replacement);
        return true;
    }

    if (reduced->children().size() != children().size()) {
        merge_graph.replace(me, reduced);
        return true;
    }

    return
        AbelianCall::transform(merge_graph, call_factory, environment, reporter);
}

bool Or::validate(NodeReporter reporter) const
{
    return Validate::n_or_more_children(reporter, 2);
}

string And::name() const
{
    return "and";
}

void And::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    assert(children().size() >= 2);
    NodeEvalState& my_state = graph_eval_state[index()];
    bool unfinished_child = false;
    BOOST_FOREACH(const node_p& child, children()) {
        graph_eval_state.eval(child, context);
        size_t child_index = child->index();
        if (
            graph_eval_state.is_finished(child_index) &&
            ! graph_eval_state.value(child_index)
        ) {
            my_state.finish();
            return;
        }
        if (! graph_eval_state.is_finished(child_index)) {
            unfinished_child = true;
        }
    }
    if (! unfinished_child) {
        // No unfinished children; no empty children.
        my_state.finish_true(context);
    }
}

bool And::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        environment,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();

    // reduced is me without my literal children.
    node_p reduced = call_factory(name());
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            if (! literal_value(child)) {
                node_p replacement(new Literal());
                merge_graph.replace(me, replacement);
                return true;
            }
        }
        else {
            // Dynamic child.
            reduced->add_child(child);
        }
    }

    if (reduced->children().size() == 1) {
        node_p replacement = reduced->children().front();
        merge_graph.replace(me, replacement);
        return true;
    }

    if (reduced->children().size() == 0) {
        node_p replacement(new Literal(c_true));
        merge_graph.replace(me, replacement);
        return true;
    }

    if (reduced->children().size() != children().size()) {
        merge_graph.replace(me, reduced);
        return true;
    }

    return
        AbelianCall::transform(merge_graph, call_factory, environment, reporter);
}

bool And::validate(NodeReporter reporter) const
{
    return Validate::n_or_more_children(reporter, 2);
}

string Not::name() const
{
    return "not";
}

void Not::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];

    assert(children().size() == 1);
    const node_p& child = children().front();
    graph_eval_state.eval(child, context);
    size_t child_index = child->index();
    if (graph_eval_state.value(child_index)) {
        assert(! my_state.value());
        my_state.finish();
    }
    else if (graph_eval_state.is_finished(child_index)) {
        my_state.finish_true(context);
    }
}

bool Not::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        environment,
    NodeReporter       reporter
)
{
    assert(children().size() == 1);
    const node_p& child = children().front();
    const node_cp& me = shared_from_this();

    if (child->is_literal()) {
        node_p replacement;
        if (literal_value(child)) {
            replacement.reset(new Literal());
        }
        else {
            replacement.reset(new Literal(c_true));
        }
        merge_graph.replace(me, replacement);
        return true;
    }
    else {
        return false;
    }
}

bool Not::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 1);
}

string If::name() const
{
    return "if";
}

void If::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    assert(children().size() == 3);
    NodeEvalState& my_state = graph_eval_state[index()];

    node_list_t::const_iterator i;
    i = children().begin();
    const node_p& pred = *i;
    ++i;
    const node_p& true_value = *i;
    ++i;
    const node_p& false_value = *i;

    graph_eval_state.eval(pred, context);

    if (graph_eval_state.value(pred->index())) {
        graph_eval_state.eval(true_value, context);
        my_state.forward(true_value);
    }
    else if (graph_eval_state.is_finished(pred->index())) {
        graph_eval_state.eval(false_value, context);
        my_state.forward(false_value);
    }
}

bool If::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        environment,
    NodeReporter       reporter
)
{
    assert(children().size() == 3);
    const node_cp& me = shared_from_this();
    node_list_t::const_iterator i;
    i = children().begin();
    const node_p& pred = *i;
    ++i;
    const node_p& true_value = *i;
    ++i;
    const node_p& false_value = *i;

    if (pred->is_literal()) {
        node_p replacement;
        if (literal_value(pred)) {
            replacement = true_value;
        }
        else {
            replacement = false_value;
        }
        merge_graph.replace(me, replacement);
        return true;
    }
    else {
        return false;
    }
}

bool If::validate(NodeReporter reporter) const
{
    return Validate::n_children(reporter, 3);
}

string OrSC::name() const
{
    return "orSC";
}

void OrSC::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    assert(children().size() >= 2);
    NodeEvalState& my_state = graph_eval_state[index()];
    BOOST_FOREACH(const node_p& child, children()) {
        graph_eval_state.eval(child, context);
        if (graph_eval_state.value(child->index())) {
            my_state.finish_true(context);
            return;
        }
        if (! graph_eval_state.is_finished(child->index())) {
            // Don't evaluate further children until we know this one is
            // false.
            return;
        }
    }
    // Only reach here if all children are finished and false.
    my_state.finish();
}

bool OrSC::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        environment,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();

    // reduced is me without my literal children.
    node_p reduced = call_factory(name());
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            if (literal_value(child)) {
                node_p replacement(new Literal(c_true));
                merge_graph.replace(me, replacement);
                return true;
            }
        }
        else {
            reduced->add_child(child);
        }
    }

    if (reduced->children().size() == 1) {
        node_p replacement = reduced->children().front();
        merge_graph.replace(me, replacement);
        return true;
    }

    if (reduced->children().size() == 0) {
        node_p replacement(new Literal());
        merge_graph.replace(me, replacement);
        return true;
    }

    if (reduced->children().size() != children().size()) {
        merge_graph.replace(me, reduced);
        return true;
    }

    return false;
}

bool OrSC::validate(NodeReporter reporter) const
{
    return Validate::n_or_more_children(reporter, 2);
}

string AndSC::name() const
{
    return "andSC";
}

void AndSC::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    assert(children().size() >= 2);
    NodeEvalState& my_state = graph_eval_state[index()];
    BOOST_FOREACH(const node_p& child, children()) {
        graph_eval_state.eval(child, context);
        if (! graph_eval_state.value(child->index())) {
            if (graph_eval_state.is_finished(child->index())) {
                // Known false child; we are false.
                my_state.finish();
            }
            return;
        }
    }
    // Only reached if all children are truthy.
    my_state.finish_true(context);
}

bool AndSC::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        environment,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();

    // reduced is me minus my literal children
    node_p reduced = call_factory(name());
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            if (! literal_value(child)) {
                node_p replacement(new Literal());
                merge_graph.replace(me, replacement);
                return true;
            }
        }
        else {
            reduced->add_child(child);
        }
    }

    if (reduced->children().size() == 1) {
        node_p replacement = reduced->children().front();
        merge_graph.replace(me, replacement);
        return true;
    }

    if (reduced->children().size() == 0) {
        node_p replacement(new Literal(c_true));
        merge_graph.replace(me, replacement);
        return true;
    }

    if (reduced->children().size() != children().size()) {
        merge_graph.replace(me, reduced);
        return true;
    }

    return false;
}

bool AndSC::validate(NodeReporter reporter) const
{
    return Validate::n_or_more_children(reporter, 2);
}

} // Anonymous

void load_boolean(CallFactory& to)
{
    to
        .add("false", Functional::generate<False>)
        .add("true",  Functional::generate<True>)
        .add<Or>()
        .add<And>()
        .add<Not>()
        .add<If>()
        .add<OrSC>()
        .add<AndSC>()
        ;
}

} // Standard
} // Predicate
} // IronBee
