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
 * @brief Predicate --- Functional Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/predicate/functional.hpp>

#include <ironbee/predicate/call_helpers.hpp>
#include <ironbee/predicate/merge_graph.hpp>
#include <ironbee/predicate/reporter.hpp>
#include <ironbee/predicate/validate.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Functional {

namespace Impl {

/* Doxygen gets confused here. */
#ifndef DOXYGEN_SKIP
Call::Call(const string& name, const base_p& base) :
    m_base(base),
    m_name(name)
{
    // nop
}
#endif

void Call::pre_transform(NodeReporter reporter) const
{
    Predicate::Call::pre_transform(reporter);
    bool okay = Validate::n_children(
        reporter,
        m_base->num_static_args() + m_base->num_dynamic_args()
    );
    if (! okay) {
        return;
    }

    // Validate any literal children.
    size_t i = 0;
    BOOST_FOREACH(const node_p& child, children()) {
        if (child->is_literal()) {
            m_base->validate_argument(
                i,
                literal_value(child),
                reporter
            );
        }
        ++i;
    }
}

void Call::post_transform(NodeReporter reporter) const
{
    Predicate::Call::post_transform(reporter);

    const size_t n = children().size();
    node_list_t::const_iterator iter;
    size_t i;

    // Validate static arguments.
    for (
        i = 0, iter = children().begin();
        i < min(m_base->num_static_args(), n) &&
        iter != children().end();
        ++i, ++iter
    ) {
        bool literal_result = Validate::nth_child_is_literal(reporter, i);
        if (literal_result) {
            m_base->validate_argument(
                i,
                literal_value(*iter),
                reporter
            );
        }
    }

    // Validate dynamic arguments that happen to be literal.
    for (; iter != children().end(); ++i, ++iter) {
        if ((*iter)->is_literal()) {
            m_base->validate_argument(
                i,
                literal_value(*iter),
                reporter
            );
        }
    }
}

namespace {

bool prepare_call(
    Call&         me,
    Base&         base,
    MemoryManager mm,
    Environment   environment,
    NodeReporter  reporter
)
{
    value_vec_t static_args;
    size_t i;
    node_list_t::const_iterator iter;
    for (
        i = 0, iter = me.children().begin();
        i < base.num_static_args();
        ++i, ++iter
    ) {
        static_args.push_back(literal_value(*iter));
    }

    return base.prepare(mm, static_args, environment, reporter);
}

} // Anonymous

bool Call::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        environment,
    NodeReporter       reporter
)
{
    // Only do something if all arguments are literal.
    BOOST_FOREACH(const node_p& arg, children()) {
        if (! arg->is_literal()) {
            return m_base->transform(
                shared_from_this(),
                merge_graph,
                call_factory,
                environment,
                reporter
            );
        }
    }

    // All arguments are literal.  Set up a graph eval state for just this
    // subtree.  Note that transformations must happen before final indexing.
    set_index(0);
    size_t index = 1;
    BOOST_FOREACH(const node_p& arg, children()) {
        arg->set_index(index);
        ++index;
    }
    GraphEvalState ges(index);
    set<node_p> initialized;
    BOOST_FOREACH(const node_p& arg, children()) {
        // None of this would work if we had non-literal args.
        assert(arg->is_literal());
        if (! initialized.count(arg)) {
            ges.initialize(arg.get(), EvalContext());
            initialized.insert(arg);
        }
    }
    boost::shared_ptr<ScopedMemoryPoolLite> mpl(new ScopedMemoryPoolLite());
    boost::any substate;
    bool prepared = prepare_call(*this, *m_base, *mpl, environment, reporter);
    if (prepared)  {
        // Construct a fake EvalContext that only contains our memory manager.
        ib_tx_t ib_eval_context;
        memset(&ib_eval_context, 0, sizeof(ib_eval_context));
        ib_eval_context.mm = MemoryManager(*mpl).ib();
        EvalContext eval_context(&ib_eval_context);
        ges.initialize(this, eval_context);
        ges.eval(this, eval_context);
        // This use of index_final() is necessary and correct.
        const NodeEvalState& my_state = ges.index_final(0);

        if (my_state.is_finished()) {
            // Here we pass the mpl shared pointer on to the new Literal node.
            // If this statement is not run, the mpl and any work is
            // discarded at the end of this function.
            node_p replacement(new Literal(mpl, my_state.value()));

            merge_graph.replace(shared_from_this(), replacement);

            return true;
        }
    }

    // If we reached here, did not pre-evaluate, so give base a chance.
    return m_base->transform(
        shared_from_this(),
        merge_graph,
        call_factory,
        environment,
        reporter
    );
}

