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
 * @brief IronAutomata &mdash; Eudoxus DFA Engine Preprocessor Metacode
 *
 * @warning Preprocessor metacode.  Will not compile directly.  See
 * eudoxus_subengine.c for discussion.
 *
 * The node structures make extensive use of variable length and optional
 * fields.  See vls.h for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifndef IA_EUDOXUS
#error "IA_EUDOXUS not defined.  Do not include this file directly."
#endif
#ifndef IA_EUDOXUS_ID_T
#error "IA_EUDOXUS_ID_T not defined.  Do not include this file directly."
#endif

/**
 * @addtogroup IronAutomataEudoxusAutomata
 *
 * @{
 */

typedef struct IA_EUDOXUS(output_t) IA_EUDOXUS(output_t);
struct IA_EUDOXUS(output_t)
{
    uint32_t        output_length;
    IA_EUDOXUS_ID_T next_output;
    const char      output[];
} __attribute((packed));

/* Low Degree Nodes */

typedef struct IA_EUDOXUS(low_edge_t) IA_EUDOXUS(low_edge_t);
struct IA_EUDOXUS(low_edge_t)
{
    uint8_t         c;
    IA_EUDOXUS_ID_T next_node;
} __attribute((packed));

typedef struct IA_EUDOXUS(low_node_t) IA_EUDOXUS(low_node_t);
struct IA_EUDOXUS(low_node_t)
{
    /*
     * type: 00
     * flag0: has_output
     * flag1: has_nonadvancing -- edges only; not including default.
     * flag2: has_default
     * flag3: advance_on_default
     * flag4: has_edges
     */
    uint8_t header;

    /* variable: */

    /*
     * First output.  Important that this is the first variable entry so it
     * matches other (future) node types.
     */
    /*
    IA_EUDOXUS_ID_T first_output          if has_output
    */

    /*
     * Number of edges, not including default.
     *
     * I.e., the size of advance and edges.
     */
    /*
    uint8_t out_degree if has_edges
    */

    /*
    IA_EUDOXUS_ID_T default_node          if has_defaults
    uint8_t         advance[out_degree/8] if has_nonadvancing & has_edges
    low_edge_t      edges[]
    */
} __attribute((packed));

/**
 * Eudoxus High Degree Node
 *
 * High Degree nodes provide faster lookup and more compact representations
 * than low degree nodes by using bitmaps.  These bitmaps have a fixed
 * cost (32 bytes) regardless of how many edges they describe.
 *
 * Targets are stored in a final, variable length, table consisting solely of
 * IDs.  For any given input, the index in the targets table is calculated and
 * the target accessed directly.
 *
 * The targets table uses a form of run-length-encoding to further compress
 * it.  Ranges of identical entries can be compressed into a single entry.
 * This is recorded in an Advance-Lookup-Index (ALI) bitmap.  The ALI bitmap
 * itself uses 32 bytes, so, when few or no ranges exist, it may be cheaper
 * to include them in the targets table and omit the ALI table.
 *
 * If not every input has an entry in the targets table, then a bitmap is
 * needed to record this.  This is called the target bitmap.
 *
 * The target and ALI bitmaps have several possible interactions:
 * - If not all inputs have targets and there are many ranges, then both
 *   the target and ALI bitmaps are included.  The target bitmap determines
 *   whether an input is in the targets table and the ALI bitmap is used to
 *   calculate the table index.
 * - If not all inputs have targets but there are few or no ranges, then
 *   only the target bitmap is included.  It is used both to determine whether
 *   an input is in the the targets table and to determine its index.  This
 *   is possible, because there is a one-to-one correspondence between
 *   inputs and entries in the targets table.
 * - If every input is present and there are many ranges, then only the ALI
 *   bitmap is included.  It is used to lookup the index in the targets table
 *   of each input.
 * - If every input is present and there are few ranges, then neither the
 *   targets nor the ALI bitmap is included.  In this case, the targets table
 *   has 256 entries and the index for input @c c is @c c.
 *
 * <table>
 * <tr>
 *   <th>has_target_bm</th>
 *   <th>has_ali_bm</th>
 *   <th>degree</th>
 *   <th>has target of <i>c</i></th>
 *   <th>target index of <i>c</i></th>
 * </tr>
 * <tr><td>true</td><td>true</td>
 *    <td>popcount(target_bm)</td>
 *    <td>target_bm[<i>c</i>]</td>
 *    <td>popcount(ali_bm, c)</td>
 * </tr>
 * <tr><td>true</td><td>false</td>
 *    <td>popcount(target_bm)</td>
 *    <td>target_bm[<i>c</i>]</td>
 *    <td>popcount(target_bm, <i>c</i>)</td>
 * </tr>
 * <tr><td>false</td><td>true</td>
 *    <td>256</td>
 *    <td>true</td>
 *    <td>popcount(ali_bm, <i>c</i>)</td>
 * </tr>
 * <tr><td>false</td><td>false</td>
 *    <td>256</td>
 *    <td>true</td>
 *    <td><i>c</i></td>
 * </tr>
 * </table>
 */
