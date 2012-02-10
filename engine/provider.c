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
 * @brief IronBee - Provider Interface
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>


#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/hash.h>
#include <ironbee/provider.h>

#include "ironbee_private.h"


ib_status_t ib_provider_define(ib_engine_t *ib,
                               const char *type,
                               ib_provider_register_fn_t fn_reg,
                               void *api)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_provider_def_t *prd;
    char *type_copy;

    /* Create the provider definition. */
    prd = (ib_provider_def_t *)ib_mpool_calloc(ib->config_mp, 1, sizeof(*prd));
    if (prd == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    prd->mp = ib->config_mp;
    prd->fn_reg = fn_reg;
    prd->api = api;

    /* Copy the type. */
    type_copy = (char *)ib_mpool_alloc(prd->mp, strlen(type) + 1);
    if (type_copy == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    strcpy(type_copy, type);
    prd->type = (const char *)type_copy;

    rc = ib_hash_set(ib->apis, type, prd);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_provider_register(ib_engine_t *ib,
                                 const char *type,
                                 const char *key,
                                 ib_provider_t **ppr,
                                 void *iface,
                                 ib_provider_inst_init_fn_t fn_init)
{
    IB_FTRACE_INIT();
    char *pr_key;
    ib_status_t rc;
    ib_provider_def_t *prd;
    ib_provider_t *pr;

    if (ppr != NULL) {
        *ppr = NULL;
    }

    /* Get the API, if any */
    rc = ib_hash_get(&prd, ib->apis, type);
    if (rc != IB_OK) {
        ib_log_error(ib, 1,
                     "Error registering provider \"%s\": "
                     "Unknown provider type \"%s\"",
                     key, type);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Create the provider. */
    pr = (ib_provider_t *)ib_mpool_calloc(prd->mp, 1, sizeof(*pr));
    if (pr == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    pr->ib = ib;
    pr->mp = prd->mp;
    pr->type = prd->type;
    pr->iface = iface;
    pr->api = prd->api;
    pr->fn_init = fn_init;

    if (ppr != NULL) {
        *ppr = pr;
    }

    /* Register. */
    /// @todo Hash of hash?  Hash of list?
    pr_key = (char *)ib_mpool_alloc(ib->mp, strlen(type) + strlen(key) + 2);
    memcpy(pr_key, type, strlen(type));
    pr_key[strlen(type)] = '.';
    memcpy(pr_key + strlen(type) + 1, key, strlen(key));
    pr_key[strlen(type) + strlen(key) + 1] = '\0';
    rc = ib_hash_set(ib->providers, pr_key, pr);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* If available, call the registration callback,
     * de-registering on failure.
     */
    if (prd->fn_reg != NULL) {
        rc = prd->fn_reg(ib, pr);
        if (rc != IB_OK) {
            ib_hash_remove(NULL, ib->providers, pr_key);
        }
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_provider_lookup(ib_engine_t *ib,
                               const char *type,
                               const char *key,
                               ib_provider_t **ppr)
{
    IB_FTRACE_INIT();
    char *pr_key;
    ib_status_t rc;

    /// @todo Hash of hash?  Hash of list?
    pr_key = (char *)ib_mpool_alloc(ib->mp, strlen(type) + strlen(key) + 2);
    memcpy(pr_key, type, strlen(type));
    pr_key[strlen(type)] = '.';
    memcpy(pr_key + strlen(type) + 1, key, strlen(key));
    pr_key[strlen(type) + strlen(key) + 1] = '\0';
    rc = ib_hash_get(ppr, ib->providers, pr_key);
    if (rc != IB_OK) {
        *ppr = NULL;
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_provider_instance_create_ex(ib_engine_t *ib,
                                           ib_provider_t *pr,
                                           ib_provider_inst_t **ppi,
                                           ib_mpool_t *pool,
                                           void *data)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Create the provider instance. */
    *ppi = (ib_provider_inst_t *)ib_mpool_calloc(pool, 1, sizeof(**ppi));
    if (*ppi == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    (*ppi)->mp = pool;
    (*ppi)->pr = pr;
    (*ppi)->data = NULL;

    /* Use an initialization function if available. */
    if (pr->fn_init != NULL) {
        rc = pr->fn_init(*ppi, data);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        (*ppi)->data = data;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_provider_instance_create(ib_engine_t *ib,
                                        const char *type,
                                        const char *key,
                                        ib_provider_inst_t **ppi,
                                        ib_mpool_t *pool,
                                        void *data)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_provider_t *pr;

    /* Get the provider */
    rc = ib_provider_lookup(ib, type, key, &pr);
    if (rc != IB_OK) {
        /// @todo no provider registered
        *ppi = NULL;
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_provider_instance_create_ex(ib, pr, ppi, pool, data);

    IB_FTRACE_RET_STATUS(rc);
}

void *ib_provider_data_get(ib_provider_t *pr)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(void, pr->data);
}

void ib_provider_data_set(ib_provider_t *pr, void *data)
{
    IB_FTRACE_INIT();
    pr->data = data;
    IB_FTRACE_RET_VOID();
}