void Call::pre_eval(Environment environment, NodeReporter reporter)
{
    prepare_call(
        *this,
        *m_base,
        environment.engine().main_memory_mm(),
        environment,
        reporter
    );
}

namespace {

typedef pair<const Node*, size_t> arg_with_index_t;
typedef list<arg_with_index_t> arg_list_t;

struct call_state_t {
    arg_list_t unfinished;
    boost::any substate;
};
typedef boost::shared_ptr<call_state_t> call_state_p;

void eval_args(
    arg_list_t&     args,
    const Base&     base,
    GraphEvalState& graph_eval_state,
    EvalContext     context
)
{
    arg_list_t::iterator iter = args.begin();
    while (iter != args.end()) {
        const Node* n = iter->first;
        NodeEvalState& n_nes = graph_eval_state.eval(n, context);
        if (n_nes.is_finished()) {
            Reporter reporter(false);
            NodeReporter node_reporter(reporter, iter->first->shared_from_this());
            base.validate_argument(iter->second, n_nes.value(), node_reporter);
            if (reporter.num_errors() > 0) {
                stringstream report;
                reporter.write_report(report);
                BOOST_THROW_EXCEPTION(
                    einval() << errinfo_what(
                        (
                            boost::format(
                                "Argument validation failed: "
                                "n=%d report=%s arg=%s"
                            ) %
                            iter->second %
                            report.str() %
                            n->to_s()
                        ).str()
                    )
                );
            }

            arg_list_t::iterator to_remove = iter;
            ++iter;
            args.erase(to_remove);
        }
        else {
            ++iter;
        }
    }
}

} // Anonymous

void Call::eval_initialize(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    node_cp me = shared_from_this();
    call_state_p call_state(new call_state_t);

    Predicate::Call::eval_initialize(graph_eval_state, context);

    node_list_t::const_iterator iter;
    size_t i;
    for (
        iter = children().begin(), i = 0;
        iter != children().end();
        ++i, ++iter
    ) {
        if (! (*iter)->is_literal()) {
            call_state->unfinished.push_back(make_pair(iter->get(), i));
        }
    }

    m_base->eval_initialize(
        context.memory_manager(),
        me,
        call_state->substate,
        graph_eval_state
    );

    graph_eval_state.node_eval_state(index()).state() = call_state;
}

void Call::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state.node_eval_state(this, context);
    call_state_p call_state = boost::any_cast<call_state_p>(my_state.state());

    eval_args(call_state->unfinished, *m_base, graph_eval_state, context);

    m_base->eval(
        context.memory_manager(),
        shared_from_this(),
        call_state->substate,
        graph_eval_state,
        context
    );
}

} // Impl

Base::Base(
    size_t num_static_args,
    size_t num_dynamic_args
) :
    m_num_static_args(num_static_args),
    m_num_dynamic_args(num_dynamic_args)
{
    // nop
}

void Base::validate_argument(
    int          n,
    Value        v,
    NodeReporter reporter
) const
{
    // nop
}

void Base::eval_initialize(
    MemoryManager   mm,
    const node_cp&  me,
    boost::any&     substate,
    GraphEvalState& graph_eval_state
) const
{
    // nop
}

bool Base::transform(
    node_p             me,
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    Environment        environment,
    NodeReporter       reporter
)
{
    return false;
}

bool Base::prepare(
    MemoryManager      mm,
    const value_vec_t& static_args,
    Environment        environment,
    NodeReporter       reporter
)
{
    return true;
}

Simple::Simple(
    size_t num_static_args,
    size_t num_dynamic_args
) :
    Base(num_static_args, num_dynamic_args)
{
    // nop
}

void Simple::eval(
    MemoryManager   mm,
    const node_cp&  me,
    boost::any&     substate,
    GraphEvalState& ges,
    EvalContext     context
) const
{
    size_t n = me->children().size();
    assert(n == num_dynamic_args() + num_static_args());
    value_vec_t args(num_dynamic_args());

    size_t i = 0;
    BOOST_FOREACH(const node_p& child, me->children()) {
        NodeEvalState& child_nes = ges.final(child.get(), context);
        if (i >= num_static_args()) {
            if (! child_nes.is_finished()) {
                return;
            }
            args[i] = child_nes.value();
        }
        ++i;
    }

    NodeEvalState& my_state = ges.node_eval_state(me.get(), context);
    my_state.finish(eval_simple(mm, args));
}

