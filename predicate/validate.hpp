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

#include <predicate/dag.hpp>
#include <predicate/reporter.hpp>

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
 * - The class publicly inherits from Validate::Call, passing itself in as
 *   the template parameter.  Validate::Call will implement pre_transform()
 *   and post_transform() to call the validation code defined by the
 *   the validation chain (see below).
 * - A validation chain of 1 or more validations.  The chain is a sequence of
 *   one or more validation templates with the next element of the chain
 *   passed in as the last template argument.  The final element of the chain
 *   omits the last template argument or uses the default, Validate::Base.
 *   The validation chain will add the method
 *   `virtual void validate(NodeReporter) const` to the class that performs
 *   all the validations in the chain.
 *
 * How it works: Validate::Call::pre_transform() (similar for
 * Validate::Call::post_transform()) will dynamic cast @c *this to the
 * subclass type (passed in as a template argument) and call @c validate() of
 * that.  As this is now at the bottom of the class hierarchy, it will be able
 * to find the @c validate() defined by the validation chain and call it.
 *
 * Why not have Validate::Call be the top of the validation chain?  This
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
 * virtual void pre_transform(NodeReporter reporter) const
 * {
 *    Validate::Call<MyCustomCallClass>::pre_transform(reporter);
 *    // Custom pre_transform code.
 *    ...
 * }
 * @endcode
 *
 * How to write a new validator:
 *
 * - If possible, write the validator logic is a function (see, e.g.,
 *   n_children()).  Recall that the node to be validated is
 *   available via NodeReporter::node().
 * - Write a template that calls your function.  You can use typed template
 *   parameters to parameterize your validator, i.e., pass additional
 *   arguments to your function.  At present, there is no way to pass complex
 *   types in.  Your template should chain via the last argument, e.g.,
 *
 * @code
 * template <size_t arg1, class Chain = Validate::Base>
 * struct MyCustomValidator : public Chain
 * {
 *     virtual void validate(NodeReporter reporter) const
 *     {
 *         my_custom_validator(reporter, arg1);
 *         Chain::validate(reporter);
 *     }
 * };
 * @endcode
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
    virtual public Predicate::Call
{
public:
    //! Dynamic cast to @a Subclass and call Base::validate().
    virtual void pre_transform(NodeReporter reporter) const
    {
        assert(reporter.node().get() == this);
        Predicate::Call::pre_transform(reporter);
        dynamic_cast<const Subclass&>(*this).validate(reporter);
    }

    //! Dynamic cast to @a Subclass and call Base::validate().
    virtual void post_transform(NodeReporter reporter) const
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
    virtual void validate(NodeReporter) const
    {
        // nop
    }
};

/**
 * Metafunction to construct a validator from a function taking a size_t.
 *
 * @tparam F Function of @c NodeReporter and @c size_t to call.
 **/
template <void (*F)(NodeReporter, size_t)>
struct make_validator_size
{
    //! Value; a validator.
    template <size_t N, class Chain = Validate::Base>
    struct value : public Chain
    {
        //! See Base::validate() and Predicate::Validate.
        virtual void validate(NodeReporter reporter) const
        {
            F(reporter, N);
            Chain::validate(reporter);
        }
    };
};

/**
 * Metafunction to construct a validator from a function taking nothing.
 *
 * @tparam F Function of @c NodeReporter to call.
 **/
template <void (*F)(NodeReporter)>
struct make_validator
{
    //! Value; a validator.
    template <class Chain = Validate::Base>
    struct value : public Chain
    {
        //! See Base::validate() and Predicate::Validate.
        virtual void validate(NodeReporter reporter) const
        {
            F(reporter);
            Chain::validate(reporter);
        }
    };
};

/**
 * Report error if not exactly @a n children.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        How many children expected.
 **/
void n_children(NodeReporter reporter, size_t n);

/**
 * Report error if not @a n or more children.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Minimum number of children expected.
 **/
void n_or_more_children(NodeReporter reporter, size_t n);

/**
 * Report error if not @a n or fewer children.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Maximum number of children expected.
 **/
void n_or_fewer_children(NodeReporter reporter, size_t n);

/**
 * Report error if @a nth child is not string literal.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Which child should be a string literal.
 **/
void nth_child_is_string(NodeReporter reporter, size_t n);

/**
 * Report error if @a nth child is not number literal.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Which child should be a number literal.
 **/
void nth_child_is_integer(NodeReporter reporter, size_t n);

/**
 * Report error if @a nth child is not float literal.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Which child should be a float literal.
 **/
void nth_child_is_float(NodeReporter reporter, size_t n);

/**
 * Report error if @a nth child is not a null.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Which child should be a null.
 **/
void nth_child_is_null(NodeReporter reporter, size_t n);

/**
 * Report error if @a nth child is a null.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Which child should not be a null.
 **/
void nth_child_is_not_null(NodeReporter reporter, size_t n);

/**
 * Report error if any child is literal.
 *
 * @param[in] reporter Reporter to use.
 **/
void no_child_is_literal(NodeReporter reporter);

/**
 * Report error if any child is null.
 *
 * @param[in] reporter Reporter to use.
 **/
void no_child_is_null(NodeReporter reporter);

/**
 * Validator: n_children()
 **/
template <size_t N, class Chain = Base>
struct NChildren :
    public make_validator_size<
        &n_children
    >::value<N, Chain>
{};

/**
 * Validator: n_or_more_children()
 **/
template <size_t N, class Chain = Base>
struct NOrMoreChildren :
    public make_validator_size<
        &n_or_more_children
    >::value<N, Chain>
{};

/**
 * Validator: n_or_fewer_children()
 **/
template <size_t N, class Chain = Base>
struct NOrFewerChildren :
    public make_validator_size<
        &n_or_fewer_children
    >::value<N, Chain>
{};

/**
 * Validator: nth_child_is_string()
 **/
template <size_t N, class Chain = Base>
struct NthChildIsString :
    public make_validator_size<
        &nth_child_is_string
    >::value<N, Chain>
{};

/**
 * Validator: nth_child_is_integer()
 **/
template <size_t N, class Chain = Base>
struct NthChildIsInteger :
    public make_validator_size<
        &nth_child_is_integer
    >::value<N, Chain>
{};

/**
 * Validator: nth_child_is_float()
 **/
template <size_t N, class Chain = Base>
struct NthChildIsFloat :
    public make_validator_size<
        &nth_child_is_float
    >::value<N, Chain>
{};

/**
 * Validator: nth_child_is_null()
 **/
template <size_t N, class Chain = Base>
struct NthChildIsNull :
    public make_validator_size<
        &nth_child_is_null
    >::value<N, Chain>
{};

/**
 * Validator: nth_child_is_not_null()
 **/
template <size_t N, class Chain = Base>
struct NthChildIsNotNull :
    public make_validator_size<
        &nth_child_is_not_null
    >::value<N, Chain>
{};

/**
 * Validator: no_child_is_literal()
 **/
template <class Chain = Base>
struct NoChildIsLiteral :
    public make_validator<
        &no_child_is_literal
    >::value<Chain>
{};

/**
 * Validator: no_child_is_null()
 **/
template <class Chain = Base>
struct NoChildIsNull :
    public make_validator<
        &no_child_is_null
    >::value<Chain>
{};

} // Validate
} // Predicate
} // IronBee

#endif
