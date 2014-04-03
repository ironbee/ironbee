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
 * Parse a literal to a Literal node.
 *
 * Text has the following grammar:
 * @code
 * name            := first_name_char name_char*
 * first_name_char := [A-Za-z_]
 * name_char       := first_name_char | [.-]
 * literal         := null | named_literal | literal_value
 * null            := ':'
 * named_literal   := literal_name ':' literal_value
 * literal_name    := string | name
 * literal_value   := list | string | float | integer
 * string          := '\'' (/[^'\\]/ | '\\\\' | '\\'')* '\''
 * integer         := '-'? [0-9]+
 * float           := '-'? [0-9]+ ('.' [0-9]+)?
 * list            := '[' ( literal ( ' '+ literal )* )? ']'
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
 * Parse a literal to a Value.
 *
 * See parse_literal() for grammar.
 *
 * @param[in]      text Text to parse.
 * @param[in, out] i    Index to advance.
 * @param[in]      mm   Memory manager to allocate Value from.
 * @returns Value
 **/
Value parse_literal_value(
    const std::string& text,
    size_t&            i,
    MemoryManager      mm
);

/**
 * Parse a call.
 *
 * Text has the grammar of parse_literal() plus:
 * @code
 * expression      := call | literal
 * call            := ' '* '(' name ( ' '+ expression )* ')'
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

/**
 * Emit a literal name.
 *
 * If @a name is a valid `name` (see parse_literal()), then return it.  Else,
 * emit it as an escaped `string`.
 *
 * @param[in] name Name of literal.
 * @return @a name properly encoded for parsing by parse_literal().
 */
std::string emit_literal_name(const std::string& name);

/**
 * Escape @a text as per `string` (see parse_literal()).
 *
 * Adds backslashes before any single quotes or backslashes in @a text.
 *
 * @param[in] text Text to escape.
 * @return @a text properly escaped for parsing by parse_literal().
 **/
std::string emit_escaped_string(const std::string& text);

} // Predicate
} // IronBee

#endif
