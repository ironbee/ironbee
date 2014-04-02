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
 * @brief Predicate --- Standard List Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <predicate/standard_list.hpp>

#include <predicate/call_factory.hpp>
#include <predicate/call_helpers.hpp>
#include <predicate/functional.hpp>
#include <predicate/merge_graph.hpp>
#include <predicate/validate.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

namespace {

static ScopedMemoryPoolLite s_mpl;
static const node_p c_false(new Literal());
static const node_p c_empty(new Literal(
    Value::alias_list(s_mpl, List<Value>::create(s_mpl))
));

/**
 * Construct a named value from a name (string) and value.
 **/
class SetName :
    public Functional::Map
{
public:
    //! Constructor.
    SetName() :
        Functional::Map(0, 2)
    {
        // nop
    }

protected:
    //! See Functional::Base::validate_argument().
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            Validate::value_is_type(v, Value::STRING, reporter); 
        }
    }
    
    //! See Functional::Map::eval_map()
    Value eval_map(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    map_state,
        Value                          subvalue
    ) const
    {
        ConstByteString name = secondary_args[0].as_string();
        return subvalue.dup(mm, name.const_data(), name.length());
    }
};

/**
 * Push the name of a list value to its children.
 **/
class PushName :
    public Functional::Map
{
public:
    //! Constructor.
    PushName() :
        Functional::Map(0, 1)
    { 
        // nop
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
        if (! subvalue || subvalue.type() != Value::LIST) {
            return subvalue;
        }
        else {
            List<Value> new_list = List<Value>::create(mm);
            BOOST_FOREACH(const Value& subsubvalue, subvalue.as_list()) {
                new_list.push_back(
                    subsubvalue.dup(
                        mm, 
                        subvalue.name(), subvalue.name_length()
                    )
                );
            }
            return Value::alias_list(
                mm, 
                subvalue.name(), subvalue.name_length(), 
                new_list
            );
        }
    }
};

/**
 * Concatenate values of children.
 **/
