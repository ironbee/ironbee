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
 * Which version of Eudoxus this compiler targets.
 */
extern const int EUDOXUS_VERSION;

/**
 * Compiler configuration.
 */
struct configuration_t
{
    /**
     * Constructor; sets reasonable defaults.
     *
     * - id_width = 0, i.e., minimal.
     * - align_to = 1, i.e., no alignment
     * - high_node_weight = 1.0, i.e., optimize space
     */
    configuration_t();

    /**
     * With of identifiers.  0 = minimal, 1, 2, 4, 8.
     *
     * How wide to make every identifier in the automata.  Minimal value is
     * given by size of automata in bytes (which is influenced by id_width):
     *
     * <table>
     * <tr><th>id_width</th><th>Maximum Automata Size</th></tr>
     * <tr><td>1</td><td>256</td></tr>
     * <tr><td>2</td><td>65KB</td></tr>
     * <tr><td>4</td><td>4GB</td></tr>
     * <tr><td>8</td><td>16EB</td></tr>
     * </table>
     *
     * A value of 0 will cause compile to choose the minimal id_width.  This
     * does increase compilation time.
     */
    size_t id_width;

    /**
     * Align indices of node objects to this value.
     *
     * This can be used to cause the compiler to insert padding so that the
     * indices of all node objects are aligned to this value.  Assuming the
     * automata is loaded into memory at a similarly aligned address, this
     * will provide alignment for every node which may give performance
     * benefits at the cost of space.
     *
     * A value of 1 indicates no alignment.  Other likely useful values are 4
     * and 8.
     */
    size_t align_to;

    /**
     * High Node Weight
     *
     * This multiplier adjusts the weight of high nodes.  A value of 1
     * maximizes space; a value less than 1 will allow a sacrifice of space to
     * prefer high nodes; a a value greater than 1 will allow a sacrifice of
     * space to prefer low nodes.  A value of 0 will prevent any low nodes and
     * a value greater than 3000 will prevent any high nodes.
     *
     * Weights are in bytes, so, e.g., a value of 0.9 will allow high nodes to
     * be used if 90% of their space usage is less than the usage of a low
     * node.
     *
     * In practice, values moderately below 1 may provide time performance
     * benefits and small space costs.  Eventually, smaller values will begin
     * penalizing performance as low degree nodes are both smaller and faster
     * for very low degree.
     */
    double high_node_weight;
};

/**
 * Result of a compilation.
 */
struct result_t
{
    //! Compiled automata.
    buffer_t buffer;

    //! Configuration
    configuration_t configuration;

    //! Number of IDs used.
    size_t ids_used;

    //! Number of bytes of padding added.
    size_t padding;

    //! Number of high nodes.
    size_t high_nodes;

    //! Bytes of high nodes.
    size_t high_nodes_bytes;

    //! Number of low nodes.
    size_t low_nodes;

    //! Bytes of low nodes.
    size_t low_nodes_bytes;

    //! Number of PC nodes.
    size_t pc_nodes;

    //! Bytes of PC nodes.
    size_t pc_nodes_bytes;
};

/**
 * Compile automata.
 *
 * @param[in] automata      Automata to compile.
 * @param[in] configuration Compiler configuration.
 * @return Compilation result.
 */
result_t compile(
    const Intermediate::Automata& automata,
    configuration_t               configuration = configuration_t()
);

} // EudoxusCompiler
} // IronAutomata

#endif
