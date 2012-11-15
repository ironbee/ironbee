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
 * @brief IronBee
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/engine.h>
#include "engine_private.h"

#include <ironbee/debug.h>
#include <ironbee/string.h>
#include <ironbee/ip.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>
#include <ironbee/context_selection.h>

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


ib_status_t ib_site_create(
    ib_context_t *ctx,
    const char *name,
    void *ctxsel_site,
    ib_site_t *site,
    ib_site_t **psite)
{
    IB_FTRACE_INIT();
    assert(ctx != NULL);
    assert(ctx->ctype == IB_CTYPE_SITE);
    assert(name != NULL);

    ib_mpool_t *pool = ctx->mp;

    if (psite != NULL) {
        *psite = NULL;
    }

    /* Create the main structure in the config memory pool */
    if (site == NULL) {
        site = (ib_site_t *)ib_mpool_calloc(pool, 1, sizeof(*site));
        if (site == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }
    site->mp = pool;
    site->name = ib_mpool_strdup(pool, name);
    site->context = ctx;
    site->ctxsel_site = ctxsel_site;

    if (psite != NULL) {
        *psite = site;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_site_host_create(
    const ib_site_t *site,
    const char *hostname,
    void *ctxsel_host,
    ib_site_host_t *host,
    ib_site_host_t **phost)
{
    IB_FTRACE_INIT();
    assert(site != NULL);
    assert(hostname != NULL);

    size_t hostlen = strlen(hostname);
    const char *star;
    bool is_wild = false;

    if (phost != NULL) {
        *phost = NULL;
    }

    /* Validate the host name.  Start by finding the right-most '*' */
    star = strrchr(hostname, '*');
    if (star == hostname) {
        if (hostlen != 1) {
            is_wild = true;
        }
    }
    else if (star != NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Create a host object */
    if (host == NULL) {
        host = ib_mpool_alloc(site->mp, sizeof(*host));
        if (host == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }
    host->hostname = ib_mpool_strdup(site->mp, hostname);
    if (host->hostname == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    if (is_wild) {
        host->suffix = host->hostname + 1;
    }
    else {
        host->suffix = NULL;
    }
    host->ctxsel_host = ctxsel_host;
    host->site = site;

    if (phost != NULL) {
        *phost = host;
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_site_service_create(
    const ib_site_t *site,
    const char *service_str,
    void *ctxsel_service,
    ib_site_service_t *service,
    ib_site_service_t **pservice)
{
    IB_FTRACE_INIT();
    assert(site != NULL);
    assert(service_str != NULL);

    ib_status_t rc;
    const char *colon;
    size_t ip_len;
    ib_num_t port;

    /* Find the colon separator & grab the port # */
    colon = strrchr(service_str, ':');
    if (colon == NULL) {
        port = -1;
    }
    else {
        if (strcmp(colon+1, "*") == 0) {
            port = -1;
        }
        else {
            rc = ib_string_to_num(colon+1, 10, &port);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }

    /* Validate the IP address */
    if (colon == NULL) {
        ip_len = strlen(service_str);
    }
    else {
        ip_len = colon - service_str;
    }

    /* Create the service structure */
    if (service == NULL) {
        service = ib_mpool_alloc(site->mp, sizeof(*service));
        if (service == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }

    /* Fill in the port and IP string */
    service->port = port;
    if ( (ip_len == 0) || ((ip_len == 1) && (*service_str == '*')) ) {
        service->ipstr = NULL;
    }
    else {
        rc = ib_ip_validate_ex(service_str, ip_len);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        service->ipstr = ib_mpool_memdup_to_str(site->mp, service_str, ip_len);
        if (service->ipstr == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }
    service->ctxsel_service = ctxsel_service;
    service->site = site;

    if (pservice != NULL) {
        *pservice = service;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_site_location_create(
    const ib_site_t *site,
    ib_context_t *ctx,
    const char *path,
    void *ctxsel_location,
    ib_site_location_t *location,
    ib_site_location_t **plocation)
{
    IB_FTRACE_INIT();
    assert(ctx != NULL);
    assert(ctx->ctype == IB_CTYPE_LOCATION);
    assert(site != NULL);
    assert(path != NULL);

    if (plocation != NULL) {
        *plocation = NULL;
    }

    /* Create the location structure in the site memory pool (if needed) */
    if (location == NULL) {
        location = (ib_site_location_t *)
            ib_mpool_alloc(site->mp, sizeof(*location));
        if (location == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }
    location->site = site;
    location->path = ib_mpool_strdup(site->mp, path);
    location->context = ctx;
    location->ctxsel_location = ctxsel_location;

    if (plocation != NULL) {
        *plocation = location;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_site_close(
    ib_site_t *site)
{
    IB_FTRACE_INIT();
    assert(site != NULL);

    IB_FTRACE_RET_STATUS(IB_OK);
}
