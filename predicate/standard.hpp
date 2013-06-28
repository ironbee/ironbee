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
 * @brief Predicate --- Standard Nodes
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__STANDARD__
#define __PREDICATE__STANDARD__

#include <predicate/call_factory.hpp>
#include <predicate/dag.hpp>
#include <predicate/meta_call.hpp>
#include <predicate/validate.hpp>

#include <boost/scoped_ptr.hpp>

namespace IronBee {
namespace Predicate {
namespace Standard {

/**
 * Returns data field with name given by only argument.
 **/
class Field :
    public Validate::Call<Field>,
    public Validate::NChildren<1,
           Validate::NthChildIsString<0
           > >
{
public:
    //! See Call::name()
    virtual std::string name() const;

protected:
    virtual void calculate(EvalContext context);
    virtual void pre_eval(Environment environment, NodeReporter reporter);

private:
    bool m_is_indexed;
    size_t m_index;
};

/**
 * Run IronBee operator.
 *
 * First child is name of operator, second is parameters, third is input.
 * First and second must be string literals.  If operator results is 0,
 * node value is NULL, otherwise node value is the capture collection.  The
 * collection will be empty for operators that do not support capture.
 **/
class Operator :
    public MaplikeCall,
    public Validate::Call<Operator>,
    public Validate::NChildren<3,
           Validate::NthChildIsString<0,
           Validate::NthChildIsString<1
           > > >
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::pre_eval()
    virtual void pre_eval(Environment environment, NodeReporter reporter);

protected:
    virtual void calculate(EvalContext context);
    virtual Value value_calculate(Value v, EvalContext context);

private:
    typedef Validate::Call<Operator> parent_t;

    //! Hidden complex implementation details.
    struct data_t;

    //! Hidden complex implementation details.
    boost::scoped_ptr<data_t> m_data;
};

/**
 * As Operator, but with operator name bound in advance.
 *
 * Unlike most Call's, SpecificOperator takes a name in its constructor.  This
 * name is used both as its name and as the name of the Operator to transform
 * into.  On transform, it will replace itself with an Operator with a first
 * child as the name and the latter children as the same as the
 * SpecificOperator's children.
 **/
class SpecificOperator :
    public Validate::Call<SpecificOperator>,
    public Validate::NChildren<2,
           Validate::NthChildIsString<0
           > >
{
public:
    //! Constructor.
    explicit
    SpecificOperator(const std::string& op);

    //! See Call:name()
    virtual std::string name() const;

   /**
    * See Node::transform().
    *
    * Will replace self with Operator with first argument given by
    * constructor.
    **/
   virtual bool transform(
       MergeGraph&        merge_graph,
       const CallFactory& call_factory,
       NodeReporter       reporter
   );

protected:
    virtual void calculate(EvalContext context);

private:
    const std::string m_operator;
};

/**
 * Run IronBee transformation.
 *
 * Execute an IronBee transformation.  The first child must be a string
 * literal naming the transformation.  The second child is the input.
 **/
class Transformation :
    public MaplikeCall,
    public Validate::Call<Transformation>,
    public Validate::NChildren<2,
           Validate::NthChildIsString<0
           > >
{
public:
    //! See Call:name()
    virtual std::string name() const;

    //! See Node::pre_eval()
    virtual void pre_eval(Environment environment, NodeReporter reporter);

protected:
    virtual void calculate(EvalContext context);
    virtual Value value_calculate(Value v, EvalContext context);

private:
    typedef Validate::Call<Operator> parent_t;

    void local_validate(NodeReporter reporter) const;

    //! Hidden complex implementation details.
    struct data_t;

    //! Hidden complex implementation details.
    boost::scoped_ptr<data_t> m_data;
};

/**
 * As Transformation, but with transformation name bound in advance.
 *
 * @sa SpecificOperator
 **/
class SpecificTransformation :
    public Validate::Call<SpecificTransformation>,
    public Validate::NChildren<1
           >
{
public:
    //! Constructor.
    explicit
    SpecificTransformation(const std::string& tfn);

    //! See Call:name()
    virtual std::string name() const;

   /**
    * See Node::transform().
    *
    * Will replace self with Operator with first argument given by
    * constructor.
    **/
   virtual bool transform(
       MergeGraph&        merge_graph,
       const CallFactory& call_factory,
       NodeReporter       reporter
   );

protected:
    virtual void calculate(EvalContext context);

private:
    const std::string m_transformation;
};

/**
 * Subfields of given name from first child in collection of second child.
 *
 * First child must be a string literal.  Second child must be a simple value.
 * If second child is empty or not a collection (Value of type list), no
 * outputs are produced.
 *
 * If you want to choose values of a (non-simple) child of a given name,
 * see Choose.
 *
 * @sa Choose
 **/
class Sub :
    public Validate::Call<Sub>,
    public Validate::NChildren<2,
           Validate::NthChildIsString<0
           > >
{
public:
    //! See Call:name()
    virtual std::string name() const;

protected:
    virtual void calculate(EvalContext context);
};

/**
 * Load all standard calls into a CallFactory.
 *
 * @param [in] to CallFactory to load into.
 **/
void load(CallFactory& to);

} // Standard
} // Predicate
} // IronBee

#endif
