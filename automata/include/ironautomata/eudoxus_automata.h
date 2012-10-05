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

#ifndef _IA_EUDOXUS_AUTOMATA_H_
#define _IA_EUDOXUS_AUTOMATA_H_

/**
 * @file
 * @brief IronAutomata &mdash; Eudoxus Internals
 *
 * This header file defines internal data structures and methods for Eudoxus
 * automata.  It is needed by the execution engine and the compiler but does
 * not provide public API for Eudoxus users.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup IronAutomataEudoxusAutomata Automata
 * @ingroup IronAutomataEudoxus
 *
 * Details of Eudoxus Automata layout.
 *
 * This module is primarily of interest to developers of the Eudoxus engine or
 * compiler.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Current automata version.
 *
 * This is checked by @c ia_eudoxus_create_ methods to insure that an automata
 * was generated for the current engine.
 */
#define IA_EUDOXUS_VERSION 3

/**
 * A Eudoxus Automata.
 *
 * Note that the in memory representation of an automata exactly matches
 * the on disk representation.  That is, loading an automata is as simple
 * as reading data into memory.
 */
typedef struct ia_eudoxus_automata_t ia_eudoxus_automata_t;
struct ia_eudoxus_automata_t
{
    /**
     * What Eudoxus version was this automata generated for.
     */
    uint8_t version;

    /**
     * Width of all ids in bytes.
     *
     * This member is used to select the proper subengine.
     */
    uint8_t id_width;

    /**
     * Does the rest of the automata use big endian?
     *
     * If the endianness of the automata does not match that of the host
     * automata, the engine will reject it.  At some point, routines to
     * convert may be provided.
     */
    int is_big_endian : 1;

    /**
     * If true, suppress output for targets of non-advancing edges.
     */
    int no_advance_no_output : 1;

    /**
     * More flags for future versions.  Currently, only valid value is 0.
     */
    int reserved : 6;

    /** @name Non-execution members.
     * These members are not used during execution.  They are provided to
     * assist in validation and conversion.
     *
     * @{
     */
    /**
     * Number of nodes in automata.
     *
     * Note that @c num_nodes + @c num_outputs is the number of distinct
     * ids.
     */
    uint64_t num_nodes;

    /**
     * Number of outputs in automata.
     */
    uint64_t num_outputs;

    /**
     * Number of bytes defining automata after end of this structure.
     */
    uint64_t data_length;

    /**
     * Index of first (i.e., start) node.  At most 256 bytes in.
     */
    uint8_t start_index;

    /** @} */

    /**
     * Remaining bytes of automata.
     *
     * Note this is not a pointer stored in the structure but rather
     * can be used to refer to the bytes of memory stored after the
     * structure.  This is a common compiler extension.
     */
    char     data[];
} __attribute((packed));

/**
 * Node Types.
 *
 * Eudoxus makes use of a variety node types to efficiently represent
 * automata.  This enum lists them and provides the values used in the
 * ia_eudoxus_node_header_t.
 */
enum ia_eudoxus_nodetype_t
{
    /**
     * Low Degree Node.
     *
     * A low degree node stores its edges in a vector and uses linear search
     * to determine which edge to follow.
     */
    IA_EUDOXUS_LOW = 0
};
typedef enum ia_eudoxus_nodetype_t ia_eudoxus_nodetype_t;

/**
 * Header of every node type.
 *
 * Every node in automata must start with a @c header member of this type.
 */
typedef struct ia_eudoxus_node_header_t ia_eudoxus_node_header_t;
struct ia_eudoxus_node_header_t
{
    /**
     * Type of node: See ia_eudoxus_nodetype_t.
     *
     * More types coming in the future.
     */
    int type  : 3;

    /**
     * Node type specific flags.
     */
    int flags : 5;
} __attribute((packed));

/**
 * A generic node.
 *
 * This type can be used to refer to a generic node irregardless of type.
 * Pointers of this type must be cast to a more specific type in order to
 * access type specific data.
 */
typedef struct ia_eudoxus_node_t ia_eudoxus_node_t;
struct ia_eudoxus_node_t
{
    /**
     * Header.
     */
    ia_eudoxus_node_header_t header;
} __attribute((packed));

/**
 * Return 1 iff currently big endian.
 *
 * @return 1 if currently on a big endian system and 0 otherwise.
 */
int ia_eudoxus_is_big_endian(void);

#ifdef __cplusplus
}

namespace IronAutomata {
namespace Eudoxus {

/**
 * Subengine Traits.
 *
 * This traits class will be specialized for each subengine.  The
 * specializations will define:
 * - @c id_t
 * - @c output_t
 * - @c low_node_t
 * - @c low_edge_t
 */
template <size_t id_width>
struct subengine_traits
{
    // See specializations in eudoxus_subautomata.h
};

} // Eudoxus
} // IronAutomata

extern "C" {
#endif

/* The remaining code is preprocessor metaprogramming.  It varies two symbols,
 * IA_EUDOXUS which is used to prefix all remaining types and methods
 * and IA_EUDOXUS_ID_T which is used to determine the width of identifiers.
 * These are defined and then eudoxus_subautomata.h is included multiple
 * times, thus generating the declarations for different subengines for
 * different id widths.
 */

#define IA_EUDOXUS(a) ia_eudoxus8_ ## a
#define IA_EUDOXUS_ID_T   uint64_t
#include <ironautomata/eudoxus_subautomata.h>
#undef IA_EUDOXUS
#undef IA_EUDOXUS_ID_T

#define IA_EUDOXUS(a) ia_eudoxus4_ ## a
#define IA_EUDOXUS_ID_T   uint32_t
#include <ironautomata/eudoxus_subautomata.h>
#undef IA_EUDOXUS
#undef IA_EUDOXUS_ID_T

#define IA_EUDOXUS(a) ia_eudoxus2_ ## a
#define IA_EUDOXUS_ID_T   uint16_t
#include <ironautomata/eudoxus_subautomata.h>
#undef IA_EUDOXUS
#undef IA_EUDOXUS_ID_T

#define IA_EUDOXUS(a) ia_eudoxus1_ ## a
#define IA_EUDOXUS_ID_T   uint8_t
#include <ironautomata/eudoxus_subautomata.h>
#undef IA_EUDOXUS
#undef IA_EUDOXUS_ID_T

#ifdef __cplusplus
}
#endif

/**
 * @} IronAutomataEudoxusAutomata
 */

#endif /* _IA_EUDOXUS_AUTOMATA_H_ */
