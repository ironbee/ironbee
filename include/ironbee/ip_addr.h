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

#ifndef _IB_IP_ADDR_H_
#define _IB_IP_ADDR_H_

/**
 * @file
 * @brief IronBee &mdash; IP Address Utility Functions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>
#include <ironbee/string.h>
#include <ironbee/mpool.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilIpAddr IpAddr
 * @ingroup IronBeeUtil
 *
 * @{
 */

/** @} IronBeeUtilIpAddr */

/**
 * Create a binary representation (in_addr) of IP, allocating mem from mp
 *
 * @param[in] ip ascii representation
 * @param[in] mp pool where we should allocate the in_addr
 *
 * @returns struct in_addr*
 */
struct in_addr DLL_PUBLIC *ib_ipaddr_get_IPV4(const char *ip,
                                              ib_mpool_t *mp);

/**
 * Create a binary representation (in6_addr) of IP, allocating mem from mp
 *
 * @param[in] ip ascii representation
 * @param[in] mp pool where we should allocate in6_addr
 *
 * @returns struct in6_addr*
 */
struct in6_addr DLL_PUBLIC *ib_ipaddr_get_IPV6(const char *ip,
                                               ib_mpool_t *mp);

/**
 * Determine if a bytestring looks like a CIDR IPV4 address.
 *
 * @param[in] str String to examine
 * @param[in] len Length of @a str
 * @param[in] slash Allow slash notation?
 * @param[out] result true if @a str looks like an IPV4 address, else false.
 *
 * @returns IB_OK if successful
 *          Status code from ib_strchr_nul_error()
 */
ib_status_t DLL_PUBLIC ib_ipaddr_is_ipv4_ex(const char *str,
                                            size_t len,
                                            bool slash,
                                            bool *result);

/**
 * Determine if a bytestring looks like a CIDR IPV6 address.
 *
 * @param[in] str String to examine
 * @param[in] len Length of @a str
 * @param[in] slash Allow slash notation?
 * @param[out] result true if @a str looks like an IPV6 address, else false.
 *
 * @returns IB_OK if successful
 *          Status code from ib_strchr_nul_error()
 */
ib_status_t DLL_PUBLIC ib_ipaddr_is_ipv6_ex(const char *str,
                                            size_t len,
                                            bool slash,
                                            bool *result);

/**
 * Determine if a bytestring looks like a CIDR IPV4/6 address.
 *
 * @param str String to examine
 * @param len Length of @a str
 * @param[in] slash Allow slash notation?
 * @param ipv4 If not NULL: true if @a str looks like an IPV4 addr, else false.
 * @param ipv6 If not NULL: true if @a str looks like an IPV6 addr, else false.
 *
 * @returns IB_OK if successful and address is a valid ipv4/6 address
 *          IB_EOTHER if address is neither an IPV4 / IPV6 address
 *          Status code from ib_strchr_nul_error()
 */
ib_status_t DLL_PUBLIC ib_ipaddr_is_ip_ex(const char *str,
                                          size_t len,
                                          bool slash,
                                          bool *ipv4,
                                          bool *ipv6);

/**
 * Return if the given prefix is IPV4
 *
 * @param[in] cidr const char * with format ip/mask where mask is optional
 * @returns 1 if true, 0 if false
 */
#define IB_IPADDR_IS_IPV4(cidr) ((strchr(cidr, ':') == NULL) ? 1 : 0)

/**
 * Return if the given prefix is IPV6
 *
 * @param[in] cidr const char * with format ip/mask where mask is optional
 * @returns 1 if true, 0 if false
 */
#define IB_IPADDR_IS_IPV6(cidr) ((strchr(cidr, ':') != NULL) ? 1 : 0)

/**
 * Determine if the given prefix is IPV4
 *
 * @param[in] cidr const char * with format ip/mask where mask is optional
 * @param[in] len length of the str
 * @param[in] slash Allow slash notation?
 * @param[out] result Result: true / false
 *
 * @returns Status code
 */
#define IB_IPADDR_IS_IPV4_EX(cidr,len,slash,result)     \
    ib_ipaddr_is_ipv4_ex((cidr), (len), slash, &result)

/**
 * Determine if the given prefix is IPV6
 *
 * @param[in] cidr const char * with format ip/mask where mask is optional
 * @param[in] len length of the str
 * @param[in] slash Allow slash notation?
 * @param[out] result Result: true / false
 *
 * @returns Status code
 */
#define IB_IPADDR_IS_IPV6_EX(cidr,len,slash,result)     \
    ib_ipaddr_is_ipv6_ex((cidr), (len), slash, &result)


#ifdef __cplusplus
}
#endif

#endif /* _IB_IP_ADDR_H_ */
