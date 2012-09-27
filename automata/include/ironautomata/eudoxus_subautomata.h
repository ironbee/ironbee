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
     * type: 000
     * flag0: has_output
     * flag1: has_nonadvancing -- edges only; not including default.
     * flag2: has_default
     * flag3: advance_on_default
     */
    ia_eudoxus_node_header_t header;

    /**
     * Number of edges, not including default.
     *
     * I.e., the size of advance and edges.
     */
    uint8_t         out_degree;

    /* variable:
    IA_EUDOXUS_ID_T first_output          if has_output
    IA_EUDOXUS_ID_T default_node          if has_defaults
    uint8_t         advance[out_degree/8] if has_nonadvancing
    low_edge_t      edges[]
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
    typedef IA_EUDOXUS_ID_T        id_t;
    typedef IA_EUDOXUS(output_t)   output_t;
    typedef IA_EUDOXUS(low_edge_t) low_edge_t;
    typedef IA_EUDOXUS(low_node_t) low_node_t;
};

} // Eudoxus
} // IronAutomata

extern "C" {
#endif
