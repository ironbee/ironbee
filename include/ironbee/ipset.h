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

#ifndef _IB_IPSET_H_
#define _IB_IPSET_H_

/**
 * @file
 * @brief IronBee &mdash; IP Set Utility Functions
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/ip.h>
#include <ironbee/types.h>

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilIPSet IP Set
 * @ingroup IronBeeUtil
 *
 * IP set routines.
 *
 * An IP network is an IP addresses and network mask (here limited to initial
 * sequences of 1s followed by 0s).  An IP set is a subset of the total IP
 * space defined by a collection of positive networks and negative networks.
 * An IP is then in the set if it is not in any negative network and is in
 * at least one positive network.
 *
 * IP set allows for querying membership; retrieving the most general and most
 * specific positive networks containing the IP.  It also supports associating
 * arbitrary (@c void*) data with each positive network.
 *
 * The limitation that network masks are limited to initial sequences of 1s
 * matches standard practice (e.g., CIDR) blocks and is dramatically simpler
 * to implement than a general solution allowing arbitrary netmasks.
 *
 * IP sets are *static*, that is, addition and deletion are not supported.
 * The entire contents, positive and negative, must be provided at creation.
 *
 * Query is \f$O(\log P + \log N)\f$ for finding an entry and
 * \f$O(\log P + \log N + K)\f$ for finding all matching entries where N is
 * the number of negative networks, P is the number of positive networks, and
 * K is the number of matching positive entries.
 *
 * The API is divided into v4 and v6 versions.  Besides the number of bytes in
 * the network address, the semantics are identical.
 *
 * @{
 */

/**
 * An IPSet4 entry.
 */
typedef struct ib_ipset4_entry_t ib_ipset4_entry_t;
struct ib_ipset4_entry_t
{
    /**
     * Network.
     */
    ib_ip4_network_t network;

    /**
     * Associated data.
     */
    void *data;
};

/**
 * An IPSet4.  Opaque datastructure.
 *
 * A set of IPv4 addresses defined by a collection of positive and negative
 * networks.
 *
 * @sa ib_ipset4_init()
 * @sa ib_ipset4_query()
 */
typedef struct ib_ipset4_t ib_ipset4_t;

/**
 * An IPSet6 entry.
 */
typedef struct ib_ipset6_entry_t ib_ipset6_entry_t;
struct ib_ipset6_entry_t
{
    /**
     * Network.
     */
    ib_ip6_network_t network;

    /**
     * Associated data.
     */
    void *data;
};

/**
 * An IPSet4.  Opaque datastructure.
 *
 * A set of IPv6 addresses defined by a collection of positive and negative
 * networks.
 *
 * @sa ib_ipset6_init()
 * @sa ib_ipset6_query()
 */
typedef struct ib_ipset6_t ib_ipset6_t;

/** @cond internal */

/**
 * IP Set of IPv4 addresses.
 *
 * The set is represented is a collection of positive and negative networks
 * (see ib_ipset4_init()).  A set is then nothing more than the arrays of
 * those networks.  As part of creation, those networks are made canonical
 * (i.e., @c ip @c & @c mask @c == @c ip) and sorted (see
 * ib_ipset4_compare_strict()).
 *
 * Why not patricia tries?  The problem addressed by this code is often
 * solved using patricia tries.  In the current context, patricia tries would
 * give similar space and time complexity (note that if we assume the number
 * of networks, N, used to specify the set is less than the size of the
 * IP space, then \f$\log(N) < log(2^{32}) = 32\f$, i.e., the key length).
 * Patricia tries would also allow for rapid insertion and removal which is
 * impossible with using sorted arrays.  However, the latter is not needed,
 * patricia tries are more complicated, and often use more time and space.
 *
 * Why two arrays instead of a single array of both positive and negative
 * entries?  A single array would change the runtime complexity from
 * \f$\log(N)+\log(P) = \log(NP)\f$ to \f$\log(N+P)+K'\f$ where \f$K'\f$ is
 * the number of matching positive and negative networks, with the expense of
 * higher space complexity of (at least) one byte per entry.  Assuming
 * \f$K'\f$ is small, the single array implementation would be faster.  At
 * the time of this writing, the space savings and simplicity were deemed the
 * more valuable.  However, it may change if experience suggests otherwise.
 *
 * Why linear search for specific/general entries instead of further binary
 * search?  Once an entry is found with bsearch(), a linear search is
 * conducted of adjacent entries to find the most specific and most general
 * entry.  In contrast, binary search could have been used to find the
 * least upper bound and greatest lower bound, changing the runtime complexity
 * from \f$O(\log(N)+\log(P)+K)\f$ to \f$O(\log(N)+\log(P))\f$.  However,
 * the standard library does not provide least upper bound/greatest lower
 * bound routines, so these would have to be implemented.  As \f$K\f$ is
 * expected to be low, the code complexity was deemed too expensive.
 */
