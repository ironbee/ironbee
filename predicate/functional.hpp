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
 * @brief Predicate --- Functional
 *
 * Defines a framework writing Calls in terms of functionals.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__FUNCTIONAL__
#define __PREDICATE__FUNCTIONAL__

#include <predicate/dag.hpp>
#include <predicate/eval.hpp>

namespace IronBee {
namespace Predicate {
    
/**
 * Framework for writing Predicate Calls inspired by functionals.
 *
 * This namespace contains a hierarchy of classes useful for writing 
 * Predicate Calls.  It takes a significantly different direction than the
 * Predicate::Call class, and, to accomplish this change in API, makes use
 * of the delegate pattern.  So, to write a new Call, subclass one of the
 * classes in this namespace and then use the generate() function to register
 * with the call factory:
 *
 * @code
 * class MyNewCall : public Functional::Base { ... }
 *
 * void load(CallFactory& to)
 * {
 *     to.add("myNewCall", Functional::generate<MyNewCall>)
 * }
 * @endcode
 *
 * @note You specify the name on registration with the CallFactory, rather
 * than in the class definition.
 *
 * Functional is oriented at reducing writing a new Call to writing a 
 * function.  It doesn't achieve this, but it does move considerably in that
 * direction.  An advantage of this approach is the ability to automatically
 * transform into a literal if all arguments are literals.  The disadvantage
 * is flexibility: Only certain styles of Calls can be written using
 * functionals.  In particular, there is no support for variable number of
 * arguments.
 *
 * The class hierarchy is:
 * 
 * - Functional::Base -- The root of the hierarchy.  Divides arguments into
 *   some number of static arguments followed by some number of dynamic 
 *   arguments.  Requires that the static arguments be literals.  Calls
 *   Functional::Base::prepare() with static arguments are pre-eval time and
 *   Functional::Base::eval() at post-eval time.  Provides for per-argument
 *   validation as soon as the argument can be validated, including at 
 *   runtime.
 * - Functional::Simple -- Subclass of Base for calls that do nothing until
 *   all arguments are finished.  Calls a much simplfiied eval_simple(), 
 *   providing the *values* of the dynamic arguments.
 * - Functional::Constant -- Subclass of Simple for calls that have a constant
 *   value.  Subclasses simply construct and pass the value to the parent
 *   constructor.
 * - Functional::Primary -- Subclass of Base for calls that have secondary
 *   arguments and a single, final, primary argument.  Waits for all secondary
 *   arguments to be finished and then calls eval_primary() with the values
 *   of the secondary argument until finished.
 * - Functional::Map -- Subclass of Primary for calls that are maps.  Calls
 *   eval_map() for each subvalue of the primary argument and adds the 
 *   returned value to its own value.  Also handles if the primary argument
 *   is not a list.
 * - Functional::Filter -- Subclass of Primary for calls that are filters.
 *   Calls eval_filter() for each subvalue of the primary argument and add the
 *   subvalue iff eval_filter() returned true.  Also handles if the primary
 *   argument is not a list.
 **/
namespace Functional {

class Base;
//! Pointer to Base.
typedef boost::shared_ptr<Base> base_p;

/// @cond Impl
namespace Impl {

/**
 * Call making use of a Base subclass as a delegate.
 *
 * This class should not be used directly.  It is part of the implementation
 * of generate().
 **/
class Call :
    public Predicate::Call
{
public:
    /**
     * Constructor.
     * 
     * @param[in] name Name of function implementing.
     * @param[in] base Base delegate defining operation.
     **/
    Call(const std::string& name, const base_p& base);

    //! See Node::name()
    virtual
    std::string name() const
    {
        return m_name;
    }

    /**
     * Do pre-transform validations.
     *
     * See Node::pre_transform().
     *
     * Checks for the right number of arguments and validates any literal
     * arguments.
     *
     * @sa post_transform()
     **/
    virtual
    void pre_transform(NodeReporter reporter) const;

    /**
     * Do post-transform validations.
     *
     * See Node::post_transform().
     *
     * Checks that all static arguments are literals and validates those
     * and any other literal children.
     *
     * @sa pre_transform()
     **/
    virtual
    void post_transform(NodeReporter reporter) const;

    /**
     * Transform.
     *
     * See Node::transform().
     *
     * If all arguments are literal, evaluates and transforms into resulting
     * value.  Otherwise, gives delegate a chance to transform.
     **/
    virtual
    bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        NodeReporter       reporter
    );

    /**
     * Pre-eval.
     *
     * See Node::pre_eval().
     *
     * Calls Base::setup().
     **/
    virtual
    void pre_eval(
        Environment  environment,
        NodeReporter reporter
    );

