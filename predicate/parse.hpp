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
 * @brief Predicate --- Parse expression.
 *
 * Defines parse_call() and parse_literal().
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef __PREDICATE__PARSE__
#define __PREDICATE__PARSE__

#include <predicate/call_factory.hpp>

namespace IronBee {
namespace Predicate {

/**
 * Parse a literal.
 *
 * Text has the following grammar:
 * @code
 * literal    := "'" ( [^\'] | \\ | \' )* "'" | null
 * @endcode
 *
 * @param [in]      text Text to parse.
 * @param [in, out] i    Index of first character to parse; will be updated
 *                       to index of last character parsed.  Will be
 *                       length - 1 on complete parse.
 * @return node_p corresponding to parsed @a call.
 * @throw IronBee::einval on parse error.
 **/
node_p parse_literal(
    const std::string& text,
    size_t&            i
);

/**
 * Parse a call.
 *
 * Text has the following grammar:
 * @code
 * call       := " "* "(" name ( " "* + expression )* ")"
 * expression := call | literal
 * literal    := null | string | float | integer
 * null       := 'null'
 * string     := '\'' + *(/[^'\\]/ | '\\\\' | '\\'') + '\''
 * integer    := /^-?[0-9]+$/
 * float      := /^-?[0-9]+(\.[0-9]+)?$/
 * name       := [-_A-Za-z0-9]+
 * @endcode
 *
 * @param [in]           text    Text to parse.
 * @param [in, out] i    Index of first character to parse; will be updated
 *                       to index of last character parsed.  Will be
 *                       length - 1 on complete parse.
 * @param [in]           factory CallFactory to use to generate call nodes.
 * @return node_p corresponding to parsed @a call.
 * @throw IronBee::enoent if contains a name not in @a factory.
 * @throw IronBee::einval on parse error.
 **/
node_p parse_call(
    const std::string& text,
    size_t&            i,
    const CallFactory& factory
);

} // Predicate
} // IronBee

#endif
