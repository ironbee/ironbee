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

#ifndef _IB_IP_H_
#define _IB_IP_H_

#include <stdint.h>

/**
 * @file
 * @brief IronBee &mdash; IP Utility Types and Functions
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilIP IP
 * @ingroup IronBeeUtil
 *
 * Types and function related to IP addresses.
 *
 * @{
 */

/**
 * An IPv4 address.
 */
typedef uint32_t ib_ip4_t;

/**
 * An IPv4 network.
 */
typedef struct ib_ip4_network_t ib_ip4_network_t;
struct ib_ip4_network_t
{
    /**
     * Network address.
     *
     * All bits except the initial @c mask will be treated as zero.
     **/
    ib_ip4_t ip;

    /**
     * Network mask as the number of initial 1s.
     *
     * The actual mask is \f$1^{size}0^{32-size}\f$.
     **/
    uint8_t size;
};

/**
 * An IPv6 address.
 */
typedef struct ib_ip6_t ib_ip6_t;
struct ib_ip6_t
{
    /** IP as four 32 bit words. */
    uint32_t ip[4];
};

/**
 * An IPv6 network.
 */
typedef struct ib_ip6_network_t ib_ip6_network_t;
struct ib_ip6_network_t
{
    /**
     * Network address.
     *
     * All bits except the initial @c mask will be treated as zero.
     **/
    ib_ip6_t ip;

    /**
     * Network mask as the number of initial 1s.
     *
     * The actual mask is \f$1^{size}0^{128-size}\f$.
     **/
    uint8_t size;
};

/** @} IronBeeUtilIP */

#ifdef __cplusplus
}
#endif

#endif /* _IB_IPSET_H_ */
