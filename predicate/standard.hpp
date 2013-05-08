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
#include <predicate/validate.hpp>

#include <boost/scoped_ptr.hpp>

namespace IronBee {
namespace Predicate {
namespace Standard {

/**
 * Falsy value, null.
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
    virtual Value calculate(EvalContext);
};

/**
 * Truthy value, ''.
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
    virtual Value calculate(EvalContext);
};

/**
 * Base class for calls that want children in canonical order.
 **/
class AbelianCall :
    public Validate::Call<AbelianCall>,
    public Validate::NOrMoreChildren<2>
{
public:
    //! Constructor.
    AbelianCall();

    // The three routines below simply mark this node as unordered.
    //! See Node::add_child().
    virtual void add_child(const node_p& child);
    //! See Node::replace_child().
    virtual void replace_child(const node_p& child, const node_p& with);

    /**
     * See Node::transform().
     *
     * Will order children canonically.
     **/
    virtual bool transform(
        MergeGraph&        merge_graph,
        const CallFactory& call_factory,
        NodeReporter       reporter
    );

private:
    typedef Validate::Call<AbelianCall> parent_t;
    bool m_ordered;
};

/**
 * True iff any children are truthy.
 **/
class Or :
    public AbelianCall
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
    virtual Value calculate(EvalContext context);
};

/**
 * True iff all children are truthy.
 **/
class And :
    public AbelianCall
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
    virtual Value calculate(EvalContext context);
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
    virtual Value calculate(EvalContext context);
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
    virtual Value calculate(EvalContext context);
};

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
    virtual Value calculate(EvalContext context);
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
    virtual Value calculate(EvalContext context);

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
    virtual Value calculate(EvalContext context);

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
    virtual Value calculate(EvalContext context);

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
    virtual Value calculate(EvalContext context);

private:
    const std::string m_transformation;
};

/**
 * Construct a named value from a name (string) and value.
 **/
class SetName :
    public Validate::Call<SetName>,
    public Validate::NChildren<2,
           Validate::NthChildIsString<0,
           Validate::NthChildIsNotNull<1
           > > >
{
public:
    //! See Call:name()
    virtual std::string name() const;

protected:
    virtual Value calculate(EvalContext context);
};

/**
 * Construct list of values of children.
 **/
class List :
    public Call
{
public:
    //! See Call:name()
    virtual std::string name() const;

protected:
    virtual Value calculate(EvalContext context);
};

/**
 * First subfield of name from first child in second child.
 *
 * Is null if second child is not a list or has no such subfield.
 * If all subfields of a given name are wanted, use SubAll.
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
    virtual Value calculate(EvalContext context);
};

/**
 * All subfield of name from first child in second child.
 *
 * Is null if second child is not a list or result would be empty.
 * If only one subfield of a given name is wanted, use Sub.
 **/
class SubAll :
    public Validate::Call<SubAll>,
    public Validate::NChildren<2,
           Validate::NthChildIsString<0
           > >
{
public:
    //! See Call:name()
    virtual std::string name() const;

protected:
    virtual Value calculate(EvalContext context);
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
