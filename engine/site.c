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

#include "engine_private.h"

#include <ironbee/context_selection.h>
#include <ironbee/engine.h>
#include <ironbee/hash.h>
#include <ironbee/ip.h>
#include <ironbee/mm.h>
#include <ironbee/string.h>

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
    assert(ctx != NULL);
    assert(ctx->ctype == IB_CTYPE_SITE);
    assert(name != NULL);

    ib_mm_t mm = ctx->mm;

    if (psite != NULL) {
        *psite = NULL;
    }

    /* Create the main structure in the config memory pool */
    if (site == NULL) {
        site = (ib_site_t *)ib_mm_calloc(mm, 1, sizeof(*site));
        if (site == NULL) {
            return IB_EALLOC;
        }
    }
    site->mm = mm;
    site->context = ctx;
    site->ctxsel_site = ctxsel_site;
    site->name = ib_mm_strdup(mm, name);
    if (site->name == NULL) {
        return IB_EALLOC;
    }

    if (psite != NULL) {
        *psite = site;
    }

    return IB_OK;
}

ib_status_t ib_site_host_create(
    const ib_site_t *site,
    const char *hostname,
    void *ctxsel_host,
    ib_site_host_t *host,
    ib_site_host_t **phost)
{
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
        return IB_EINVAL;
    }

    /* Create a host object */
    if (host == NULL) {
        host = ib_mm_alloc(site->mm, sizeof(*host));
        if (host == NULL) {
            return IB_EALLOC;
        }
    }
    host->hostname = ib_mm_strdup(site->mm, hostname);
    if (host->hostname == NULL) {
        return IB_EALLOC;
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
    return IB_OK;
}

ib_status_t ib_site_service_create(
    const ib_site_t *site,
    const char *service_str,
    void *ctxsel_service,
    ib_site_service_t *service,
    ib_site_service_t **pservice)
{
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
                return rc;
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
        service = ib_mm_alloc(site->mm, sizeof(*service));
        if (service == NULL) {
            return IB_EALLOC;
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
            return rc;
        }

        service->ipstr = ib_mm_memdup_to_str(site->mm, service_str, ip_len);
        if (service->ipstr == NULL) {
            return IB_EALLOC;
        }
    }
    service->ctxsel_service = ctxsel_service;
    service->site = site;

    if (pservice != NULL) {
        *pservice = service;
    }

    return IB_OK;
}

ib_status_t ib_site_location_create(
    const ib_site_t *site,
    ib_context_t *ctx,
    const char *path,
    void *ctxsel_location,
    ib_site_location_t *location,
    ib_site_location_t **plocation)
{
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
            ib_mm_alloc(site->mm, sizeof(*location));
        if (location == NULL) {
            return IB_EALLOC;
        }
    }
    location->site = site;
    location->path = ib_mm_strdup(site->mm, path);
    location->context = ctx;
    location->ctxsel_location = ctxsel_location;

    if (plocation != NULL) {
        *plocation = location;
    }

    return IB_OK;
}

ib_status_t ib_site_close(
    ib_site_t *site)
{
    assert(site != NULL);

    return IB_OK;
}
