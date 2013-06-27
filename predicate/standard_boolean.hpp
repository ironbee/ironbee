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
 * @brief Predicate --- Standard Boolean
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__STANDARD_BOOLEAN__
#define __PREDICATE__STANDARD_BOOLEAN__

#include <predicate/call_factory.hpp>
#include <predicate/dag.hpp>
#include <predicate/meta_call.hpp>
#include <predicate/validate.hpp>

namespace IronBee {
namespace Predicate {
namespace Standard {

/**
 * Falsy value, [].
 **/
class False :
    public Validate::Call<False>,
    public Validate::NChildren<0>
{
public:
    //! See Call::name()
    virtual std::string name() const;

   /**
    * See Node::transform().
    *
    * Will replace self with Null.
    **/
   virtual bool transform(
       MergeGraph&        merge_graph,
       const CallFactory& call_factory,
       NodeReporter       reporter
   );

protected:
    //! See Node::calculate()
    virtual void calculate(EvalContext);
};

/**
 * Truthy value, [''].
 **/
class True :
    public Validate::Call<True>,
    public Validate::NChildren<0>
{
public:
    //! See Call::name()
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

protected:
    //! See Node::calculate()
    virtual void calculate(EvalContext);
};

/**
 * True iff any children are truthy.
 **/
class Or :
    public AbelianCall,
    public Validate::Call<Or>,
    public Validate::NOrMoreChildren<2>
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
       NodeReporter       reporter
   );

protected:
    virtual void calculate(EvalContext context);
};

/**
 * True iff all children are truthy.
 **/
class And :
    public AbelianCall,
    public Validate::Call<And>,
    public Validate::NOrMoreChildren<2>
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
       NodeReporter       reporter
   );
protected:
    virtual void calculate(EvalContext context);
};

/**
 * True iff child is falsy.
 **/
class Not :
    public Validate::Call<Not>,
    public Validate::NChildren<1>
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
        NodeReporter       reporter
    );

protected:
    virtual void calculate(EvalContext context);
};

/**
 * If first child is true, second child, else third child.
 **/
class If :
    public Validate::Call<If>,
    public Validate::NChildren<3>
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
        NodeReporter       reporter
    );

protected:
    virtual void calculate(EvalContext context);
};

// XXX AndSC
// XXX OrSC

/**
 * Load all standard boolean calls into a CallFactory.
 *
 * @param [in] to CallFactory to load into.
 **/
void load_boolean(CallFactory& to);

} // Standard
} // Predicate
} // IronBee

#endif
