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
 *****************************************************************************/

/**
 * @file
 * @brief IronBee &mdash; IP Set Implementation
 *
 * See ib_ipset4_t for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @nosubgrouping
 */

#include "ironbee_config_auto.h"

#include <ironbee/ipset.h>
#include <ironbee/debug.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

/**
 * Helper typedef of a stdlib compare function.
 */
typedef int (*ib_ipset_compare_fn)(const void *, const void *);

/**
 * The mask \f$1^{bits}0^{32-bits}\f$.
 *
 * @param[in] bits Number of 1s.
 * @return \f$1^{bits}0^{32-bits}\f$ as a uint32_t.
 */
static
uint32_t ib_ipset4_mask(size_t bits)
{
    IB_FTRACE_INIT();

    /* Special case. */
    if (bits >= 32) {
        IB_FTRACE_RET_UINT(0xffffffff);
    }
    IB_FTRACE_RET_UINT(~(0xffffffff >> bits));
}

/**
 * Return canonical address of @a net. (v4 version)
 *
 * @param[in] net Network to check for canonicalness.
 * @return Address of @a net with all bits outside of mask set to 0.
 */
static
ib_ip4_t ib_ipset4_canonical(
    ib_ip4_network_t net
)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_UINT(
        net.ip & ib_ipset4_mask(net.size)
    );
}

/**
 * Return canonical address of @a net. (v6 version)
 *
 * @param[in] net Network to check for canonicalness.
 * @return Address of @a net with all bits outside of mask set to 0.
 */
static
ib_ip6_t ib_ipset6_canonical(
    ib_ip6_network_t net
)
{
    ib_ip6_t ip = {{0, 0, 0, 0}};

    size_t initial_bytes = net.size / 32;
    size_t initial_bits  = net.size % 32;

    for (size_t i = 0; i < initial_bytes; ++i) {
        ip.ip[i] = net.ip.ip[i];
    }
    ip.ip[initial_bytes] =
        net.ip.ip[initial_bytes] & ib_ipset4_mask(initial_bits);
    return ip;
}

/**
 * True iff @a a_net is a prefix of @a b_net (v4 version).
 *
 * @param[in] a_net Network to check if prefix.
 * @param[in] b_net Network to check if prefix of.
 * @return true iff @a a_net is a prefix of @a b_net.
 */
static
bool ib_ipset4_is_prefix(
    ib_ip4_network_t a_net,
    ib_ip4_network_t b_net
)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_BOOL(
        (b_net.ip & ib_ipset4_mask(a_net.size)) ==
        (a_net.ip & ib_ipset4_mask(a_net.size))
    );
}

/**
 * True iff @a a_net is a prefix of @a b_net (v6 version).
 *
 * @param[in] a_net Network to check if prefix.
 * @param[in] b_net Network to check if prefix of.
 * @return true iff @a a_net is a prefix of @a b_net.
 */
static
bool ib_ipset6_is_prefix(
    const ib_ip6_network_t a_net,
    const ib_ip6_network_t b_net
)
{
    IB_FTRACE_INIT();

    int initial_bytes  = a_net.size / 32;
    int remaining_bits = a_net.size % 32;

    for (int i = 0; i < initial_bytes; ++i) {
        if (a_net.ip.ip[i] != b_net.ip.ip[i]) {
            IB_FTRACE_RET_BOOL(false);
        }
    }

    IB_FTRACE_RET_BOOL(
        (a_net.ip.ip[initial_bytes] & ib_ipset4_mask(remaining_bits)) ==
        (b_net.ip.ip[initial_bytes] & ib_ipset4_mask(remaining_bits))
    );
}

/**
 * Comparison function for ib_ip4_network_t (strict).
 *
 * Networks are treated as bit strings of length @c size.  They are then
 * ordered lexicographically with tied broken by length (longer is greater).
 *
 * Assumes @a a and @a b are pointers to ib_ip4_network_t in canonical
 * form.
 *
 * @param[in] a LHS.
 * @param[in] b RHS.
 * @return
 * - 0  -- Same address and mask.
 * - -1 -- @a a has earlier address or same address and shorter length.
 * - 1  -- @a a has later address or same address and longer length.
 */
