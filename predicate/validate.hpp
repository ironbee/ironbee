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
 * @brief Predicate --- Validation Support
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__VALIDATION__
#define __PREDICATE__VALIDATION__

#include "dag.hpp"
#include "reporter.hpp"

namespace IronBee {
namespace Predicate {

/**
 * Easy validation of Call nodes.
 *
 * This namespace defines a set of templates and routines designed to do
 * easy common validation checks on custom Call nodes such as
 * "has N children".  It uses template metaprogramming and multiple
 * inheritance to accomplish this.  Example of use:
 *
 * @code
 * class MyCustomCallClass :
 *   public Validate::Call<MyCustomCallClass>,
 *   public Validate::NChildren<5,
            Validate::NthChildStatic<3
 *          > >
 * {
 *     ...
 * };
 * @endcode
 *
 * Use involves two parts:
 *
 * - The class publically inherits from Validate::Call, passing itself in as
 *   the template parameter.  Validate::Call will implement pre_transform()
 *   and post_transform() to call the validation code defined by the
 *   the validation chain (see below).
 * - A validation chain of 1 or more validations.  The chain is a sequence of
 *   one or more validation templates with the next element of the chain
 *   passed in as the last template argument.  The final element of the chain
 *   omits the last template argument or uses the default, Validate::Base.
 *   The validation chain will add the method
 *   `virtual void validate(NodeReporter&) const` to the class that performs
 *   all the validations in the chain.
 *
 * How it works: Validate::Call::pre_transform() (similar for
 * Validate::Call::post_transform()) will dynamic cast @c *this to the
 * subclass type (passed in as a template argument) and call @c validate() of
 * that.  As this is now at the bottom of the class hierarchy, it will be able
 * to find the @c validate() defined by the validation chain and call it.
 *
 * Why not have Validate::Call be the top fo the validation chain?  This
 * technique would work perfectly well if Validate::Call was used in place of
 * Validate::Base at the top of the validation chain.  Doing so would also
 * remove the need for the dynamic cast Validate::Call would be able to
 * define the virtual method validate in its own definition and thus have
 * access to it without the need of a dynamic cast.  However, this approach
 * would result in an extremely complicated parent for the subclass making it
 * difficult to further override Node::pre_transform() and
 * Node::post_transform().  Under the current approach, these can be further
 * used via:
 *
 * @code
 * virtual void pre_transform(NodeReporter& reporter) const
 * {
 *    Validate::Call<MyCustomCallClass>::pre_transform(reporter);
 *    // Custom pre_transform code.
 *    ...
 * }
 *
 * How to write a new validator:
 *
 * - If possible, write the validator logic is a function (see, e.g.,
 *   validate_n_children()).  Recall that the node to be validated is
 *   available via NodeReporter::node().
 * - Write a template that calls your function.  You can use typed template
 *   parameters to parameterize your validator, i.e., pass additional
 *   arguments to your function.  At present, there is no way to pass complex
 *   types in.  Your template should chian via the last argument, e.g.,
 *
 * @code
 * template <size_t arg1, class Chain = Validate::Base>
 * struct MyCustomValidator : public Chain
 * {
 *     virtual void validate(NodeReporter& reporter) const
 *     {
 *         my_custom_validator(reporter, arg1);
 *         Chain::validate(reporter);
 *     }
 * };
 **/
namespace Validate {

/**
 * Subclass this instead of Call to make use of validation chains.
 *
 * Subclassing from this will implement Node::pre_transform() and
 * Node::post_transform() to dynamic cast to @a Subclass and then call
 * validate().  See Predicate::Validate.
 *
 * @tparam Subclass Type of subclass.
 **/
template <class Subclass>
class Call :
    public Predicate::Call
{
public:
    //! Dynamic cast to @a Subclass and call Base::validate().
    virtual void pre_transform(NodeReporter& reporter) const
    {
        assert(reporter.node().get() == this);
        Predicate::Call::pre_transform(reporter);
        dynamic_cast<const Subclass&>(*this).validate(reporter);
    }

    //! Dynamic cast to @a Subclass and call Base::validate().
    virtual void post_transform(NodeReporter& reporter) const
    {
        assert(reporter.node().get() == this);
        Predicate::Call::post_transform(reporter);
        dynamic_cast<const Subclass&>(*this).validate(reporter);
    }
};

/**
 * Top of a validation chain.  Defines validate().
 *
 * You will not normally need to reference this class directly.  It is the
 * default argument of the @a Chain parameters of validation templates.
 **/
class Base
{
public:
    //! Validate this node.  Currently nop.
    virtual void validate(NodeReporter&) const
    {
        // nop
    }
};

/**
 * Report error if not exactly @a n children.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        How many children expected.
 **/
void validate_n_children(NodeReporter& reporter, size_t n);

/**
 * Validation: validate_n_children()
 *
 * @tparam N     How may children should have.
 * @tparam Chain Next validation; see Predicate::Validate.
 **/
template <size_t N, class Chain = Base>
struct NChildren :
    public Chain
{
    //! See Base::validate() and Predicate::Validate.
    virtual void validate(NodeReporter& reporter) const
    {
        validate_n_children(reporter, N);
        Chain::validate(reporter);
    }
};

} // Validate
} // Predicate
} // IronBee

#endif
