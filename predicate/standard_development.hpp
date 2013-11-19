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
 * @brief Predicate --- Standard Development
 *
 * See reference.md for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__STANDARD_DEVELOPMENT__
#define __PREDICATE__STANDARD_DEVELOPMENT__

#include <predicate/call_factory.hpp>
#include <predicate/dag.hpp>
#include <predicate/meta_call.hpp>

namespace IronBee {
namespace Predicate {
namespace Standard {

/**
 * Output value of first child and message of second child.
 **/
class P :
    public MapCall
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
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
};

/**
 * Take values of child; do not transform.
 **/
class Identity :
    public MapCall
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
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
};

/**
 * Sequence of values; potentially infinite.
 **/
class Sequence :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_initialize().
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
 * Load all standard development calls into a CallFactory.
 *
 * @param [in] to CallFactory to load into.
 **/
void load_development(CallFactory& to);

} // Standard
} // Predicate
} // IronBee

#endif
