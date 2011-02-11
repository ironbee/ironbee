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
 * @brief IronBee - Utility Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 *
 * @todo Reimplement w/o APR
 */

#include "ironbee_config_auto.h"

#include <apr_lib.h>
#include <apr_pools.h>

#include <ironbee/util.h>

#include "ironbee_util_private.h"

ib_status_t ib_mpool_create(ib_mpool_t **pmp, ib_mpool_t *parent)
{
    IB_FTRACE_INIT(ib_mpool_create);
    apr_pool_t *mp;
    ib_status_t rc;
    apr_status_t res;
    
    res = apr_pool_create(&mp, (parent ? parent->pool : NULL));
    if (res != APR_SUCCESS) {
        rc = IB_EALLOC;
        goto failed;
    }

    *pmp = (ib_mpool_t *)apr_palloc(mp, sizeof(**pmp));
    if (*pmp == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    (*pmp)->pool = mp;

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure. */
    if (*pmp != NULL) {
        ib_mpool_destroy(*pmp);
    }
    else if (mp != NULL) {
        apr_pool_destroy(mp);
    }
    *pmp = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

void *ib_mpool_alloc(ib_mpool_t *mp, size_t size)
{
    IB_FTRACE_INIT(ib_mpool_alloc);
    void *ptr = apr_palloc(mp->pool, (apr_size_t)size);
    IB_FTRACE_RET_PTR(void, ptr);
}

void *ib_mpool_calloc(ib_mpool_t *mp, size_t nelem, size_t size)
{
    IB_FTRACE_INIT(ib_mpool_calloc);
    void *ptr = apr_pcalloc(mp->pool, (apr_size_t)(nelem * size));
    IB_FTRACE_RET_PTR(void, ptr);
}

void ib_mpool_clear(ib_mpool_t *mp)
{
    IB_FTRACE_INIT(ib_mpool_clear);
    apr_pool_clear(mp->pool);
    IB_FTRACE_RET_VOID();
}

void ib_mpool_destroy(ib_mpool_t *mp)
{
    IB_FTRACE_INIT(ib_mpool_destroy);
    apr_pool_destroy(mp->pool);
    IB_FTRACE_RET_VOID();
}

void ib_mpool_cleanup_register(ib_mpool_t *mp,
                               void *data,
                               ib_mpool_cleanup_fn_t cleanup)
{
    IB_FTRACE_INIT(ib_mpool_cleanup_register);
    apr_pool_cleanup_register(mp->pool, data, (apr_status_t(*)(void *))cleanup, NULL);
    IB_FTRACE_RET_VOID();
}