static
int ib_ipset4_compare_strict(
  const void *a,
  const void *b
)
{
    IB_FTRACE_INIT();

    const ib_ip4_network_t *a_net = (const ib_ip4_network_t *)a;
    const ib_ip4_network_t *b_net = (const ib_ip4_network_t *)b;

    assert(a_net->ip == ib_ipset4_canonical(*a_net));
    assert(b_net->ip == ib_ipset4_canonical(*b_net));

    if (a_net->ip == b_net->ip) {
        if (a_net->size < b_net->size) {
            IB_FTRACE_RET_INT(-1);
        }
        if (a_net->size > b_net->size) {
            IB_FTRACE_RET_INT(-1);
        }
        IB_FTRACE_RET_INT(0);
    }

    if (a_net->ip < b_net->ip) {
        IB_FTRACE_RET_INT(-1);
    }
    IB_FTRACE_RET_INT(1);
}

/**
 * Comparison function for ib_ip4_network_t.
 *
 * This function is as ib_ipset4_compare_strict() except that it considers
 * elements to be equal if either is a prefix of the other.
 *
 * Assumes @a a and @a b are pointers to ib_ip4_network_t in canonical
 * form.
 *
 * @param[in] a LHS.
 * @param[in] b RHS.
 * @return
 * - 0  -- Same address and mask, or @a a is a prefix of @a b, or @a b is a
 *         prefix of @a a.
 * - -1 -- @a a has earlier address or same address and shorter length.
 * - 1  -- @a a has later address or same address and longer length.
 */
static
int ib_ipset4_compare(
    const void *a,
    const void *b
)
{
    IB_FTRACE_INIT();

    const ib_ip4_network_t *a_net = (const ib_ip4_network_t *)a;
    const ib_ip4_network_t *b_net = (const ib_ip4_network_t *)b;

    assert(a_net->ip == ib_ipset4_canonical(*a_net));
    assert(b_net->ip == ib_ipset4_canonical(*b_net));

    if (
        ib_ipset4_is_prefix(*a_net, *b_net) ||
        ib_ipset4_is_prefix(*b_net, *a_net)
    ) {
        IB_FTRACE_RET_INT(0);
    }

    IB_FTRACE_RET_INT(ib_ipset4_compare_strict(a, b));
}

/**
 * Comparison function for ib_ip6_network_t (strict).
 *
 * As ib_ipset4_compare_strict() but for v6 networks.
 *
 *Assumes @a a and @a b are pointers to ib_ip6_network_t in canonical
 * form.
 *
 * @sa ib_ipset4_compare_strict()
 *
 * @param[in] a LHS.
 * @param[in] b RHS.
 * @return
 * - 0  -- Same address and mask.
 * - -1 -- @a a has earlier address or same address and shorter length.
 * - 1  -- @a a has later address or same address and longer length.
 */
static
int ib_ipset6_compare_strict(
    const void *a,
    const void *b
)
{
    IB_FTRACE_INIT();

    const ib_ip6_network_t *a_net = (const ib_ip6_network_t *)a;
    const ib_ip6_network_t *b_net = (const ib_ip6_network_t *)b;

    for (int i = 0; i < 4; ++i) {
        if (a_net->ip.ip[i] < b_net->ip.ip[i]) {
            IB_FTRACE_RET_INT(-1);
        }
        else if (a_net->ip.ip[i] > b_net->ip.ip[i]) {
            IB_FTRACE_RET_INT(1);
        }
    }

    if (a_net->size < b_net->size) {
        IB_FTRACE_RET_INT(-1);
    }
    if (a_net->size > b_net->size) {
        IB_FTRACE_RET_INT(1);
    }

    IB_FTRACE_RET_INT(0);
}

/**
 * Comparison function for ib_ip6_network_t.
 *
 * This function is as ib_ipset6_compare_strict() except that it considers
 * elements to be equal if either is a prefix of the other.
 *
 * Assumes @a a and @a b are pointers to ib_ip6_network_t in canonical
 * form.
 *
 * @sa ib_ipset6_compare()
 *
 * @param[in] a LHS.
 * @param[in] b RHS.
 * @return
 * - 0  -- Same address and mask, or @a a is a prefix of @a b, or @a b is a
 *         prefix of @a a.
 * - -1 -- @a a has earlier address or same address and shorter length.
 * - 1  -- @a a has later address or same address and longer length.
 */
static
int ib_ipset6_compare(
    const void *a,
    const void *b
)
{
    IB_FTRACE_INIT();

    const ib_ip6_network_t *a_net = (const ib_ip6_network_t *)a;
    const ib_ip6_network_t *b_net = (const ib_ip6_network_t *)b;

    if (
        ib_ipset6_is_prefix(*a_net, *b_net) ||
        ib_ipset6_is_prefix(*b_net, *a_net)
    ) {
        IB_FTRACE_RET_INT(0);
    }

    IB_FTRACE_RET_INT(ib_ipset6_compare_strict(a, b));
}

