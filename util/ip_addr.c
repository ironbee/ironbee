/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.    See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.    You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * @file
 * @author Nick LeRoy <nleroy@qualys.com>
 * @brief IronBee &mdash; Utility IP address functions
 *
 */

#include "ironbee_config_auto.h"

#include <ironbee/debug.h>
#include <ironbee/ip_addr.h>
#include <ironbee/mpool.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h> /* For FreeBSD */
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

const size_t MIN_IPV4_STR = 7;   /* 1.2.3.4 */
const size_t MAX_IPV4_STR = 15;  /* 255.255.255.255 */
const size_t MAX_IPV6_STR = 39;

/*
 * Create a binary representation (in_addr) of IP, allocating mem from mp
 */
struct in_addr *ib_ipaddr_get_IPV4(const char *ip,
                                   ib_mpool_t *mp)
{
    IB_FTRACE_INIT();
    struct in_addr *rawbytes = NULL;

    rawbytes = (struct in_addr *)ib_mpool_calloc(mp, 1, sizeof(struct in_addr));
    if (rawbytes == NULL) {
        IB_FTRACE_RET_PTR(struct in_addr, NULL);
    }

    if (inet_pton(AF_INET, ip, rawbytes) <= 0) {
        IB_FTRACE_RET_PTR(struct in_addr, NULL);
    }

    IB_FTRACE_RET_PTR(struct in_addr, rawbytes);
}

/*
 * Create a binary representation (in6_addr) of IP, allocating mem from mp
 */
struct in6_addr *ib_ipaddr_get_IPV6(const char *ip,
                                    ib_mpool_t *mp)
{
    IB_FTRACE_INIT();
    struct in6_addr *rawbytes = NULL;

    rawbytes = (struct in6_addr *)
        ib_mpool_calloc(mp, 1, sizeof(struct in6_addr));
    if (rawbytes == NULL) {
        IB_FTRACE_RET_PTR(struct in6_addr, NULL);
    }

    if (inet_pton(AF_INET6, ip, rawbytes) <= 0) {
        IB_FTRACE_RET_PTR(struct in6_addr, NULL);
    }

    IB_FTRACE_RET_PTR(struct in6_addr, rawbytes);
}

/*
 * Determine if a bytestring looks like a CIDR IPV4 address.
 */
ib_status_t ib_ipaddr_is_ipv4_ex(const char *str,
                                 size_t len,
                                 bool slash,
                                 bool *result)
{
    IB_FTRACE_INIT();

    assert(str != NULL);
    assert(result != NULL);

    ib_status_t rc;
    ssize_t offset;
    struct in_addr addr;
    char buf[MAX_IPV4_STR + 1];

    *result = false;

    if (len < MIN_IPV4_STR) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_strchr_nul_error(str, len, '/', &offset);
    if ( (rc == IB_OK) && (offset > 1) ) {
        if (!slash) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        len = offset;
    }
    if (len > MAX_IPV4_STR) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Look for a colon: if it has one, not an IPv4 */
    rc = ib_strchr_nul_error(str, len, ':', &offset);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    if (offset >= 0) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    memcpy(buf, str, len);
    buf[len] = '\0';
    if (inet_pton(AF_INET, buf, &addr) > 0) {
        *result = true;
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/*
 * Determine if a bytestring looks like a CIDR IPV6 address.
 */
ib_status_t ib_ipaddr_is_ipv6_ex(const char *str,
                                 size_t len,
                                 bool slash,
                                 bool *result)
{
    IB_FTRACE_INIT();

    assert(str != NULL);
    assert(result != NULL);

    ib_status_t rc;
    ssize_t offset;
    struct in6_addr addr;
    char buf[MAX_IPV6_STR + 1];

    *result = false;

    rc = ib_strchr_nul_error(str, len, '/', &offset);
    if ( (rc == IB_OK) && (offset > 1) ) {
        if (!slash) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        len = offset;
    }
    if (len > MAX_IPV6_STR) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Look for a colon: if it doesn't have one, not an IPv6 */
    rc = ib_strchr_nul_error(str, len, ':', &offset);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    if (offset <= 0) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    memcpy(buf, str, len);
    buf[len] = '\0';
    if (inet_pton(AF_INET6, buf, &addr) > 0) {
        *result = true;
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/*
 * Determine if a bytestring looks like a CIDR IPV4/6 address.
 */
ib_status_t ib_ipaddr_is_ip_ex(const char *str,
                               size_t len,
                               bool slash,
                               bool *ipv4,
                               bool *ipv6)
{
    IB_FTRACE_INIT();

    assert(str != NULL);

    ib_status_t rc4;
    ib_status_t rc6;
    bool result;

    /* Initialize non-NULL results to false */
    if (ipv4 != NULL) {
        *ipv4 = false;
    }
    if (ipv6 != NULL) {
        *ipv6 = false;
    }

    /* Check if it's an IPv4 address */
    rc4 = ib_ipaddr_is_ipv4_ex(str, len, slash, &result);
    if ( (rc4 == IB_OK) && (result) ) {
        if (ipv4 != NULL) {
            *ipv4 = true;
        }
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Check if it's an IPv6 address */
    rc6 = ib_ipaddr_is_ipv6_ex(str, len, slash, &result);
    if ( (rc6 == IB_OK) && (result) ) {
        if (ipv6 != NULL) {
            *ipv6 = true;
        }
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Check for errors */
    if (rc4 != IB_OK) {
        IB_FTRACE_RET_STATUS(rc4);
    }
    else if (rc6 != IB_OK) {
        IB_FTRACE_RET_STATUS(rc6);
    }
    else {
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }
}
