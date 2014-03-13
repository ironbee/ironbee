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
 * @brief Predicate --- Standard Template.
 *
 * See reference.txt for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__STANDARD_TEMPLATE__
#define __PREDICATE__STANDARD_TEMPLATE__

#include <predicate/call_factory.hpp>
#include <predicate/merge_graph.hpp>

using namespace std;

namespace IronBee {
namespace Predicate {
namespace Standard {

//! List of arguments.
typedef std::list<std::string> template_arg_list_t;

/**
 * Reference to something else, see Template.
 *
 * Ref nodes exist only to reference something else.  They do not transform
 * and throw exceptions if calculated.  They are replaced by
 * Template::transform() when they appear in a template body.
 **/
class Ref :
    public Call
{
public:
    //! See Call::name()
    virtual std::string name() const;

    //! See Node::validate().
    virtual bool validate(NodeReporter reporter) const;

    //! See Node::post_transform().
    void post_transform(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(GraphEvalState&, EvalContext) const;
};

/**
 * Call that transforms based on an expression and ref substitution.
 *
 * A template call is initialized with a body expression tree and an argument
 * list.  At transformation, the body is traversed and any ref nodes are
 * replaced by replacing a ref node whose name is at position @c i in the
 * argument list with the @c ith child.
 **/
class Template :
    public Call
{
public:
    /**
     * Constructor.
     *
     * @param[in] name Name of template.
     * @param[in] args List of arguments.  All ref nodes in @a body must
     *                 use one of these.
     * @param[in] body Body.  Any ref nodes in body will be replaced by
     *                 children of this node according to @a args.
     **/
    Template(
        const std::string&          name,
        const template_arg_list_t&  args,
        const node_cp&              body
    );

    //! See Call::name()
    virtual std::string name() const;

   /**
    * See Node::transform().
    *
    * Will replace self with tree copy of body with ref nodes replace
    * according to children and @a args.
    **/
   virtual bool transform(
       MergeGraph&        merge_graph,
       const CallFactory& call_factory,
       NodeReporter       reporter
   );

   //! See Node::validate().
   virtual bool validate(NodeReporter reporter) const;

   //! See Node::post_transform().
   void post_transform(NodeReporter reporter) const;

    //! See Node::eval_calculate()
    virtual void eval_calculate(GraphEvalState&, EvalContext) const;

private:
    //! Name.
    const std::string m_name;
    //! Arguments.
    const template_arg_list_t m_args;
    //! Body expression.
    const node_cp m_body;
};

/**
 * Create a Template generator.
 *
 * @param[in] args Template arguments.
 * @param[in] body Template body.
 * @return Generator suitable for registration with call factory.
 **/
CallFactory::generator_t define_template(
    const template_arg_list_t& args,
    const node_cp&             body
);

/**
 * Load all standard Template calls into a CallFactory.
 *
 * Adds Ref to @a to.  Templates need to be added as they are defined.
 *
 * @sa define_template()
 *
 * @param [in] to CallFactory to load into.
 **/
void load_template(CallFactory& to);

} // Standard
} // Predicate
} // IronBee

#endif
