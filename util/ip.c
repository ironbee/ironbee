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
 * @brief IronBee --- IP Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @nosubgrouping
 */

#include "ironbee_config_auto.h"

#include <ironbee/ip.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>

ib_status_t ib_ip4_str_to_ip(
    const char *s,
    ib_ip4_t   *ip
)
{
    if (s == NULL) {
        return IB_EINVAL;
    }

    ib_ip4_t local_ip;
    if (ip == NULL) {
        ip = &local_ip;
    }
    int r = 0;
    r = inet_pton(AF_INET, s, ip);
    if (r != 1) {
        return IB_EINVAL;
    }
    *ip = htonl(*ip);

    return IB_OK;
}

ib_status_t ib_ip4_str_to_net(
    const char       *s,
    ib_ip4_network_t *net
)
{
    ib_status_t rc = IB_OK;
    ib_ip4_network_t local_net;


    if (s == NULL) {
        return IB_EINVAL;
    }
    if (net == NULL) {
        net = &local_net;
    }

    char buffer[18];
    const char *slash = strchr(s, '/');

    if (slash == NULL || (slash - s) > 17) {
        return IB_EINVAL;
    }

    memcpy(buffer, s, slash - s);
    buffer[slash - s] = '\0';

    rc = ib_ip4_str_to_ip(buffer, &(net->ip));
    if (rc != IB_OK) {
        return IB_EINVAL;
    }

    int size = 0;
    int consumed = 0;
    int r = sscanf(slash, "/%d%n", &size, &consumed);
    if (
        r != 1 ||
        size < 0 || size > 32 ||
        slash[consumed] != '\0'
    ) {
        return IB_EINVAL;
    }

    net->size = size;

    return IB_OK;
}

ib_status_t ib_ip6_str_to_ip(
    const char *s,
    ib_ip6_t   *ip
)
{
    if (s == NULL) {
        return IB_EINVAL;
    }

    ib_ip6_t local_ip;
    if (ip == NULL) {
        ip = &local_ip;
    }
    int r = inet_pton(AF_INET6, s, ip);
    if (r != 1) {
        return IB_EINVAL;
    }
    for (int i = 0; i < 4; ++i) {
        ip->ip[i] = ntohl(ip->ip[i]);
    }

    return IB_OK;
}


ib_status_t ib_ip6_str_to_net(
    const char       *s,
    ib_ip6_network_t *net
)
{
    ib_status_t rc = IB_OK;
    ib_ip6_network_t local_net;

    if (s == NULL) {
        return IB_EINVAL;
    }
    if (net == NULL) {
        net = &local_net;
    }

    char buffer[40];
    const char *slash = strchr(s, '/');

    if (slash == NULL || (slash - s) > 40) {
        return IB_EINVAL;
    }

    strncpy(buffer, s, slash - s);
    buffer[slash-s] = '\0';

    rc = ib_ip6_str_to_ip(buffer, &(net->ip));
    if (rc != IB_OK) {
        return IB_EINVAL;
    }

    int size = 0;
    int consumed = 0;
    int r = sscanf(slash, "/%d%n", &size, &consumed);
    if (
        r != 1 ||
        size < 0 || size > 128 ||
        slash[consumed] != '\0'
    ) {
        return IB_EINVAL;
    }

    net->size = size;

    return IB_OK;
}

ib_status_t ib_ip_validate_ex(
    const char *s,
    size_t      len
)
{
    char buffer[40];

    if (len >= 40) {
        return IB_EINVAL;
    }

    strncpy(buffer, s, len);
    buffer[len] = '\0';

    return ib_ip_validate(buffer);
}

ib_status_t ib_ip_validate(
    const char *s
)
{
    const char *colon = NULL;
    const char *period = NULL;

    colon = strchr(s, ':');
    if (colon == NULL) {
        return ib_ip4_str_to_ip(s, NULL);
    }
    else {
        period = strchr(s, '.');
        if (period != NULL && period < colon) {
            return IB_EINVAL;
        }
        return ib_ip6_str_to_ip(s, NULL);
    }
}
