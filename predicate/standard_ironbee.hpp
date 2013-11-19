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
 * @brief Predicate --- Standard IronBee
 *
 * See reference.md for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__STANDARD_IRONBEE__
#define __PREDICATE__STANDARD_IRONBEE__

#include <predicate/call_factory.hpp>
#include <predicate/dag.hpp>
#include <predicate/meta_call.hpp>

namespace IronBee {
namespace Predicate {
namespace Standard {

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

//! Alias for Var.
class Field :
    public AliasCall
{
public:
    //! Constructor.
    Field();

    //! See Call::name()
    virtual std::string name() const;
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
 * literal naming the transformation.  The second child is the input.
 **/
class Transformation :
    public MapCall
{
public:
    //! Constructor.
    Transformation();

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

private:
    //! Hidden complex implementation details.
    struct data_t;

    //! Hidden complex implementation details.
    boost::scoped_ptr<data_t> m_data;
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

/**
 * Load all standard filter calls into a CallFactory.
 *
 * @param [in] to CallFactory to load into.
 **/
void load_ironbee(CallFactory& to);

} // Standard
} // Predicate
} // IronBee

#endif