class Cat :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    /**
     * See Node::transform().
     *
     * Will replace self with ''.
     **/
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        NodeReporter       reporter
    );

    //! See Node::eval_initialize()
    virtual void eval_initialize(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * List of children.
 **/
class List :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    /**
     * See Node::transform().
     *
     * Will replace self with ''.
     **/
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        NodeReporter       reporter
    );

    //! See Node::eval_initialize()
    virtual void eval_initialize(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * First element of child.
 **/
class First :
    public Functional::Selector
{
public:
    //! Constructor.
    First() :
        Functional::Selector(0, 1)
    {
        // nop
    }
    
protected:
    //! See Functional::Selector::eval_selector().
    bool eval_selector(
        MemoryManager      mm, 
        const Functional::value_vec_t& secondary_args, 
        boost::any&        selector_state,
        Value              subvalue
    ) const
    {
        return true;
    }
};

/**
 * All but first element of child.
 **/
class Rest :
    public Functional::Filter
{
public:
    //! Constructor.
    Rest() :
        Functional::Filter(0, 1)
    {
        // nop
    }
    
protected:
    // See Functional::Filter::eval_initialize_filter().
    void eval_initialize_filter(
        MemoryManager  mm,
        const node_cp& me,
        boost::any&    filter_state
    ) const
    {
        filter_state = bool(false);
    }
    
    //! See Functional::Filter::eval_filter().
    bool eval_filter(
        MemoryManager                  mm, 
        const Functional::value_vec_t& secondary_args, 
        boost::any&                    filter_state,
        bool&                          early_finish,
        Value                          subvalue
    ) const
    {
        if (boost::any_cast<bool>(filter_state)) {
            return true;
        }   
        else {
            filter_state = bool(true);
            return false;
        }
    }
};

/**
 * Nth element of child.
 **/
class Nth :
    public Functional::Selector
{
public:
    //! Constructor.
    Nth() :
        Functional::Selector(0, 2)
    {
        // nop
    }
    
protected:
    void eval_initialize_selector(
        MemoryManager  mm,
        const node_cp& me,
        boost::any&    selector_state
    ) const
    {
        selector_state = int64_t(0);
    }
    
    //! See Functional::Selector::eval_selector().
    bool eval_selector(
        MemoryManager                  mm,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    selector_state,
        Value                          subvalue
    ) const
    {
        int64_t n = boost::any_cast<int64_t>(selector_state);
        ++n;
        if (secondary_args.front().as_number() == n) {
            return true;
        }
        
        selector_state = n;
        return false;
    }
};


/**
 * Flatten list of lists into a list.
 **/
class Flatten :
    public Functional::Each
{
public:
    //! Constructor.
    Flatten() :
        Functional::Each(0, 1)
    {
        // nop
    }
    
protected:
    //! See Each::ready().
    void ready(
        MemoryManager                  mm,
        const node_cp&                 me,
        NodeEvalState&                 my_state,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    each_state,
        Value                          primary_value
    ) const
    {
        if (primary_value.type() == Value::LIST) {
            my_state.setup_local_list(
                mm,
                primary_value.name(), primary_value.name_length()
            );
        }
    }
    
    //! See Each::eval_each().
    void eval_each(
        MemoryManager                  mm,
        NodeEvalState&                 my_state,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    each_state,
        Value                          primary_value,
        Value                          subvalue
    ) const
    {
        if (primary_value.type() == Value::LIST) {
            if (subvalue.type() == Value::LIST) {
                BOOST_FOREACH(const Value& v, subvalue.as_list()) {
                    my_state.append_to_list(v);
                }
            }
            else {
                my_state.append_to_list(subvalue);
            }
        }
        else {
            assert(primary_value == subvalue);
            my_state.finish(subvalue);
        }
    }
};

/**
 * Focus on one value from each subvalue.
 **/
class Focus :
    public Functional::Each
{
public:
    //! Constructor.
    Focus() :
        Functional::Each(0, 2)
    {
        // nop
    }
    
    //! See Functional::Base::validate_argument().
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const
    {
        if (n == 0) {
            Validate::value_is_type(v, Value::STRING, reporter); 
        }
    }
    
protected:
    //! See Each::ready().
    void ready(
        MemoryManager                  mm,
        const node_cp&                 me,
        NodeEvalState&                 my_state,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    each_state,
        Value                          primary_value
    ) const
    {
        my_state.setup_local_list(
            mm,
            primary_value.name(), primary_value.name_length()
        );
    }
    
    //! See Each::eval_each().
    void eval_each(
        MemoryManager                  mm,
        NodeEvalState&                 my_state,
        const Functional::value_vec_t& secondary_args,
        boost::any&                    each_state,
        Value                          primary_value,
        Value                          subvalue
    ) const
    {
        if (subvalue.type() != Value::LIST) {
            return;
        }
        
        ConstByteString n = secondary_args.front().as_string();
        
        BOOST_FOREACH(const Value& v, subvalue.as_list()) {
            if (
                v.name_length() == n.length() && 
                equal(
                    n.const_data(), n.const_data() + n.length(),
                    v.name()
                )
            ) {
                my_state.append_to_list(
                    v.dup(mm, subvalue.name(), subvalue.name_length())
                );
                return;
            }
        }
    }
};

/**
 * Implementation details of Cat.
 *
 * To implement Cat, we track two iterators (per thread):
 * - last_unfinished is the child we last processed.  That is, the last time
 *   calculate was run, we added all children of last_unfinished but it was
 *   unfinished so we did not advance to the next child.
 * - last_value_added is the last value of last_unfinished.  That is, the
 *   last time calculate was run, we added all children of last_unfinished,
 *   the last of which was last_value_added.
 *
 * Thus, our task on calculate is to add any remaining children of
 * last_unfinished and check if it is now finished.  If it is, we go on to
 * add any subsequent finished children.  If that consumes all children, we
 * are done and can finish.  Otherwise, we have arrived at a new leftmost
 * unfinished child.  We must add all of its current children, and then
 * wait for the next calculate.
 *
 * This task is handled by add_from_current() and add_until_next_unfinished().
 *
 * This class is a friend of Cat and all routines take a `me` argument being
 * the Cat instance they are working on behalf of.
 **/
class cat_impl_t
{
public:
    /**
     * Constructor.
     *
     * Set last unfinished child to be first child.
     **/
    explicit
    cat_impl_t(const Cat& me)
    {
        m_last_unfinished = me.children().begin();
        m_last_value_added_good = false;
    }

    /**
     * Calculate.
     *
     * After this, last unfinished child and last value added will be updated.
     **/
    void eval_calculate(
        const Cat&      me,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    )
    {
        // Add any new children from last_unfinished.
        add_from_current(me, graph_eval_state, context);
        // If last_unfinished is still unfinished, nothing more to do.
        if (! graph_eval_state.is_finished((*m_last_unfinished)->index())) {
            return;
        }

        // Need to find new leftmost unfinished child.  Do so, adding any
        // values from finished children along the way.
        add_until_next_unfinished(me, graph_eval_state, context);

        // If no new leftmost unfinished child, all done.  Finish.
        if (m_last_unfinished == me.children().end()) {
            graph_eval_state[me.index()].finish();
        }
        // Otherwise, need to add children from the new last_unfinished.
        else {
            m_last_value_added_good = false;
            add_from_current(me, graph_eval_state, context);
        }
    }

private:
    /**
     * Add all values from last unfinished child after last value added.
     *
     * Updates last value added.
     **/
    void add_from_current(
        const Cat&      me,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    )
    {
        Value value = graph_eval_state.eval(
            *m_last_unfinished,
            context
        );
            
        if (
            ! value && 
            ! graph_eval_state.is_finished((*m_last_unfinished)->index())
        ) {
            return;
        }
        
        if (value) {
            if (value.type() == Value::LIST) {
                const ConstList<Value> values = value.as_list();
                assert(! values.empty());
                if (! m_last_value_added_good) {
                    graph_eval_state[me.index()].append_to_list(
                        values.front()
                    );
                    m_last_value_added = values.begin();
                    m_last_value_added_good = true;
                }
                IronBee::List<Value>::const_iterator n = m_last_value_added;
                IronBee::List<Value>::const_iterator end = values.end();
                for (;;) {
                    ++n;
                    if (n == end) {
                        break;
                    }
                    graph_eval_state[me.index()].append_to_list(*n);
                    m_last_value_added = n;
                }
            }
            else {
                assert(
                    graph_eval_state.is_finished(
                        (*m_last_unfinished)->index()
                    )
                );
                graph_eval_state[me.index()].append_to_list(value);
            }
        }
    }

    /**
     * Advanced last unfinished to new leftmost unfinished child.
     *
     * Adds values of finished children along the way.
     * If no unfinished children, last unfinished will end up as
     * `me.children().end()`.
     **/
    void add_until_next_unfinished(
        const Cat&      me,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    )
    {
        assert(graph_eval_state.is_finished((*m_last_unfinished)->index()));
        NodeEvalState& my_state = graph_eval_state[me.index()];
        for (
            ++m_last_unfinished;
            m_last_unfinished != me.children().end();
            ++m_last_unfinished
        ) {
            if (
                ! graph_eval_state.is_finished((*m_last_unfinished)->index())
            ) {
                break;
            }
            Value v = graph_eval_state.eval(*m_last_unfinished, context);
            if (v) {
                if (v.type() == Value::LIST) {
                    BOOST_FOREACH(Value v, v.as_list()) {
                        my_state.append_to_list(v);
                    }
                }
                else {
                    my_state.append_to_list(v);
                }
            }
        }
    }

    //! Last unfinished child processed.
    node_list_t::const_iterator m_last_unfinished;

    //! Is m_last_value_added meaningful?
    bool m_last_value_added_good;
    
    /**
     * Last value added from @ref m_last_unfinished.
     *
     * A singular value means no children of @ref m_last_unfinished have
     * been added.
     **/
    IronBee::List<Value>::const_iterator m_last_value_added;
};

string Cat::name() const
{
    return "cat";
}

bool Cat::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    bool result = false;

    // Remove false children.
    {
        node_list_t to_remove;
        BOOST_FOREACH(const node_p& child, children()) {
            if (child->is_literal() && ! literal_value(child)) {
                to_remove.push_back(child);
            }
        }
        BOOST_FOREACH(const node_p& child, to_remove) {
            merge_graph.remove(me, child);
        }

        if (! to_remove.empty()) {
            result = true;
        }
    }

    // Become child if only one child and child is list literal..
    if (children().size() == 1) {
        node_p replacement = children().front();
        if (
            replacement->is_literal() && 
            literal_value(replacement).type() == Value::LIST
        ) {
            merge_graph.replace(me, replacement);
            return true;            
        }
    }

    // Become [] if no children.
    if (children().size() == 0) {
        node_p replacement(c_empty);
        merge_graph.replace(me, replacement);
        return true;
    }
    
    // Become Literal if all children are literals.
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
            v = v.dup(*mpl, v.name(), v.name_length());
            if (v) {
                if (v.type() == Value::LIST) {
                    copy(
                        v.as_list().begin(), v.as_list().end(), 
                        back_inserter(my_value)
                    );
                }
                else {
                    my_value.push_back(v);
                }
            }
        }
        
        if (replace) {
            node_p replacement(new Literal(mpl, 
                Value::alias_list(*mpl, my_value)
            ));
            merge_graph.replace(me, replacement);
            return true;
        }
    }

    return result;
}

