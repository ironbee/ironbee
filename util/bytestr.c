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
 * @brief IronBee - Utility Byte String Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/util.h>

#include <htp/bstr.h>

#include "ironbee_util_private.h"

struct ib_bytestr_t {
    ib_mpool_t       *mp;
    ib_flags_t        flags;
    bstr             *data;
};

static ib_status_t bytestr_cleanup(void *data)
{
    IB_FTRACE_INIT(bytestr_cleanup);
    ib_bytestr_t *bs = (ib_bytestr_t *)data;
    bstr_free(&bs->data);
    IB_FTRACE_RET_STATUS(IB_OK);
}


size_t ib_bytestr_length(ib_bytestr_t *bs)
{
    IB_FTRACE_INIT(ib_bytestr_length);
    if ((bs == NULL) || (bs->data == NULL)) {
        IB_FTRACE_RET_SIZET(0);
    }
    /* bstr_len is a macro, so can go in the FTRACE return wrapper. */
    IB_FTRACE_RET_SIZET(bstr_len(bs->data));
}

size_t ib_bytestr_size(ib_bytestr_t *bs)
{
    IB_FTRACE_INIT(ib_bytestr_size);
    if ((bs == NULL) || (bs->data == NULL)) {
        IB_FTRACE_RET_SIZET(0);
    }
    /* bstr_len is a macro, so can go in the FTRACE return wrapper. */
    IB_FTRACE_RET_SIZET(bstr_size(bs->data));
}

uint8_t *ib_bytestr_ptr(ib_bytestr_t *bs)
{
    IB_FTRACE_INIT(ib_bytestr_ptr);
    if ((bs == NULL) || (bs->data == NULL)) {
        IB_FTRACE_RET_PTR(uint8_t, NULL);
    }
    /* bstr_len is a macro, so can go in the FTRACE return wrapper. */
    IB_FTRACE_RET_PTR(uint8_t, (uint8_t *)bstr_ptr(bs->data));
}

ib_status_t ib_bytestr_create(ib_bytestr_t **pdst,
                              ib_mpool_t *pool,
                              size_t size)
{
    IB_FTRACE_INIT(ib_bytestr_create);
    ib_status_t rc;

    /* Create the structure. */
    *pdst = (ib_bytestr_t *)ib_mpool_alloc(pool, sizeof(**pdst));
    if (*pdst == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    (*pdst)->data = (bstr *)bstr_alloc(size);
    if ((*pdst)->data == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    (*pdst)->mp = pool;
    (*pdst)->flags = 0;
    ib_mpool_cleanup_register((*pdst)->mp, *pdst, bytestr_cleanup);

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    /* Make sure everything is cleaned up on failure */
    *pdst = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_bytestr_dup(ib_bytestr_t **pdst,
                           ib_mpool_t *pool,
                           const ib_bytestr_t *src)
{
    IB_FTRACE_INIT(ib_bytestr_dup);
    ib_status_t rc;

    if ((src == NULL) || (src->data == NULL)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_bytestr_create(pdst, pool, bstr_len(src->data));
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    (*pdst)->data = bstr_add_noex((bstr *)(*pdst)->data, src->data);
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_bytestr_dup_mem(ib_bytestr_t **pdst,
                               ib_mpool_t *pool,
                               const uint8_t *data,
                               size_t dlen)
{
    IB_FTRACE_INIT(ib_bytestr_dup_mem);
    ib_status_t rc = ib_bytestr_create(pdst, pool, dlen);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    (*pdst)->data = bstr_add_mem_noex((bstr *)(*pdst)->data, (char *)data, dlen);
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_bytestr_dup_nulstr(ib_bytestr_t **pdst,
                                     ib_mpool_t *pool,
                                     const char *data)
{
    IB_FTRACE_INIT(ib_bytestr_dup_nulstr);
    ib_status_t rc = ib_bytestr_dup_mem(pdst, pool, (uint8_t *)data, strlen(data));
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_bytestr_alias(ib_bytestr_t **pdst,
                             ib_mpool_t *pool,
                             const ib_bytestr_t *src)
{
    IB_FTRACE_INIT(ib_bytestr_alias);
    ib_status_t rc;

    if ((src == NULL) || (src->data == NULL)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_bytestr_create(pdst, pool, 0);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    (*(bstr_t *)((*pdst)->data)).ptr = bstr_ptr(src->data);
    (*(bstr_t *)((*pdst)->data)).len = bstr_len(src->data);
    (*(bstr_t *)((*pdst)->data)).size = bstr_size(src->data);
    (*pdst)->flags |= IB_BYTESTR_FREADONLY;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_bytestr_alias_mem(ib_bytestr_t **pdst,
                                 ib_mpool_t *pool,
                                 const uint8_t *data,
                                 size_t dlen)
{
    IB_FTRACE_INIT(ib_bytestr_alias_mem);
    ib_status_t rc = ib_bytestr_create(pdst, pool, 0);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    (*(bstr_t *)((*pdst)->data)).ptr = (char *)data;
    (*(bstr_t *)((*pdst)->data)).len = dlen;
    (*(bstr_t *)((*pdst)->data)).size = dlen;
    (*pdst)->flags |= IB_BYTESTR_FREADONLY;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_bytestr_alias_nulstr(ib_bytestr_t **pdst,
                                    ib_mpool_t *pool,
                                    const char *data)
{
    IB_FTRACE_INIT(ib_bytestr_alias_nulstr);
    IB_FTRACE_RET_STATUS(ib_bytestr_alias_mem(pdst, pool, (uint8_t *)data, strlen(data)));
}

ib_status_t ib_bytestr_append(ib_bytestr_t *dst,
                              const ib_bytestr_t *src)
{
    IB_FTRACE_INIT(ib_bytestr_append);
    if (IB_BYTESTR_CHECK_FREADONLY(dst->flags)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    bstr_add(dst->data, src->data);
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_bytestr_append_mem(ib_bytestr_t *dst,
                                  const uint8_t *data,
                                  size_t dlen)
{
    IB_FTRACE_INIT(ib_bytestr_append_mem);
    if (IB_BYTESTR_CHECK_FREADONLY(dst->flags)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    bstr_add_mem(dst->data, (char *)data, dlen);
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_bytestr_append_nulstr(ib_bytestr_t *dst,
                                     const char *data)
{
    IB_FTRACE_INIT(ib_bytestr_append_nulstr);
    if (IB_BYTESTR_CHECK_FREADONLY(dst->flags)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    bstr_add_mem(dst->data, (char *)data, strlen(data));
    IB_FTRACE_RET_STATUS(IB_OK);
}

