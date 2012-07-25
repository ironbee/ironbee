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

/**
 * @file
 * @brief IronBee &mdash; IP Utility Types and Functions
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>

#include <stdint.h>

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

/**
 * Convert a string of the form a.b.c.d to an ib_ip4_t.
 *
 * @param[in]  s  String to convert.
 * @param[out] ip IP address corresponding to s.  Can be NULL if only want to
 *                validate @a s.
 * @returns
 * - IB_OK on success.
 * - IB_EINVAL if @a s is not a proper IP address.
 */
ib_status_t DLL_PUBLIC ib_ip4_str_to_ip(
    const char *s,
    ib_ip4_t   *ip
 );

/**
 * Convert a string of the form a.b.c.d/mask to an ib_ip4_network_t.
 *
 * @param[in]  s   String to convert.
 * @param[out] net Network corresponding to s.  Can be NULL if only want to
 *                 validate @a s.
 * @returns
 * - IB_OK on success.
 * - IB_EINVAL if @a s is not a proper network.
 */
ib_status_t DLL_PUBLIC ib_ip4_str_to_net(
    const char       *s,
    ib_ip4_network_t *net
);

/** @} IronBeeUtilIP */

#ifdef __cplusplus
}
#endif

#endif /* _IB_IPSET_H_ */
