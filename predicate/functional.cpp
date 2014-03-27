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

#include <predicate/functional.hpp>

#include <predicate/call_helpers.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/reporter.hpp>
#include <predicate/validate.hpp>

#include <boost/format.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Functional {

namespace Impl {

Call::Call(const string& name, const base_p& base) :
    m_base(base),
    m_name(name)
{
    // nop
}

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

void prepare_call(
    Call&         me,
    Base&         base,
    MemoryManager mm,
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

    base.prepare(mm, static_args, reporter);
}

} // Anonymous

bool Call::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
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
    BOOST_FOREACH(const node_p& arg, children()) {
        // None of this would work if we had non-literal args.
        assert(arg->is_literal()); 
        ges.initialize(arg, EvalContext());
    }
    boost::shared_ptr<ScopedMemoryPoolLite> mpl(new ScopedMemoryPoolLite());
    boost::any substate;
    node_p me = shared_from_this();
    prepare_call(*this, *m_base, *mpl, reporter);

    // Construct a fake EvalContext that only contains our memory manager.
    ib_tx_t ib_eval_context;
    ib_eval_context.mm = MemoryManager(*mpl).ib();
    EvalContext eval_context(&ib_eval_context);
    ges.initialize(me, eval_context);
    ges.eval(me, eval_context);
    const NodeEvalState& my_state = ges.final(0);

    if (my_state.is_finished()) {
        // Here we pass the mpl shared pointer on to the new Literal node.
        // If this statement is not run, the mpl and any work is 
        // discarded at the end of this function.
        node_p replacement(new Literal(mpl, my_state.value()));

        merge_graph.replace(shared_from_this(), replacement);

        return true;
    }
    else {
        return m_base->transform(
            shared_from_this(), 
            merge_graph, 
            call_factory, 
            reporter
        );
    }
}

void Call::pre_eval(Environment environment, NodeReporter reporter)
{
    prepare_call(
        *this,
        *m_base, 
        environment.main_memory_mm(), 
        reporter
    );
}