void Cat::eval_initialize(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& node_eval_state = graph_eval_state[index()];
    node_eval_state.setup_local_list(context.memory_manager());
    node_eval_state.state() =
        boost::shared_ptr<cat_impl_t>(new cat_impl_t(*this));
}

void Cat::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    boost::any_cast<boost::shared_ptr<cat_impl_t> >(
        graph_eval_state[index()].state()
    )->eval_calculate(*this, graph_eval_state, context);
}

string List::name() const
{
    return "list";
}

bool List::transform(
    MergeGraph&        merge_graph,
    const CallFactory& call_factory,
    NodeReporter       reporter
)
{
    node_p me = shared_from_this();
    
    if (children().size() == 0) {
        node_p replacement(c_empty);
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

void List::eval_initialize(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];
    node_list_t::const_iterator last_unfinished = children().begin();
    my_state.state() = last_unfinished;
    my_state.setup_local_list(context.memory_manager());
}

void List::eval_calculate(
    GraphEvalState& graph_eval_state,
    EvalContext     context
) const
{
    NodeEvalState& my_state = graph_eval_state[index()];

    node_list_t::const_iterator last_unfinished = 
        boost::any_cast<node_list_t::const_iterator>(my_state.state());
    while (last_unfinished != children().end()) {
        size_t index = (*last_unfinished)->index();
        Value v = graph_eval_state.eval(*last_unfinished, context);
        if (! graph_eval_state.is_finished(index)) {
            break;
        }
        
        my_state.append_to_list(v);
        ++last_unfinished;
    }
    
    if (last_unfinished == children().end()) {
        my_state.finish();
    }
    
    my_state.state() = last_unfinished;
}

} // Anonymous

void load_list(CallFactory& to)
{
    to
        .add("setName", Functional::generate<SetName>)
        .add("pushName", Functional::generate<PushName>)
        .add<Cat>()
        .add<List>()
        .add("first", Functional::generate<First>)
        .add("rest", Functional::generate<Rest>)
        .add("nth", Functional::generate<Nth>)
        .add("flatten", Functional::generate<Flatten>)
        .add("focus", Functional::generate<Focus>)
        ;
}

} // Standard
} // Predicate
} // IronBee