    /**
     * Initialize for evaluation.
     *
     * See Node::eval_initialize().
     *
     * Calls Base::eval_initialize().
     **/
    virtual
    void eval_initialize(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

    /**
     * Evaluate node.
     *
     * See Node::eval_calculate().
     * 
     * Validates any children that have become finished since last call and
     * call Base::eval().
     **/
    virtual
    void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

private:
    //! Pointer to Base delegate.
    base_p m_base;
    //! Name.
    const std::string m_name;
};

} // Impl
/// @endcond

//! Vector of values.
typedef std::vector<Value> value_vec_t;

/**
 * Base of delegate hierarchy.
 *
 * To use, subclass and:
 *
 * 1. Call consturctor with the number of static and number of dynamic
 *    arguments.  Static arguments will be required to be literals by the end
 *    of transformation.
 * 2. Override validate_argument() to provide per-argument validations.
 * 3. Override transform() if you have custom transformations.  For example,
 *    if it is possible to transform in some cases even if some arguments are
 *    not literals.
 * 4. Override prepare() to do any preparations based on static arguments. 
 * 5. Override eval_initialize() if needed.
 * 6. Override eval().
 **/
class Base
{
public:
    //! Number of static arguments.
    size_t num_static_args() const
    {
        return m_num_static_args;
    }

    //! Number of dynamic arguments.
    size_t num_dynamic_args() const
    {
        return m_num_dynamic_args;
    }

    /**
     * Validate argument @a n with value @a v.
     *
     * Called for literal arguments at configuration time and for dynamic
     * arguments when they first finish.
     *
     * @param[in] n        Index of argument.  0 based.
     * @param[in] v        Value of argument.
     * @param[in] reporter Reporter to reporter issues with.
     **/
    virtual
    void validate_argument(
        int          n,
        Value        v,
        NodeReporter reporter
    ) const;

    /**
     * Prepare for evaluation.
     *
     * @param[out] substate Store any per-evaluation state you need here.
     **/
    virtual
    void eval_initialize(
        boost::any& substate
    ) const;

    /**
     * Evaluate.
     * 
     * @param[in] mm               Memory manager defining lifetime of any 
     *                             new values.
     * @param[in] me               Node being evaluated.
     * @param[in] substate         Substate provided by eval_initialize().
     * @param[in] graph_eval_state Graph evaluation state.
     **/
    virtual
    void eval(
        MemoryManager   mm,
        const node_cp&  me,
        boost::any&     substate,
        GraphEvalState& graph_eval_state
    ) const = 0;

    /**
     * Transform.
     *
     * Note: Base will automatically handle case that all arguments are 
     * literals by calling eval() and transforming into literal with resulting
     * value if node is finished.  In that case, this transform() will not be
     * callled.
     *
     * By default does nothing, reutrning false.
     *
     * @param[in] me           Node to transform.
     * @param[in] merge_graph  Merge graph to transform.
     * @param[in] call_factory Call factory to create new calls with.
     * @parma[in] reporter     Reporter to report issues to.
     **/
    virtual
    bool transform(
        node_p             me,
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        NodeReporter       reporter
    );

