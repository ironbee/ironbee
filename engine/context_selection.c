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
 * @brief IronBee --- Context Selection Logic
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/context_selection.h>

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif

#include "core_private.h"
#include "engine_private.h"
#include "rule_engine_private.h"

#include <ironbee/mpool.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <assert.h>
#include <inttypes.h>
#include <unistd.h>

bool ib_ctxsel_module_is_active(
    const ib_engine_t *ib,
    const ib_module_t *module)
{
    if (ib == NULL) {
        return false;
    }
    return ib->act_ctxsel.module == module;
}

ib_status_t ib_ctxsel_site_create(
    ib_context_t *ctx,
    const char *name,
    ib_site_t **psite)
{
    assert(ctx != NULL);
    assert(ctx->ctype == IB_CTYPE_SITE);
    assert(name != NULL);

    ib_status_t rc;
    const ib_ctxsel_registration_t *ctxsel = &ctx->ib->act_ctxsel;

    if (ctxsel->site_create_fn == NULL) {
        rc = ib_site_create(ctx, name, NULL, NULL, psite);
    }
    else {
        rc = ctxsel->site_create_fn(ctx, name,
                                    ctxsel->common_cb_data,
                                    ctxsel->site_create_cb_data,
                                    psite);
    }
    return rc;
}

ib_status_t ib_ctxsel_location_create(
    const ib_site_t *site,
    ib_context_t *ctx,
    const char *location_str,
    ib_site_location_t **plocation)
{
    assert(site != NULL);
    assert(location_str != NULL);

    ib_status_t rc;
    const ib_ctxsel_registration_t *ctxsel = &ctx->ib->act_ctxsel;

    if (ctxsel->location_create_fn == NULL) {
        rc = ib_site_location_create(site, ctx, location_str, NULL,
                                     NULL, plocation);
    }
    else {
        rc = ctxsel->location_create_fn(site, ctx, location_str,
                                        ctxsel->common_cb_data,
                                        ctxsel->location_create_cb_data,
                                        plocation);
    }
    return rc;
}

ib_status_t ib_ctxsel_host_create(
    const ib_site_t *site,
    const char *host_str,
    ib_site_host_t **phost)
{
    assert(site != NULL);
    assert(host_str != NULL);

    ib_status_t rc;
    const ib_ctxsel_registration_t *ctxsel = &site->context->ib->act_ctxsel;

    if (ctxsel->host_create_fn == NULL) {
        rc = ib_site_host_create(site, host_str, NULL, NULL, phost);
    }
    else {
        rc = ctxsel->host_create_fn(site, host_str,
                                    ctxsel->common_cb_data,
                                    ctxsel->host_create_cb_data,
                                    phost);
    }
    return rc;
}

ib_status_t ib_ctxsel_service_create(
    const ib_site_t *site,
    const char *service_str,
    ib_site_service_t **pservice)
{
    assert(site != NULL);
    assert(service_str != NULL);

    ib_status_t rc;
    const ib_ctxsel_registration_t *ctxsel = &site->context->ib->act_ctxsel;

    if (ctxsel->service_create_fn == NULL) {
        rc = ib_site_service_create(site, service_str, NULL, NULL, pservice);
    }
    else {
        rc = ctxsel->service_create_fn(site, service_str,
                                       ctxsel->common_cb_data,
                                       ctxsel->service_create_cb_data,
                                       pservice);
    }
    return rc;
}

ib_status_t ib_ctxsel_site_open(
    const ib_engine_t *ib,
    ib_site_t *site)
{
    assert(ib != NULL);
    assert(site != NULL);

    ib_status_t rc = IB_OK;
    const ib_ctxsel_registration_t *ctxsel = &ib->act_ctxsel;

    if (ctxsel->site_open_fn != NULL) {
        rc = ctxsel->site_open_fn(ib, site,
                                  ctxsel->common_cb_data,
                                  ctxsel->site_open_cb_data);
    }
    return rc;
}

