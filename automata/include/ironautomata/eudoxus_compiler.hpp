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

#ifndef _IA_EUDOXUS_COMPILER_
#define _IA_EUDOXUS_COMPILER_

/**
 * @file
 * @brief IronAutomata --- Eudoxus Compiler
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironautomata/buffer.hpp>
#include <ironautomata/intermediate.hpp>

namespace IronAutomata {

/**
 * @namespace IronAutomata::EudoxusCompiler
 * The Eudoxus Compiler library.
 */
namespace EudoxusCompiler {

/**
 * Result of a compilation.
 */
struct result_t
{
    //! Compiled automata.
    buffer_t buffer;

    //! ID width used.
    size_t id_width;

    //! Align to used.
    size_t align_to;

    //! Number of IDs used.
    size_t ids_used;

    //! Number of bytes of padding added.
    size_t padding;
};

/**
 * Compile automata using given id width.
 *
 * @param[in] automata Automata to compile.
 * @param[in] id_width Width of all id fields in bytes.
 * @param[in] align_to Node indices will be padded to be 0 mod @a align_to.
 * @return Compilation result.
 */
result_t compile(
    const Intermediate::Automata& automata,
    size_t                        id_width = 8,
    size_t                        align_to = 1
);

/**
 * Two-pass compiler that minimizes id width.
 *
 * This function behaves as compile() but chooses the smallest possible
 * id width.  It can take significantly longer than compile().
 *
 * @param[in] automata Automata to compile.
 * @param[in] align_to Node indices will be padded to be 0 mod @a align_to.
 * @return Compilation result.
 */
result_t compile_minimal(
    const Intermediate::Automata& automata,
    size_t                        align_to = 1
);

} // EudoxusCompiler
} // IronAutomata

#endif
