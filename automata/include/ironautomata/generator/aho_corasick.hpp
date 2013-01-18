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

#ifndef _IA_GENERATOR_AHO_CORASICK_
#define _IA_GENERATOR_AHO_CORASICK_

/**
 * @file
 * @brief IronAutomata --- Optimized edges of Intermediate Format
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/intermediate.hpp>

namespace IronAutomata {
namespace Generator {

/**
 * Begin Aho-Corasick automata construction.
 *
 * This should be called on an empty automata before any of the other
 * generator routines are called.  It should then be followed by one or more
 * of aho_corasick_add_length(), aho_corasick_add_data(), or
 * aho_corasick_add_pattern() and finished with a single call to
 * aho_corasick_finish().
 *
 * @param[in] automata Automata to begin constructing.
 * @throw invalid_argument if automata is non-empty.
 */
void aho_corasick_begin(
    Intermediate::Automata& automata
);

/**
 * Add a string and length to an Aho-Corasick automata under construction.
 *
 * This adds @a s to the automata @a automata under construction with
 * associated data as the length of @a s as a uint32_t.  This is useful data
 * when all the is desired is to know the substring that matched.  If you
 * wish to associate different data, use aho_corasick_add_data().
 *
 * @param[in] automata Automata under construction.  Must have had
 *                     aho_corasick_begin() called on it.
 * @param[in] s        String to add.
 * @throw invalid_argument if aho_corasick_begin() has not been called.
 */
void aho_corasick_add_length(
    Intermediate::Automata& automata,
    const std::string&      s
);

/**
 * Add a string and data to an Aho-Corasick automata under construction.
 *
 * This adds @a s to the automata @a automata under construction with
 * associated data @a data.
 *
 * @param[in] automata Automata under construction.  Must have had
 *                     aho_corasick_begin() called on it.
 * @param[in] s        String to add.
 * @param[in] data     Data to associate with @a s.
 * @throw invalid_argument if aho_corasick_begin() has not been called.
 */
void aho_corasick_add_data(
    Intermediate::Automata&            automata,
    const std::string&                 s,
    const Intermediate::byte_vector_t& data
);

// Note: Documentation below escapes all backslashes for proper doxygen
// output.  E.g., actual shortcut is \n not &#92;n.
/**
 * Add a pattern and data to an Aho-Corasick automata under construction.
 *
 * Patterns provide a variety of fixed width operators that are shortcuts for
 * a byte or span of bytes.  E.g., "foo\dbar" is a pattern for "foo0bar",
 * "foo1bar", ..., "foo9bar".  Adding "foo\dbar" is semantically equivalent
 * to adding the 10 matching strings via aho_corasick_add_data(), but this
 * method creates better automata.
 *
 * It is possible to mix this routine with the other add routines.  However,
 * any calls to this routine must come after all other add calls.  I.e., add
 * patterns last.
 *
 * Single Shortcuts:
 * - \\\\ -- Backslash.
 * - \\t -- Horizontal tab.
 * - \\v -- Vertical tab.
 * - \\n -- New line
 * - \\r -- Carriage return.
 * - \\f -- Form feed.
 * - \\0 -- Null.
 * - \\e -- Escape.
 *
 * Parameterized Single Shortcuts:
 * - \\^X -- Control character, where X is A-Z, [, \\, ], ^, _, or ?.
 * - \\xXX -- ASCII character XX in hex.
 * - \\iX -- Match lower case of X and upper case of X where X is A-Za-z.
 *
 * Multiple Shortcuts:
 * - \\d -- Digit -- 0-9
 * - \\D -- Non-Digit -- all but 0-9
 * - \\h -- Hexadecimal digit -- A-Fa-f0-9
 * - \\w -- Word Character -- A-Za-z0-9
 * - \\W -- Non-Word Character -- All but A-Za-z0-9
 * - \\a -- Alphabetic character -- A-Za-z
 * - \\l -- Lowercase letters -- a-z
 * - \\u -- Uppercase letters -- A-Z
 * - \\s -- White space -- space, \\t\\r\\n\\v\\f
 * - \\S -- Non-white space -- All but space,
 *             \\t\\r\\n\\v\\f
 * - \\$ -- End of line -- \\r\\f
 * - \\p -- Printable character, ASCII hex 20 through 7E.
 * - \\\. -- Any character.
 *
 * @param[in] automata Initialize automata to add to.
 * @param[in] pattern  Pattern to add.
 * @param[in] data     Data to associate with pattern.
 * @throw invalid_argument if aho_corasick_begin() has not been called.
 */
void aho_corasick_add_pattern(
     Intermediate::Automata&            automata,
     const std::string&                 pattern,
     const Intermediate::byte_vector_t& data
);

/**
 * Complete construction of an Aho-Corasick automata.
 *
 * This routine should be called after aho_corasick_begin() and one or more
 * of aho_corasick_add_length() and aho_corasick_add_data().
 *
 * @note It is legal to call this without adding any strings to the automata.
 *       However, the resulting automata will output nothing.
 *
 * @param[in] automata Automata to finish.
 * @throw invalid_argument if aho_corasick_begin() has not been called.
 */
void aho_corasick_finish(
    Intermediate::Automata& automata
);

} // Generator
} // IronAutomata

#endif