ib_status_t ib_ctxsel_location_open(
    const ib_engine_t *ib,
    ib_site_location_t *location)
{
    assert(ib != NULL);
    assert(location != NULL);

    ib_status_t rc = IB_OK;
    const ib_ctxsel_registration_t *ctxsel = &ib->act_ctxsel;

    if (ctxsel->location_open_fn != NULL) {
        rc = ctxsel->location_open_fn(ib, location, ctxsel->common_cb_data,
                                      ctxsel->location_open_cb_data);
    }
    return rc;
}

ib_status_t ib_ctxsel_site_close(
    const ib_engine_t *ib,
    ib_site_t *site)
{
    assert(ib != NULL);
    assert(site != NULL);

    ib_status_t rc = IB_OK;
    const ib_ctxsel_registration_t *ctxsel = &ib->act_ctxsel;

    if (ctxsel->site_close_fn != NULL) {
        rc = ctxsel->site_close_fn(ib, site,
                                   ctxsel->common_cb_data,
                                   ctxsel->site_close_cb_data);
    }
    return rc;
}

ib_status_t ib_ctxsel_location_close(
    const ib_engine_t *ib,
    ib_site_location_t *location)
{
    assert(ib != NULL);
    assert(location != NULL);

    ib_status_t rc = IB_OK;
    const ib_ctxsel_registration_t *ctxsel = &ib->act_ctxsel;

    if (ctxsel->location_close_fn != NULL) {
        rc = ctxsel->location_close_fn(ib, location,
                                       ctxsel->common_cb_data,
                                       ctxsel->location_close_cb_data);
    }
    return rc;
}

ib_status_t ib_ctxsel_finalize(
    const ib_engine_t *ib)
{
    assert(ib != NULL);

    ib_status_t rc = IB_OK;
    const ib_ctxsel_registration_t *ctxsel = &ib->act_ctxsel;

    if (ctxsel->finalize_fn != NULL) {
        rc = ctxsel->finalize_fn(ib,
                                 ctxsel->common_cb_data,
                                 ctxsel->finalize_cb_data);
    }
    return rc;
}

ib_status_t ib_ctxsel_registration_create(
    ib_mpool_t *mp,
    ib_module_t *module,
    void *common_cb_data,
    ib_ctxsel_registration_t **pregistration)
{
    assert(module != NULL);
    assert(pregistration != NULL);
    ib_ctxsel_registration_t *registration;

    if (mp == NULL) {
        registration = calloc(sizeof(*registration), 1);
    }
    else {
        registration = ib_mpool_calloc(mp, sizeof(*registration), 1);
    }
    if (registration == NULL) {
        return IB_EALLOC;
    }
    registration->mp = mp;
    registration->module = module;
    registration->common_cb_data = common_cb_data;
    *pregistration = registration;
    return IB_OK;
}

ib_status_t ib_ctxsel_registration_store_select(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_select_fn_t select_fn,
    void *fn_cb_data)
{
    assert(registration != NULL);
    assert(registration->module != NULL);
    assert(select_fn != NULL);

    registration->select_fn      = select_fn;
    registration->select_cb_data = fn_cb_data;
    return IB_OK;
}

ib_status_t ib_ctxsel_registration_store_site_create(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_site_create_fn_t site_create_fn,
    void *fn_cb_data)
{
    assert(registration != NULL);
    assert(registration->module != NULL);
    assert(site_create_fn != NULL);

    registration->site_create_fn      = site_create_fn;
    registration->site_create_cb_data = fn_cb_data;
    return IB_OK;
}

ib_status_t ib_ctxsel_registration_store_location_create(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_location_create_fn_t location_create_fn,
    void *fn_cb_data)
{
    assert(registration != NULL);
    assert(registration->module != NULL);
    assert(location_create_fn != NULL);

    registration->location_create_fn      = location_create_fn;
    registration->location_create_cb_data = fn_cb_data;
    return IB_OK;
}

ib_status_t ib_ctxsel_registration_store_host_create(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_host_create_fn_t host_create_fn,
    void *fn_cb_data)
{
    assert(registration != NULL);
    assert(registration->module != NULL);
    assert(host_create_fn != NULL);

    registration->host_create_fn      = host_create_fn;
    registration->host_create_cb_data = fn_cb_data;
    return IB_OK;
}