Constant::Constant(Value value) :
    Simple(0, 0),
    m_value(value)
{
    // nop
}

Value Constant::eval_simple(MemoryManager, const value_vec_t&) const
{
    return m_value;
}

Primary::Primary(
    size_t num_static_args,
    size_t num_dynamic_args
) :
    Base(num_static_args, num_dynamic_args)
{
    // nop
}

void Primary::eval(
    MemoryManager   mm,
    const node_cp&  me,
    boost::any&     substate,
    GraphEvalState& ges,
    EvalContext     context
) const
{
    value_vec_t values;
    size_t i = 0;
    size_t n = me->children().size();
    const NodeEvalState* primary_state = NULL;
    BOOST_FOREACH(const node_p& child, me->children()) {
        NodeEvalState& child_nes = ges.final(child.get(), context);
        if (i == n - 1) {
            // Primary argument.
            primary_state = &child_nes;
        }
        else if (i >= num_static_args()) {
            // Dynamic argument.
            if (! child_nes.is_finished()) {
                return;
            }
            values.push_back(child_nes.value());
        }
        ++i;
    }
    assert(primary_state);
    NodeEvalState& my_state = ges.node_eval_state(me.get(), context);

    eval_primary(mm, me, substate, my_state, values, *primary_state);
}

namespace {

struct each_state_t
{
    each_state_t() : initialized(false) {}
    bool initialized;
    ConstList<Value>::const_iterator last_subvalue;
    boost::any subsubstate;
};
typedef boost::shared_ptr<each_state_t> each_state_p;

} // Anonymous


Each::Each(
    size_t num_static_args,
    size_t num_dynamic_args
) :
    Primary(num_static_args, num_dynamic_args)
{
    // nop
}

void Each::eval_initialize(
    MemoryManager   mm,
    const node_cp&  me,
    boost::any&     substate,
    GraphEvalState& graph_eval_state
) const
{
    each_state_p each_state(new each_state_t());
    substate = each_state;
    eval_initialize_each(mm, me, each_state->subsubstate);
}

void Each::eval_primary(
    MemoryManager        mm,
    const node_cp&       me,
    boost::any&          substate,
    NodeEvalState&       my_state,
    const value_vec_t&   secondary_args,
    const NodeEvalState& primary_arg
) const
{
    each_state_t& each_state =
        *boost::any_cast<each_state_p>(substate);

    Value primary_value = primary_arg.value();
    if (primary_value.is_null()) {
        if (primary_arg.is_finished()) {
            my_state.finish(primary_value);
        }
        return;
    }
    if (primary_value.type() != Value::LIST) {
        ready(
            mm,
            me,
            my_state,
            secondary_args,
            each_state.subsubstate,
            primary_value
        );
        eval_each(
            mm,
            my_state,
            secondary_args,
            each_state.subsubstate,
            primary_value,
            primary_value
        );
        if (! my_state.is_finished()) {
            my_state.finish();
        }
    }
    else {
        ConstList<Value> primary_values = primary_value.as_list();

        ready(
            mm,
            me,
            my_state,
            secondary_args,
            each_state.subsubstate,
            primary_value
        );

        if (primary_values.empty()) {
            if (primary_arg.is_finished() && ! my_state.is_finished()) {
                my_state.finish();
            }
            return;
        }
        if (my_state.is_finished()) {
            return;
        }

        ConstList<Value>::const_iterator subvalue_iter;
        if (! each_state.initialized) {
            each_state.initialized = true;
            subvalue_iter = primary_values.begin();
        }
        else {
            subvalue_iter = each_state.last_subvalue;
            ++subvalue_iter;
        }

        while (subvalue_iter != primary_values.end()) {
            eval_each(
                mm,
                my_state,
                secondary_args,
                each_state.subsubstate,
                primary_value,
                *subvalue_iter
            );
            if (my_state.is_finished()) {
                return;
            }
            each_state.last_subvalue = subvalue_iter;
            ++subvalue_iter;
        }

        if (primary_arg.is_finished()) {
            if (! my_state.is_finished()) {
                my_state.finish();
            }
        }
    }
}

void Each::eval_initialize_each(
    MemoryManager  mm,
    const node_cp& me,
    boost::any&    map_state
) const
{
    // nop
}

