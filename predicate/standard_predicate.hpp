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
 * @brief Predicate --- Standard Predicates
 *
 * See reference.txt for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__STANDARD_PREDICATE__
#define __PREDICATE__STANDARD_PREDICATE__

#include <predicate/merge_graph.hpp>
#include <predicate/meta_call.hpp>

#include <ironbeepp/operator.hpp>
#include <ironbeepp/transformation.hpp>

#include <boost/foreach.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

//! Does second child have more than specified values?
class IsLonger :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const;

    /**
     * See Node::transform().
     *
     * Will replace self with if possible if child is literal.
     **/
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        NodeReporter       reporter
    );

    //! See Node::validate()
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(GraphEvalState&, EvalContext) const;
};

//! Is child literal?
class IsLiteral :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const;

    /**
     * See Node::transform().
     *
     * Will replace self with true or false based on child.
     **/
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        NodeReporter       reporter
    );

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(GraphEvalState&, EvalContext) const;
};

//! Is child simple?
class IsSimple :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const;

    /**
     * See Node::transform().
     *
     * Will replace self with true if child is literal.
     **/
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        NodeReporter       reporter
    );

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(GraphEvalState&, EvalContext) const;
};

//! Is child finished?
class IsFinished :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const;

    /**
     * See Node::transform().
     *
     * Will replace self with true if child is literal.
     **/
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        NodeReporter       reporter
    );

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(GraphEvalState&, EvalContext) const;
};

//! Do all values of child have the same type?
class IsHomogeneous :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const;

   /**
    * See Node::transform().
    *
    * Will replace self with true if child is literal.
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

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(GraphEvalState&, EvalContext) const;
};

/**
 * Load all standard predicate calls into a CallFactory.
 *
 * @param [in] to CallFactory to load into.
 **/
void load_predicate(CallFactory& to);

} // Standard
} // Predicate
} // IronBee

#endif