ib_status_t ib_ctxsel_registration_store_service_create(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_service_create_fn_t service_create_fn,
    void *fn_cb_data)
{
    assert(registration != NULL);
    assert(registration->module != NULL);
    assert(service_create_fn != NULL);

    registration->service_create_fn      = service_create_fn;
    registration->service_create_cb_data = fn_cb_data;
    return IB_OK;
}

ib_status_t ib_ctxsel_registration_store_site_open(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_site_open_fn_t site_open_fn,
    void *fn_cb_data)
{
    assert(registration != NULL);
    assert(registration->module != NULL);

    registration->site_open_fn      = site_open_fn;
    registration->site_open_cb_data = fn_cb_data;
    return IB_OK;
}

ib_status_t ib_ctxsel_registration_store_location_open(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_location_open_fn_t location_open_fn,
    void *fn_cb_data)
{
    assert(registration != NULL);
    assert(registration->module != NULL);

    registration->location_open_fn      = location_open_fn;
    registration->location_open_cb_data = fn_cb_data;
    return IB_OK;
}

ib_status_t ib_ctxsel_registration_store_site_close(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_site_close_fn_t site_close_fn,
    void *fn_cb_data)
{
    assert(registration != NULL);
    assert(registration->module != NULL);

    registration->site_close_fn      = site_close_fn;
    registration->site_close_cb_data = fn_cb_data;
    return IB_OK;
}

ib_status_t ib_ctxsel_registration_store_location_close(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_location_close_fn_t location_close_fn,
    void *fn_cb_data)
{
    assert(registration != NULL);
    assert(registration->module != NULL);

    registration->location_close_fn      = location_close_fn;
    registration->location_close_cb_data = fn_cb_data;
    return IB_OK;
}

ib_status_t ib_ctxsel_registration_store_finalize(
    ib_ctxsel_registration_t *registration,
    ib_ctxsel_finalize_fn_t finalize_fn,
    void *fn_cb_data)
{
    assert(registration != NULL);
    assert(registration->module != NULL);

    registration->finalize_fn      = finalize_fn;
    registration->finalize_cb_data = fn_cb_data;
    return IB_OK;
}

/**
 * Store a context registration data to a destination
 *
 * @param[out] dest Destination registration
 * @param[in] source Source registration
 *
 * @returns Status code:
 *  - IB_OK
 */
static ib_status_t ctxsel_store(
    ib_ctxsel_registration_t *dest,
    const ib_ctxsel_registration_t *source)
{
    assert(dest != NULL);
    assert(source != NULL);

    memcpy(dest, source, sizeof(*dest));
    return IB_OK;
}

ib_status_t ib_ctxsel_registration_register(
    ib_engine_t                    *ib,
    const ib_ctxsel_registration_t *registration)
{
    if ( (ib == NULL) ||
         (registration == NULL) ||
         (registration->module == NULL) ||
         (registration->select_fn == NULL) )
    {
        return IB_EINVAL;
    }

    if (registration->module == ib_core_module(ib)) {
        if (ib->core_ctxsel.module != NULL) {
            return IB_DECLINED;
        }
        ctxsel_store(&ib->core_ctxsel, registration);
    }

    /* If it's not the core module, don't allow a second registrant */
    else if (ib->act_ctxsel.module != ib_core_module(ib)) {
        return IB_DECLINED;
    }

    ctxsel_store(&ib->act_ctxsel, registration);
    return IB_OK;

}

ib_status_t ib_ctxsel_select_context(
    const ib_engine_t *ib,
    const ib_conn_t *conn,
    const ib_tx_t *tx,
    ib_context_t **pctx)
{
    assert(ib != NULL);
    assert(pctx != NULL);

    ib_status_t rc;
    const ib_ctxsel_registration_t *ctxsel = &ib->act_ctxsel;
    assert(ctxsel->select_fn != NULL);

    rc = ctxsel->select_fn(ib, conn, tx, ctxsel->common_cb_data,
                           ctxsel->select_cb_data, pctx);
    return rc;
}
