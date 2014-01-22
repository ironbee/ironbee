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
 * @brief Predicate --- Standard ValueList.
 *
 * See reference.md for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__STANDARD_VALUELIST__
#define __PREDICATE__STANDARD_VALUELIST__

#include <predicate/merge_graph.hpp>
#include <predicate/meta_call.hpp>

#include <ironbeepp/operator.hpp>
#include <ironbeepp/transformation.hpp>

#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

/**
 * Construct a named value from a name (string) and value.
 **/
class SetName :
    public MapCall
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

protected:
    //! See MapCall::value_calculate().
    virtual Value value_calculate(
        Value           v,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * Push the name of a list value to its children.
 **/
class PushName :
    public MapCall
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

protected:
    //! See MapCall::value_calculate().
    virtual Value value_calculate(
        Value           v,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
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
        NodeEvalState& node_eval_state,
        EvalContext    context
    ) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * First value.
 **/
class First :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * All but first value.
 **/
class Rest :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_initialize()
    virtual void eval_initialize(
        NodeEvalState& node_eval_state,
        EvalContext    context
    ) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * Nth value.
 **/
class Nth :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * Expand simple list value.
 **/
class Scatter :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * Gathers values into simple list value.
 **/
class Gather :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * Flatten list values into a valuelist.
 **/
class Flatten :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    /**
     * See Node::transform().
     *
     * Will replace self with child if child is non-list literal.
     **/
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        NodeReporter       reporter
    );

    //! See Node::eval_initialize()
    virtual void eval_initialize(
        NodeEvalState& node_eval_state,
        EvalContext    context
    ) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * Focus on one value from each child.
 **/
class Focus :
    public MapCall
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;

protected:
    //! See MapCall::value_calculate().
    virtual Value value_calculate(
        Value           v,
        GraphEvalState& graph_eval_state,
        EvalContext     context
    ) const;
};

/**
 * Load all standard ValueList calls into a CallFactory.
 *
 * @param [in] to CallFactory to load into.
 **/
void load_valuelist(CallFactory& to);

} // Standard
} // Predicate
} // IronBee

#endif
