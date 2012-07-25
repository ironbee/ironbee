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
 * @brief IronBee &mdash; IP Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @nosubgrouping
 */

#include "ironbee_config_auto.h"

#include <ironbee/ip.h>
#include <ironbee/debug.h>

#include <stdio.h>

/* Internal */

/** True iff @a x is in [0, 255] */
#define IB_IP4_VALID_OCTET(x) ((x) >= 0 && (x) <= 255)

/**
 * As ib_ip4_str_to_ip() but does not require all of @a s to be consumed.
 *
 * @param[in] s         String to parse.
 * @param[out] ip       IP at beginning of @a s.
 * @param[out] consumed Number of bytes of @a s consumed.
 * @returns As ib_ip4_str_to_ip().
 */
ib_status_t ib_ip4_str_to_ip_helper(
    const char *s,
    ib_ip4_t   *ip,
    int        *consumed
)
{
    IB_FTRACE_INIT();

    int r = 0;
    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;

    if (s == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    r = sscanf(s, "%d.%d.%d.%d%n", &a, &b, &c, &d, consumed);
    if (
        r != 4 ||
        ! IB_IP4_VALID_OCTET(a) ||
        ! IB_IP4_VALID_OCTET(b) ||
        ! IB_IP4_VALID_OCTET(c) ||
        ! IB_IP4_VALID_OCTET(d)
    ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (ip != NULL) {
        *ip = (a << 24) + (b << 16) + (c << 8) + d;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* End Internal */

ib_status_t ib_ip4_str_to_ip(
    const char *s,
    ib_ip4_t   *ip
)
{
    IB_FTRACE_INIT();

    int consumed = 0;
    ib_status_t rc = ib_ip4_str_to_ip_helper(s, ip, &consumed);
    if (rc != IB_OK || s[consumed] != '\0') {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_ip4_str_to_net(
    const char       *s,
    ib_ip4_network_t *net
)
{
    IB_FTRACE_INIT();

    int         consumed  = 0;
    int         consumed2 = 0;
    ib_status_t rc        = IB_OK;
    int         r         = 0;
    int         size      = 0;
    ib_ip4_t    ip        = 0;

    rc = ib_ip4_str_to_ip_helper(s, &ip, &consumed);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    r = sscanf(s + consumed, "/%d%n", &size, &consumed2);
    if (
        r != 1 ||
        size < 0 || size > 32 ||
        s[consumed + consumed2] != '\0'
    ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (net != NULL) {
        net->ip   = ip;
        net->size = size;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
