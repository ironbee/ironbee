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
 * @brief Predicate --- C API.
 *
 * Defines a C interface to some predicate functions.
 *
 * The intent is to drive analysis of predicate in various other
 * languages through FFIs.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#ifndef __PREDICATE__C__API__
#define __PREDICATE__C__API__

#include <ironbee/types.h>
#include <ironbee/mm.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void ib_predicate_node_t;

/**
 * Parse an expression and put it in the given node.
 *
 * @param[out] node The parsed node.
 * @param[in] expression The expression to parse.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL On a parse error.
 * - IB_EALLOC On allocation errors.
 */
ib_status_t DLL_PUBLIC ib_predicate_parse(
    ib_predicate_node_t **node,
    const char           *expression
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Destroy a node, and all child nodes.
 */
void DLL_PUBLIC ib_predicate_node_destroy(
    ib_predicate_node_t *node
) NONNULL_ATTRIBUTE(1);

/**
 * Return the string representation of this node.
 */
const char DLL_PUBLIC * ib_predicate_node_to_s(
    ib_predicate_node_t *node
) NONNULL_ATTRIBUTE(1);

/**
 * Return the number of child nodes.
 */
size_t DLL_PUBLIC ib_predicate_node_child_count(
    ib_predicate_node_t *node
) NONNULL_ATTRIBUTE(1);

/**
 * Return the name of the node.
 */
const char DLL_PUBLIC * ib_predicate_node_name(
    ib_predicate_node_t *node
) NONNULL_ATTRIBUTE(1);

/**
 * Return child node i or NULL if none.
 *
 * @param[in] i The child index to retrieve.
 * @param[out] children An array that can hold all the children pointers.
 *             Do not destroy the child pointers, they are owned by @a node.
 */
void DLL_PUBLIC ib_predicate_node_children(
    ib_predicate_node_t  *node,
    ib_predicate_node_t **children
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Return if the given node is a literal or not.
 */
bool DLL_PUBLIC ib_predicate_node_is_literal(
    ib_predicate_node_t *node
) NONNULL_ATTRIBUTE(1);

#ifdef __cplusplus
}
#endif

#endif // __PREDICATE__C__API__