namespace {

typedef pair<node_p, size_t> arg_with_index_t;
typedef list<arg_with_index_t> arg_list_t;

struct call_state_t {
    arg_list_t unfinished;
    boost::any substate;
};
typedef boost::shared_ptr<call_state_t> call_state_p;

void validate_args(
    arg_list_t&           args,
    const Base&           base,
    const GraphEvalState& graph_eval_state
)
{
    arg_list_t::iterator iter = args.begin();
    for (;;) {
        if (graph_eval_state.is_finished(iter->first->index())) {
            Reporter reporter;
            NodeReporter node_reporter(reporter, iter->first, false);
            base.validate_argument(
                iter->second,
                graph_eval_state.value(iter->first->index()),
                node_reporter
            );
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
                            iter->first->to_s()
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
    call_state_p call_state(new call_state_t);

    Predicate::Call::eval_initialize(graph_eval_state, context);

    node_list_t::const_iterator iter;
    size_t i;
    for (
        iter = children().begin(), i = 0;
        iter != children().end();
        ++i
    ) {
        if (! (*iter)->is_literal()) {
            call_state->unfinished.push_back(make_pair(*iter, i));
        }
    }

    m_base->eval_initialize(call_state->substate);

    graph_eval_state[index()].state() = call_state;
}

void Call::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];
    call_state_p call_state = boost::any_cast<call_state_p>(my_state.state());

    validate_args(call_state->unfinished, *m_base, graph_eval_state);
    
    m_base->eval(
        context.memory_manager(),
        shared_from_this(),
        call_state->substate,
        graph_eval_state
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

void Base::eval_initialize(boost::any& substate) const
{
    // nop
}

bool Base::transform(
    node_p             me,
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    return false;
}

void Base::prepare(
    MemoryManager      mm,
    const value_vec_t& static_args,
    NodeReporter       reporter
)
{
	// nop
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
    GraphEvalState& ges
) const
{
    size_t n = me->children().size();
    assert(n == num_dynamic_args() + num_static_args());
    value_vec_t args(num_dynamic_args());

    size_t i = 0;
    BOOST_FOREACH(const node_p& child, me->children()) {
        if (i >= num_static_args()) {
            if (! ges.is_finished(child->index())) {
                return;
            }
            args.push_back(ges.value(child->index()));
        }
        ++i;
    }

    NodeEvalState& my_state = ges[me->index()];
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
    GraphEvalState& ges
) const
{
    value_vec_t values;
    size_t i = 0;
    size_t n = me->children().size();
    const NodeEvalState* primary_state = NULL;
    BOOST_FOREACH(const node_p& child, me->children()) {
        if (i == n - 1) {
            // Primary argument.
            primary_state = &ges.final(child->index());
        }
        else if (i >= num_static_args()) {
            // Dynamic argument.
            if (! ges.is_finished(child->index())) {
                return;
            }
            values.push_back(ges.value(child->index()));
        }
        ++i;
    }
    assert(primary_state);
    NodeEvalState& my_state = ges[me->index()];

    eval_primary(mm, me, substate, my_state, values, *primary_state);
}

namespace {

struct map_filter_state_t
{
    map_filter_state_t() : initialized(false) {}
    bool initialized;
    ConstList<Value>::const_iterator last_added_subvalue;
};
typedef boost::shared_ptr<map_filter_state_t> map_filter_state_p;

}

Map::Map(
    size_t num_static_args,
    size_t num_dynamic_args
) :
    Primary(num_static_args, num_dynamic_args)
{
    // nop
}

void Map::eval_initialize(boost::any& substate) const
{
    substate = map_filter_state_p(new map_filter_state_t());
}

void Map::eval_primary(
    MemoryManager        mm,
    const node_cp&       me,
    boost::any&          substate,
    NodeEvalState&       my_state,
    const value_vec_t&   secondary_args,
    const NodeEvalState& primary_arg
) const
{
    Value primary_value = primary_arg.value();
    if (! primary_value) {
        if (primary_arg.is_finished()) {
            my_state.finish();
        }
        return;
    }
    if (primary_value.type() != Value::LIST) {
        Value my_value = eval_map(mm, secondary_args, primary_value);
        my_state.finish(my_value);
    }
    else {
        Value primary_value = primary_arg.value();
        ConstList<Value> primary_values = primary_value.as_list();
        if (primary_values.empty()) {
            return;
        }

        map_filter_state_t& map_state =
            *boost::any_cast<map_filter_state_p>(substate);
        ConstList<Value>::const_iterator to_add;
        if (! map_state.initialized) {
            my_state.setup_local_list(
                mm,
                primary_value.name(), primary_value.name_length()
            );
            map_state.initialized = true;
            to_add = primary_values.begin();
        }
        else {
            to_add = map_state.last_added_subvalue;
            ++to_add;
        }

        while (to_add != primary_values.end()) {
            my_state.append_to_list(
                eval_map(mm, secondary_args, *to_add)
            );
            map_state.last_added_subvalue = to_add;
            ++to_add;
        }

        if (primary_arg.is_finished()) {
            my_state.finish();
        }
    }
}

Filter::Filter(
    size_t num_static_args,
    size_t num_dynamic_args
) :
    Primary(num_static_args, num_dynamic_args)
{
    // nop
}

void Filter::eval_initialize(boost::any& substate) const
{
    substate = map_filter_state_p(new map_filter_state_t());
}

void Filter::eval_primary(
    MemoryManager        mm,
    const node_cp&       me,
    boost::any&          substate,
    NodeEvalState&       my_state,
    const value_vec_t&   secondary_args,
    const NodeEvalState& primary_arg
) const
{
    Value primary_value = primary_arg.value();
    if (! primary_value) {
        if (primary_arg.is_finished()) {
            my_state.finish();
        }
        return;
    }
    if (primary_value.type() != Value::LIST) {
        bool add = eval_filter(mm, secondary_args, primary_value);
        if (add) {
            my_state.finish(primary_value);
        }
        else {
            my_state.finish();
        }
    }
    else {
        Value primary_value = primary_arg.value();
        ConstList<Value> primary_values = primary_value.as_list();
        if (primary_values.empty()) {
            return;
        }

        map_filter_state_t& filter_state =
            *boost::any_cast<map_filter_state_p>(substate);
        ConstList<Value>::const_iterator to_add;
        if (! filter_state.initialized) {
            my_state.setup_local_list(
                mm,
                primary_value.name(), primary_value.name_length()
            );
            filter_state.initialized = true;
            to_add = primary_values.begin();
        }
        else {
            to_add = filter_state.last_added_subvalue;
            ++to_add;
        }

        while (to_add != primary_values.end()) {
            if (eval_filter(mm, secondary_args, *to_add)) {
                my_state.append_to_list(*to_add);
            }
            filter_state.last_added_subvalue = to_add;
            ++to_add;
        }

        if (primary_arg.is_finished()) {
            my_state.finish();
        }
    }
}

} // Functional
} // Predicate
} // IronBee
