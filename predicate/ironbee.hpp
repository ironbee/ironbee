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
 * @brief Predicate --- Fundamental connections to IronBee.
 *
 * Defines the base hierarchy for DAG nodes.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__IRONBEE__
#define __PREDICATE__IRONBEE__

#include <ironbeepp/engine.hpp>
#include <ironbeepp/field.hpp>
#include <ironbeepp/transaction.hpp>

namespace IronBee {
namespace Predicate {

/*
 * The following typedefs are the primary parameters for tying the public
 * API to IronBee.  If this code was to be used in a different system, much
 * of the implementation, especially of standard.hpp, would need to change,
 * but changing these typedefs should suffice for adapating the
 * public API.
 *
 * The types are not Const because IronBee requires non-const versions for
 * a variety of purposes, e.g., operators and adding fields to capture
 * collections.  However, they should be treated as non-mutable when possible.
 */

/**
 * Value of a node.
 **/
typedef IronBee::Field Value;

/**
 * Context a node is evaluated in.
 **/
typedef IronBee::Transaction EvalContext;

/**
 * Environment of a node.
 **/
typedef IronBee::Engine Environment;

} // Predicate
} // IronBee

#endif