struct ib_ipset4_t
{
    ib_ipset4_entry_t *positive;
    size_t             num_positive;
    ib_ipset4_entry_t *negative;
    size_t             num_negative;
};

/**
 * As above, but for IPv6.
 *
 * @sa ib_ipset6_t.
 */
struct ib_ipset6_t
{
    ib_ipset6_entry_t *positive;
    size_t             num_positive;
    ib_ipset6_entry_t *negative;
    size_t             num_negative;
};

/** @endcond */
/**
 * Initialize an IPv4 set.
 *
 * Initialize a new IP set defined defined by @a negative and @a positive,
 * each an array of ib_ipset4_entry_t.
 *
 * The parameters @a negative and @a positive are *claimed* by @a set as part
 * of creation.  They should be treated as opaque and read-only after this
 * call.  They must survive at least as long as @a set.
 *
 * This function does no allocations.
 *
 * Let N be the number of negative networks in @a set, P be the number of
 * positive entries in @a set.  Then, the runtime of this function is
 * \f$O(N \log N + P \log P)\f$.
 *
 * @param[out]    set          Set to initialize.
 * @param[in,out] negative     Negative networks of IP set.  Claimed by @a set
 *                             as part of creation.
 * @param[in]     num_negative Number of entries in @a negative.
 * @param[in,out] positive     Negative networks of IP set.  Claimed by @a set
 *                             as part of creation.
 * @param[in]     num_positive Number of entries in @a positive.
 *
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if @a set == NULL.
 * - IB_EINVAL if @a negative == NULL and @a num_negative > 0.
 * - IB_EINVAL if @a positive == NULL and @a num_positive > 0.
 * - IB_EOTHER if sorting failed; see errno for code.
 */
ib_status_t ib_ipset4_init(
    ib_ipset4_t       *set,
    ib_ipset4_entry_t *negative,
    size_t             num_negative,
    ib_ipset4_entry_t *positive,
    size_t             num_positive
);

/**
 * Query @a set for @a ip.
 *
 * Query if @a ip is not contained in any negative networks of @a set and
 * is contained in at least one positive network of @a set.
 *
 * All entries can be retrieved by treating [@a *out_general_entry, @a
 * *out_specific_entry] as an array of ib_ipset4_entry_t.
 *
 * This function makes no heap allocations.
 *
 * Let N be the number of negative networks in @a set, P be the number of
 * positive entries in @a set, and K be the number of matching entries.  Then
 * runtime is:
 *
 * - \f$O(\log N)\f$ if K == 0
 * - \f$O(\log N + \log P)\f$ if K > 0, @a out_specific_entry == NULL, and
 *   @a out_general_entry == NULL.
 * - \f$O(\log N + \log P + K)\f$ if K > 0 and either @a out_specific_entry or
 *   @a out_general_entry is not NULL.
 *
 * @param[in]  set                IP set to query.
 * @param[in]  ip                 IP to query.
 * @param[out] out_entry          If non-NULL and IP is in @a set, will be
 *                                set to a containing entry.
 * @param[out] out_specific_entry If non-NULL and IP is in @a set, will be
 *                                set to most specific containing entry.
 * @param[out] out_general_entry  If non-NULL and IP is in @a set, will be
 *                                set to most general containing entry.
 *
 * @return
 * - IB_OK if @a ip is in @a set.
 * - IB_ENOENT if @a ip is not in @a set.
 * - IB_EINVAL if @a set is NULL.
 */
ib_status_t ib_ipset4_query(
    const ib_ipset4_t        *set,
    ib_ip4_t                  ip,
    const ib_ipset4_entry_t **out_entry,
    const ib_ipset4_entry_t **out_specific_entry,
    const ib_ipset4_entry_t **out_general_entry
);

/**
 * As ib_ipset4_init() except for v6 addresses.
 *
 * See ib_ipset4_init() for documentation.
 *
 * @sa ib_ipset4_init()
 */
ib_status_t ib_ipset6_init(
    ib_ipset6_t       *set,
    ib_ipset6_entry_t *negative,
    size_t             num_negative,
    ib_ipset6_entry_t *positive,
    size_t             num_positive
);

/**
 * As ib_ipset4_query() except for v6 addresses.
 *
 * See ib_ipset4_query() for documentation.
 *
 * @sa ib_ipset4_query()
 */
ib_status_t ib_ipset6_query(
    const ib_ipset6_t        *set,
    ib_ip6_t                  ip,
    const ib_ipset6_entry_t **out_entry,
    const ib_ipset6_entry_t **out_specific_entry,
    const ib_ipset6_entry_t **out_general_entry
);

/** @} IronBeeUtilIPSet */

#ifdef __cplusplus
}
#endif

#endif /* _IB_IPSET_H_ */