    /**
     * Prepare node.
     *
     * Calls at pre-eval to allow delegate to do any setup it can based on
     * the static arguments.
     *
     * Must be possible to call multiple times.  The most recent call should
     * be used to setup state using that @a mm.
     *
     * @param[in] mm          Memory manager that will outlive call.
     * @param[in] static_args Values of static arguments.       
     * @param[in] reporter    Reporter to report any issues to.
     **/
    virtual 
    void prepare(
        MemoryManager      mm,
        const value_vec_t& static_args,
        NodeReporter       reporter
    );
    
protected:
    /**
     * Constructor.
     *
     * @param[in] num_static_args  Number of static arguments.
     * @param[in] num_dynamic_args Number of dynamic arguments.
     **/
    Base(
        size_t num_static_args,
        size_t num_dynamic_args
    );

private:
    //! Number of static arguments.
    size_t m_num_static_args;
    //! Number of dynamic arguments.
    size_t m_num_dynamic_args;
};

/**
 * Delegate for Calls that want all arguments finished.
 *
 * Use is similar to Base, except, instead of overriding Base::eval(), 
 * override, eval_simple(), which is given the values of the dynamic 
 * arguments and returns the value of the function.
 **/
class Simple :
    public Base
{
public:
    //! See Base::eval().
    void eval(
        MemoryManager          mm,
        const node_cp&         me,
        boost::any&            substate,
        GraphEvalState& graph_eval_state
    ) const;

protected:
    //! Constructor.  See Base::Base().
    Simple(
        size_t num_static_args,
        size_t num_dynamic_args
    );

    /**
     * Evaluate simple function.
     *
     * Will not be called until all arguments are finished.
     *
     * @param[in] mm           Memory manager defining lifetime of new values.
     * @param[in] dynamic_args Values of dynamic arguments.
     * @return Value of this simple call.
     **/
    virtual
    Value eval_simple(
        MemoryManager      mm,
        const value_vec_t& dynamic_args
    ) const = 0;
};

/**
 * Delegate for Calls that have a constant result.
 *
 * To use, subclass and call Constant::Constant() with the value of the
 * call.
 **/
class Constant :
    public Simple
{
protected:
    /**
     * Constructor.
     *
     * @param[in] value Value of call.
     **/
    explicit
    Constant(Value value);

    //! See Simple::eval_simple().
    virtual
    Value eval_simple(MemoryManager, const value_vec_t&) const;

private:
    //! Value for eval_simple() to return.
    Value m_value;
};

/**
 * Delegate for Calls that are simple except for a primary argument.
 *
 * The primary argument is always last.  This delegate will wait for all 
 * secondary arguments (all dynamic arguments except the primary) to finish,
 * and then calls eval_primary().
 *
 * Considering using Map or Filter instead.
 *
 * See Base for discussion on use.
 **/
class Primary :
    public Base
{
public:
    //! See Base::eval().
    void eval(
        MemoryManager          mm,
        const node_cp&         me,
        boost::any&            substate,
        GraphEvalState& graph_eval_state
    ) const;

protected:
    /**
     * Constructor.
     *
     * See Base::Base.
     *
     * @param[in] num_static_args  Number of static arguments.
     * @param[in] num_dynamic_args Number of dynamic arguments, including
     *            primary argument.
     **/
    Primary(
        size_t num_static_args,
        size_t num_dynamic_args // including primary argument
    );

    /**
     * Evaluate primary call.
     *
     * Only called once all secondary arguments are finished.
     *
     * @param[in] mm             Memory manager defining lifetime of new 
     *                           values.
     * @param[in] me             Node being evaluated.
     * @param[in] substate       See Base::eval_initialize().
     * @param]in] my_state       Evaluate state of @a me.
     * @param[in] secondary_args Values of secondary arguments.
     * @param[in] primary_arg    Evaluation state of primary argument.
     **/
    virtual 
    void eval_primary(
        MemoryManager        mm,
        const node_cp&       me,
        boost::any&          substate,
        NodeEvalState&       my_state,
        const value_vec_t&   secondary_args,
        const NodeEvalState& primary_arg
    ) const = 0;
};

/**
 * Delegate for Calls that apply a subfunction to each of a list.
 *
 * If the primary argument is the empty list, result is the empty list.
 * If the primary argument is not a list, result is the subfunction applied
 * to the list.  If the primary argument is a list, result is a list of the
 * subfunction appied to each subvalue of the primary argument.
 *
 * For use, see Base and Primary, and override eval_sub().
 **/
class Map :
    public Primary
{
public:
    //! See Base::eval_initialize().
    void eval_initialize(boost::any& substate) const;

protected:
    //! See Primary::Primary().
    Map(
        size_t num_static_args,
        size_t num_dynamic_args
    );

    /**
     * See Primary::eval_primary().
     *
     * Do not override.  Instead, override eval_sub().
     **/
    void eval_primary(
        MemoryManager          mm,
        const node_cp&         me,
        boost::any&            substate,
        NodeEvalState&         my_state,
        const value_vec_t&     secondary_args,
        const NodeEvalState&   primary_arg
    ) const;

    /**
     * Subfunction to apply to each subvalue.
     *
     * @param[in] mm             Memory manager defining lifetime of new 
     *                           values.
     * @param[in] secondary_args Secondary arguments.
     * @param[in] subvalue       Subvalue.
     * @return Subresult.
     **/
    virtual
    Value eval_sub(
        MemoryManager      mm,
        const value_vec_t& secondary_args,
        Value              subvalue
    ) const = 0;
};

/**
 * Delegate for Calls that select a subset of a list.
 *
 * This class is similar to Map except that it uses the subfunction to 
 * determine which values to include rather than to modify them.
 *
 * If the primary argument is the empty list, result is the empty list.  If
 * the primary argument is not a list, result is the argument if the 
 * subfunction returns true for it and null otherwise.  If the primary 
 * argument is a list, results is a list of the elements for which the 
 * subfunction returns true.
 *
 * For use, see Base and Primary, and override eval_filter().
 **/
class Filter :
    public Primary
{
public:
    //! See Base::eval_initialize().
    void eval_initialize(boost::any& substate) const;

protected:
    //! See Primary::Primary().
    Filter(
        size_t num_static_args,
        size_t num_dynamic_args
    );

    /**
     * See Primary::eval_primary().
     *
     * Do not override.  Instead, override eval_filter().
     **/
    void eval_primary(
        MemoryManager          mm,
        const node_cp&         me,
        boost::any&            substate,
        NodeEvalState&         my_state,
        const value_vec_t&     secondary_args,
        const NodeEvalState&   primary_arg
    ) const;

    /** 
     * Subfunction to test eah subvalue.
     *
     * @param[in] mm             Memory manager.  Unlikely to be used.
     * @param[in] secondary_args Secondary arguments.
     * @param[in] subvalue       Subvalue.
     * @return Whether @a subvalue should be included in the result.
     **/
    virtual
    bool eval_filter(
        MemoryManager      mm, 
        const value_vec_t& secondary_args, 
        Value              subvalue
    ) const = 0;
};

/**
 * Generator for Calls created using Base hiearchy.
 *
 * @tparam BaseSubclass Subclass of call to create.
 * @param[in] name Name of call function to create.
 * @return Call node.
 **/
template <typename BaseSubclass>
call_p generate(const std::string& name)
{
    return call_p(new Impl::Call(name, base_p(new BaseSubclass())));
}

} // Functional
} // Predicate
} // IronBee

#endif