/**
 * Generic query routine for an entry of entries.
 *
 * This is a generic function intended for searching for v4 and v6 arrays of
 * entries for a specific entry.
 *
 * @param[in]  net         Pointer to a network describing a single IP,
 *                         i.e., size is max.  Note: This is not validated.
 * @param[in]  entries     Entries to search.
 * @param[in]  num_entries Number of entries in @a entries.
 * @param[in]  entry_size  Size of each entry and of @a net.
 * @param[in]  compare     Comparison function to use.
 * @param[out] out_entry   Output variable for a matching entry, if found.
 *                         May be NULL.
 *
 * @return
 * - IB_OK if an entry is found.
 * - IB_ENOENT if an entry is not found.
 * - IB_EINVAL if @a net is NULL, or @a entries is NULL with
 *   @a num_entries > 0, or @a entry_size is 0, or @a compare is NULL.
 */
static
ib_status_t ib_ipset_set_query(
    const void           *net,
    const void           *entries,
    size_t                num_entries,
    size_t                entry_size,
    ib_ipset_compare_fn   compare,
    const void          **out_entry
)
{
    IB_FTRACE_INIT();

    if (out_entry != NULL) {
        *out_entry = NULL;
    }

    if (
        net == NULL ||
        compare == NULL ||
        (entries == NULL && num_entries > 0) ||
        (entry_size == 0)
    ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (num_entries == 0) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    void *result = bsearch(
        net,
        entries,
        num_entries,
        entry_size,
        compare
    );

    if (result == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }
    if (out_entry != NULL) {
        *out_entry = result;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Generic query routine for an IP Set.
 *
 * This takes an IP Set, either v4 or v6, and looks for an entry, and
 * optionally, the most specific and general entries.
 *
 * @param[in]  network               Pointer to a network describing a single
 *                                   IP, i.e., size is max.  Note: This is
 *                                   not validated.
 * @param[in]  negative              Negative networks to search.
 * @param[in]  num_negative          Number of entries in @a negative.
 * @param[in]  positive              Positive networks to search.
 * @param[in]  num_positive          Number of entries in @a positive.
 * @param[in]  entry_size            Size of each entry and of @a net.
 * @param[in]  compare               Comparison function to use.
 * @param[out] out_entry             Output variable for a matching entry, if
 *                                   found.  May be NULL.
 * @param[out] out_specific_entry    Output variable for most specific
 *                                   matching entry, if found.  May be NULL.
 * @param[out] out_general_entry     Output variable for most general
 *                                   matching entry, if found.  May be NULL.
 * @return
 * - IB_OK if an entry is found.
 * - IB_ENOENT if an entry is not found.
 * - IB_EINVAL if @a net is NULL, or @a negative is NULL with
 *   @a num_negative > 0, or same for positive entries, or @a entry_size is 0,
 *   or @a compare is NULL.
 */
static
ib_status_t ib_ipset_query(
    const void           *network,
    const void           *negative,
    size_t                num_negative,
    const void           *positive,
    size_t                num_positive,
    size_t                entry_size,
    ib_ipset_compare_fn   compare,
    const void          **out_entry,
    const void          **out_specific_entry,
    const void          **out_general_entry
)
{
    IB_FTRACE_INIT();

    const void *entry = NULL;

    if (out_entry != NULL) {
        *out_entry = NULL;
    }
    if (out_specific_entry != NULL) {
        *out_specific_entry = NULL;
    }
    if (out_general_entry != NULL) {
        *out_general_entry = NULL;
    }

    if (
        network == NULL ||
        compare == NULL ||
        (negative == NULL && num_negative > 0) ||
        (positive == NULL && num_positive > 0) ||
        (entry_size == 0)
    ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_status_t rc = ib_ipset_set_query(
        network,
        negative,
        num_negative,
        entry_size,
        compare,
        NULL
    );
    if (rc != IB_ENOENT) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    rc = ib_ipset_set_query(
        network,
        positive,
        num_positive,
        entry_size,
        compare,
        &entry
    );

    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    if (out_entry != NULL) {
        *out_entry = entry;
    }
    if (out_specific_entry != NULL) {
        *out_specific_entry = entry;
        while (
            (char *)*out_specific_entry <
                ((char *)positive + entry_size * (num_positive - 1)) &&
            compare(((char *)*out_specific_entry + entry_size), network) == 0
        )
        {
            *out_specific_entry = (char *)*out_specific_entry + entry_size;
        }
    }

    if (out_general_entry != NULL) {
        *out_general_entry = entry;
        while (
            *out_general_entry > positive &&
            compare(((char *)*out_general_entry - entry_size), network) == 0
        )
        {
            *out_general_entry = (char *)*out_general_entry - entry_size;
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Public API */

ib_status_t ib_ipset4_query(
    const ib_ipset4_t        *set,
    ib_ip4_t                  ip,
    const ib_ipset4_entry_t **out_entry,
    const ib_ipset4_entry_t **out_specific_entry,
    const ib_ipset4_entry_t **out_general_entry
)
{
    IB_FTRACE_INIT();

    ib_ip4_network_t net = {ip, 32};

    if (set == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(
        ib_ipset_query(
            &net,
            set->negative,
            set->num_negative,
            set->positive,
            set->num_positive,
            sizeof(ib_ipset4_entry_t),
            &ib_ipset4_compare,
            (const void **)out_entry,
            (const void **)out_specific_entry,
            (const void **)out_general_entry
        )
    );
}

ib_status_t ib_ipset6_query(
    const ib_ipset6_t        *set,
    ib_ip6_t                  ip,
    const ib_ipset6_entry_t **out_entry,
    const ib_ipset6_entry_t **out_specific_entry,
    const ib_ipset6_entry_t **out_general_entry
)
{
    IB_FTRACE_INIT();

    ib_ip6_network_t net = {ip, 128};

    if (set == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(
        ib_ipset_query(
            &net,
            set->negative,
            set->num_negative,
            set->positive,
            set->num_positive,
            sizeof(ib_ipset6_entry_t),
            &ib_ipset6_compare,
            (const void **)out_entry,
            (const void **)out_specific_entry,
            (const void **)out_general_entry
        )
    );
}

ib_status_t ib_ipset4_init(
    ib_ipset4_t       *set,
    ib_ipset4_entry_t *negative,
    size_t             num_negative,
    ib_ipset4_entry_t *positive,
    size_t             num_positive
)
{
    IB_FTRACE_INIT();

    if (
        set == NULL ||
        (negative == NULL && num_negative > 0) ||
        (positive == NULL && num_positive > 0)
    ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    set->negative     = negative;
    set->num_negative = num_negative;
    set->positive     = positive;
    set->num_positive = num_positive;

    for (size_t i = 0; i < set->num_negative; ++i) {
        set->negative[i].network.ip =
             ib_ipset4_canonical(set->negative[i].network);
    }
    for (size_t i = 0; i < set->num_positive; ++i) {
        set->positive[i].network.ip =
             ib_ipset4_canonical(set->positive[i].network);
    }

    qsort(
        set->negative,
        set->num_negative,
        sizeof(ib_ipset4_entry_t),
        &ib_ipset4_compare_strict
    );

    qsort(
        set->positive,
        set->num_positive,
        sizeof(ib_ipset4_entry_t),
        &ib_ipset4_compare_strict
    );

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_ipset6_init(
    ib_ipset6_t       *set,
    ib_ipset6_entry_t *negative,
    size_t             num_negative,
    ib_ipset6_entry_t *positive,
    size_t             num_positive
)
{
    IB_FTRACE_INIT();

    if (
        set == NULL ||
        (negative == NULL && num_negative > 0) ||
        (positive == NULL && num_positive > 0)
    ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    set->negative     = negative;
    set->num_negative = num_negative;
    set->positive     = positive;
    set->num_positive = num_positive;

    for (size_t i = 0; i < set->num_negative; ++i) {
        set->negative[i].network.ip =
             ib_ipset6_canonical(set->negative[i].network);
    }
    for (size_t i = 0; i < set->num_positive; ++i) {
        set->positive[i].network.ip =
             ib_ipset6_canonical(set->positive[i].network);
    }
    qsort(
        set->negative,
        set->num_negative,
        sizeof(ib_ipset6_entry_t),
        &ib_ipset6_compare_strict
    );

    qsort(
        set->positive,
        set->num_positive,
        sizeof(ib_ipset6_entry_t),
        &ib_ipset6_compare_strict
    );

    IB_FTRACE_RET_STATUS(IB_OK);
}
