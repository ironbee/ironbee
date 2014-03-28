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
 * Validation checks for call nodes.
 *
 * This namespace defines a set of routines to do common validation checks on
 * custom Call nodes such as "has N children".  To use, simply override
 * Node::validate() in your child, and call these checks.  E.g.,
 *
 * @code
 * bool MyCall::validate(NodeReporter reporter) const
 * {
 *     // Note arrangement to avoid short circuiting.
 *     bool result = true;
 *     result = Validate::no_child_s_null(reporter) && result;
 *     result = Validate::has_n_children(reporter, 3) && result;
 *     return result;
 * }
 * @endcode
 **/
namespace Validate {

/**
 * Report error if not exactly @a n children.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        How many children expected.
 * @return true iff validation succeeded.
 **/
bool n_children(NodeReporter reporter, size_t n);

/**
 * Report error if not @a n or more children.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Minimum number of children expected.
 * @return true iff validation succeeded.
 **/
bool n_or_more_children(NodeReporter reporter, size_t n);

/**
 * Report error if not @a n or fewer children.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Maximum number of children expected.
 * @return true iff validation succeeded.
 **/
bool n_or_fewer_children(NodeReporter reporter, size_t n);

/**
 * Report error if @a nth child is not literal.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Which child should not be a null.
 * @return true iff validation succeeded.
 **/
bool nth_child_is_literal(NodeReporter reporter, size_t n);

/**
 * Report error if @a nth child is not string literal.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Which child should be a string literal.
 * @return true iff validation succeeded.
 **/
bool nth_child_is_string(NodeReporter reporter, size_t n);

/**
 * Report error if @a nth child is not number literal.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Which child should be a number literal.
 * @return true iff validation succeeded.
 **/
bool nth_child_is_integer(NodeReporter reporter, size_t n);

/**
 * Report error if @a nth child is not number literal or below @a max.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Which child should be a number literal.
 * @param[in] max      Maximum valid value.
 * @return true iff validation succeeded.
 **/
bool nth_child_is_integer_below(NodeReporter reporter, size_t n, int64_t max);

/**
 * Report error if @a nth child is not number literal or above @a min.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Which child should be a number literal.
 * @param[in] min      Minimum valid value.
 * @return true iff validation succeeded.
 **/
bool nth_child_is_integer_above(NodeReporter reporter, size_t n, int64_t min);

/**
 * Report error if @a nth child is not float literal.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Which child should be a float literal.
 * @return true iff validation succeeded.
 **/
bool nth_child_is_float(NodeReporter reporter, size_t n);

/**
 * Report error if @a nth child is not a null.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Which child should be a null.
 * @return true iff validation succeeded.
 **/
bool nth_child_is_null(NodeReporter reporter, size_t n);

/**
 * Report error if @a nth child is a null.
 *
 * @param[in] reporter Reporter to use.
 * @param[in] n        Which child should not be a null.
 * @return true iff validation succeeded.
 **/
bool nth_child_is_not_null(NodeReporter reporter, size_t n);

/**
 * Report error if any child is literal.
 *
 * @param[in] reporter Reporter to use.
 * @return true iff validation succeeded.
 **/
bool no_child_is_literal(NodeReporter reporter);

/**
 * Report error if any child is null.
 *
 * @param[in] reporter Reporter to use.
 * @return true iff validation succeeded.
 **/
bool no_child_is_null(NodeReporter reporter);

/**
 * Report error if @a value is not @a type.
 *
 * @param[in] reporter Reporter to use.
 * @return true iff validation succeeded.
 **/
bool value_is_type(Value v, Value::type_e type, NodeReporter reporter);

} // Validate
} // Predicate
} // IronBee

#endif