void Each::ready(
    MemoryManager      mm,
    const node_cp&     me,
    NodeEvalState&     my_state,
    const value_vec_t& secondary_args,
    boost::any&        each_state,
    Value              primary_value
) const
{
    // nop
}

Map::Map(
    size_t num_static_args,
    size_t num_dynamic_args
) :
    Each(num_static_args, num_dynamic_args)
{
    // nop
}

void Map::eval_initialize_each(
    MemoryManager  mm,
    const node_cp& me,
    boost::any&    each_state
) const
{
    eval_initialize_map(mm, me, each_state);
}

void Map::ready(
    MemoryManager      mm,
    const node_cp&     me,
    NodeEvalState&     my_state,
    const value_vec_t& secondary_args,
    boost::any&        each_state,
    Value              primary_value
) const
{
    if (primary_value.type() == Value::LIST) {
        my_state.setup_local_list(
            mm,
            primary_value.name(), primary_value.name_length()
        );
    }
}

void Map::eval_each(
    MemoryManager      mm,
    NodeEvalState&     my_state,
    const value_vec_t& secondary_args,
    boost::any&        each_state,
    Value              primary_value,
    Value              subvalue
) const
{
    if (primary_value.type() != Value::LIST) {
        assert(primary_value == subvalue);
        my_state.finish(
            eval_map(
                mm,
                secondary_args,
                each_state,
                subvalue
            )
        );
    }
    else {
        my_state.append_to_list(
            eval_map(
                mm,
                secondary_args,
                each_state,
                subvalue
            )
        );
    }
}

void Map::eval_initialize_map(
    MemoryManager  mm,
    const node_cp& me,
    boost::any&    map_state
) const
{
    // nop
}

Filter::Filter(
    size_t num_static_args,
    size_t num_dynamic_args
) :
    Each(num_static_args, num_dynamic_args)
{
    // nop
}

void Filter::eval_initialize_each(
    MemoryManager  mm,
    const node_cp& me,
    boost::any&    filter_state
) const
{
    eval_initialize_filter(mm, me, filter_state);
}

void Filter::ready(
    MemoryManager      mm,
    const node_cp&     me,
    NodeEvalState&     my_state,
    const value_vec_t& secondary_args,
    boost::any&        filter_state,
    Value              primary_value
) const
{
    if (primary_value.type() == Value::LIST) {
        my_state.setup_local_list(
            mm,
            primary_value.name(), primary_value.name_length()
        );
    }
}

void Filter::eval_each(
    MemoryManager      mm,
    NodeEvalState&     my_state,
    const value_vec_t& secondary_args,
    boost::any&        filter_state,
    Value              primary_value,
    Value              subvalue
) const
{
    if (primary_value.type() != Value::LIST) {
        assert(primary_value == subvalue);
        bool early_finish = false;
        bool pass = eval_filter(
            mm,
            secondary_args,
            filter_state,
            early_finish,
            subvalue
        );
        if (pass) {
            my_state.finish(subvalue);
        }
        else {
            my_state.finish();
        }
    }
    else {
        bool early_finish = false;
        bool pass = eval_filter(
            mm,
            secondary_args,
            filter_state,
            early_finish,
            subvalue
        );
        if (pass) {
            my_state.append_to_list(subvalue);
        }
        if (early_finish) {
            my_state.finish();
        }
    }
}

void Filter::eval_initialize_filter(
    MemoryManager  mm,
    const node_cp& me,
    boost::any&    filter_state
) const
{
    // nop
}


Selector::Selector(
    size_t num_static_args,
    size_t num_dynamic_args
) :
    Each(num_static_args, num_dynamic_args)
{
    // nop
}

void Selector::eval_initialize_each(
    MemoryManager  mm,
    const node_cp& me,
    boost::any&    selector_state
) const
{
    eval_initialize_selector(mm, me, selector_state);
}

void Selector::eval_each(
    MemoryManager      mm,
    NodeEvalState&     my_state,
    const value_vec_t& secondary_args,
    boost::any&        selector_state,
    Value              primary_value,
    Value              subvalue
) const
{
    bool pass = eval_selector(
        mm,
        secondary_args,
        selector_state,
        subvalue
    );
    if (pass) {
        my_state.finish(subvalue);
    }
}

void Selector::eval_initialize_selector(
    MemoryManager  mm,
    const node_cp& me,
    boost::any&    selector_state
) const
{
    // nop
}

} // Functional
} // Predicate
} // IronBee
