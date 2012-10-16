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
 * of aho_corasick_add_length() and aho_corasick_add_data() and finished with
 * a single call to aho_corasick_finish().
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
