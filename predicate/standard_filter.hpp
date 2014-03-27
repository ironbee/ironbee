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
 * @brief Predicate --- Standard Filter
 *
 * See reference.txt for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__STANDARD_FILTER__
#define __PREDICATE__STANDARD_FILTER__

#include <predicate/call_factory.hpp>
#include <predicate/dag.hpp>
#include <predicate/meta_call.hpp>

namespace IronBee {
namespace Predicate {
namespace Standard {

/// @cond Impl
namespace Impl {
    
class FilterBase :
    public MapCall
{
public:
    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;
    
    //! See Node::eval_calculate().
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
        
protected:
    //! See MapCall::value_calculate()
    virtual Value value_calculate(
        Value           v,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
    
    //! Pass the filter?  Child must implement.
    virtual bool pass_filter(Value f, Value v) const = 0;
};

};
/// @endcond
/**
 * Output elements of second child that are equal to first (simple) child.
 **/
class Eq :
    public Impl::FilterBase
{
public:
    //! See Call::name()
    virtual std::string name() const;

protected:
    //! See Impl::FilterBase::pass_filter()
    virtual bool pass_filter(Value f, Value v) const;
};

/**
 * Output elements of second child that are not equal to first (simple) child.
 **/
class Ne :
    public Impl::FilterBase
{
public:
    //! See Call::name()
    virtual std::string name() const;

protected:
    //! See Impl::FilterBase::pass_filter()
    virtual bool pass_filter(Value f, Value v) const;
};

/**
 * Output elements of second child that the (simple) first child is less than.
 **/
class Lt :
    public Impl::FilterBase
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

protected:
    //! See Impl::FilterBase::pass_filter()
    virtual bool pass_filter(Value f, Value v) const;
};

/**
 * Output elements of second child that the (simple) first child is less than
 * or equal to.
 **/
class Le :
    public Impl::FilterBase
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

protected:
    //! See Impl::FilterBase::pass_filter()
    virtual bool pass_filter(Value f, Value v) const;
};

/**
 * Output elements of second child that the (simple) first child is greater
 * than.
 **/
class Gt :
    public Impl::FilterBase
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

protected:
    //! See Impl::FilterBase::pass_filter()
    virtual bool pass_filter(Value f, Value v) const;
};

/**
 * Output elements of second child that the (simple) first child is greater
 * than or equal to.
 **/
class Ge :
    public Impl::FilterBase
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

protected:
    //! See Impl::FilterBase::pass_filter()
    virtual bool pass_filter(Value f, Value v) const;
};

/**
 * Output elements of second child of type described by first child.
 **/
class Typed :
    public MapCall
{
public:
    //! Constructor.
    Typed();

    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::pre_eval().
    virtual void pre_eval(Environment environment, NodeReporter reporter);

protected:
    //! See MapCall::value_calculate()
    virtual Value value_calculate(
        Value           v,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

    //! See Node::eval_calculate().
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
 * Output elements of second child with name described by first child.
 **/
class Named :
    public Impl::FilterBase
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

protected:
    //! See Impl::FilterBase::pass_filter()
    virtual bool pass_filter(Value f, Value v) const;
};

/**
 * Case insensitive version of Named.
 **/
class NamedI :
    public Impl::FilterBase
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

protected:
    //! See Impl::FilterBase::pass_filter()
    virtual bool pass_filter(Value f, Value v) const;
};

/**
 * Synonym for NamedI.
 **/
class Sub :
    public AliasCall
{
public:
    //! Constructor.
    Sub();

    //! See Call::name()
    virtual std::string name() const;
};

/**
 * Output elements of second child with name matching first child.
 **/
class NamedRx :
    public MapCall
{
public:
    //! Constructor.
    NamedRx();

    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::pre_eval().
    virtual void pre_eval(Environment environment, NodeReporter reporter);

protected:
    //! See MapCall::value_calculate()
    virtual Value value_calculate(
        Value           v,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

    //! See Node::eval_calculate().
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
 * Load all standard filter calls into a CallFactory.
 *
 * @param [in] to CallFactory to load into.
 **/
void load_filter(CallFactory& to);

} // Standard
} // Predicate
} // IronBee

#endif