typedef struct IA_EUDOXUS(high_node_t) IA_EUDOXUS(high_node_t);
struct IA_EUDOXUS(high_node_t)
{
    /*
     * type: 01
     * flag0: has_output
     * flag1: has_nonadvancing -- edges only; not including default
     * flag2: has_default
     * flag3: advance_on_default
     * flag4: has_target_bm
     * flag5: has_ali_bm
     */
    uint8_t header;

     /* variable:
     IA_EUDOXUS_ID_T first_output if has_output
     IA_EUDOXUS_ID_T default_node if has_default
     ia_bitmap256_t  advance_bm   if has_nonadvancing
     ia_bitmap256_t  target_bm    if has_targets
     ia_bitmap256_t  ali_bm       if has_ali
     IA_EUDOXUS_ID_T targets[]
     */
} __attribute((packed));

/**
 * Eudoxus Path Compression (PC) Node
 *
 * Path Compression nodes represent simple paths through the automata.  I.e.,
 * a chain of nodes that have a single entrance, single advancing non-default
 * exit, no outputs after initial node, and identical defaults.  A PC node
 * will emit outputs while entered, absorb input tokens as long as they match
 * the path, and continue on to the target (if path is fully matched) or
 * default (if ever not matched).
 */
typedef struct IA_EUDOXUS(pc_node_t) IA_EUDOXUS(pc_node_t);
struct IA_EUDOXUS(pc_node_t)
{
    /*
     * type: 10
     * flag0: has_output
     * flag1: has_default
     * flag2: advance_on_default
     * flag3: advance_on_final
     * flag4+flag5+flag6: length:
     *   000: 2
     *   001: 3
     *   010: 4
     *   011: 5
     *   100: 6
     *   101: 7
     *   110: 8
     *   111: use long_length field
     */
    uint8_t header;

    IA_EUDOXUS_ID_T final_target;

    /* variable:
    IA_EUDOXUS_ID_T first_output if has_output
    IA_EUDOXUS_ID_T default_node if has_default
    uint8_t long_length if length == 111
    uint8_t bytes[];
    */
} __attribute((packed));

/** @} IronAutomataEudoxusAutomata */

#ifdef __cplusplus
}

namespace IronAutomata {
namespace Eudoxus {

/**
 * Subengine Traits.
 */
template <>
struct subengine_traits<sizeof(IA_EUDOXUS_ID_T)>
{
    typedef IA_EUDOXUS_ID_T         id_t;
    typedef IA_EUDOXUS(output_t)    output_t;
    typedef IA_EUDOXUS(low_edge_t)  low_edge_t;
    typedef IA_EUDOXUS(low_node_t)  low_node_t;
    typedef IA_EUDOXUS(high_node_t) high_node_t;
    typedef IA_EUDOXUS(pc_node_t)   pc_node_t;
};

} // Eudoxus
} // IronAutomata

extern "C" {
#endif
