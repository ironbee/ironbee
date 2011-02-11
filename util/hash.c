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
 * @brief IronBee - Utility Hash Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <apr_lib.h>
#include <apr_hash.h>

#include <ironbee/util.h>

#include "ironbee_util_private.h"


ib_status_t ib_hash_create(ib_hash_t **ph, ib_mpool_t *pool)
{
    IB_FTRACE_INIT(ib_hash_create);
    ib_status_t rc;

    /* Create a hash table */
    *ph = (ib_hash_t *)ib_mpool_alloc(pool, sizeof(**ph));
    if (*ph == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    (*ph)->mp = pool;

    (*ph)->data = apr_hash_make((*ph)->mp->pool); /// @todo Used APR pool directly
    if ((*ph)->data == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    return IB_OK;

failed:
    /* Make sure everything is cleaned up on failure */
    *ph = NULL;

    return rc;
}

void ib_hash_clear(ib_hash_t *h)
{
    /// @todo If APR is >=1.3, then use apr_hash_clear() ???
    //apr_hash_clear(h->data);
   apr_hash_index_t *hi;
   for (hi = apr_hash_first(NULL, h->data); hi; hi = apr_hash_next(hi)) {
       const void *key;
       apr_ssize_t klen;

       apr_hash_this(hi, &key, &klen, NULL);
       apr_hash_set(h->data, key, klen, NULL);
   }
}

ib_status_t ib_hash_get_ex(ib_hash_t *h,
                           void *key, size_t klen,
                           void *pdata)
{
    if (key == NULL) {
        *(void **)pdata = NULL;
        return IB_EINVAL;
    }

    *(void **)pdata = apr_hash_get(h->data, key, (apr_ssize_t)klen);

    return *(void **)pdata ? IB_OK : IB_ENOENT;
}

ib_status_t ib_hash_get(ib_hash_t *h,
                        const char *key,
                        void *pdata)
{
    if (key == NULL) {
        *(void **)pdata = NULL;
        return IB_EINVAL;
    }

    return ib_hash_get_ex(h, (void *)key, strlen(key), pdata);
}

ib_status_t ib_hash_set_ex(ib_hash_t *h,
                           void *key, size_t klen,
                           void *data)
{
    /* Cannot be a NULL value (this means delete). */
    if (data == NULL) {
        return IB_EINVAL;
    }

    apr_hash_set(h->data, key, (apr_ssize_t)klen, data);

    return IB_OK;
}

ib_status_t ib_hash_set(ib_hash_t *h,
                        const char *key,
                        void *data)
{
    return ib_hash_set_ex(h, (void *)key, strlen(key), data);
}

ib_status_t ib_hash_remove_ex(ib_hash_t *h,
                              void *key, size_t klen,
                              void *pdata)
{
    void *data = apr_hash_get(h->data, key, (apr_ssize_t)klen);
    if (data == NULL) {
        if (pdata != NULL) {
            *(void **)pdata = NULL;
        }
        return IB_ENOENT;
    }

    if (pdata != NULL) {
        *(void **)pdata = data;
    }
    apr_hash_set(h->data, key, (apr_ssize_t)klen, NULL);

    return IB_OK;

}

ib_status_t ib_hash_remove(ib_hash_t *h,
                           const char *key,
                           void *pdata)
{
    return ib_hash_remove_ex(h, (void *)key, strlen(key), pdata);
}

