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
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/bytestr.h>

#include <ironbee/debug.h>
#include <ironbee/mpool.h>

#include <assert.h>

struct ib_bytestr_t {
    ib_mpool_t *mp;
    ib_flags_t  flags;
    uint8_t    *data;
    size_t      length;
    size_t      size;
};

size_t ib_bytestr_length(
    const ib_bytestr_t *bs
) {
    IB_FTRACE_INIT();

    assert(bs != NULL);

    IB_FTRACE_RET_SIZET(bs->length);
}

size_t ib_bytestr_size(
    const ib_bytestr_t *bs
) {
    IB_FTRACE_INIT();

    assert(bs != NULL);

    IB_FTRACE_RET_SIZET(bs->size);
}

ib_mpool_t* ib_bytestr_mpool(const ib_bytestr_t *bs)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(ib_mpool_t, bs->mp);
}

uint8_t *ib_bytestr_ptr(
    ib_bytestr_t *bs
) {
    IB_FTRACE_INIT();

    if (bs == NULL || IB_BYTESTR_CHECK_FREADONLY(bs->flags)) {
        IB_FTRACE_RET_PTR(uint8_t, NULL);
    }

    IB_FTRACE_RET_PTR(uint8_t, bs->data);
}

const uint8_t DLL_PUBLIC *ib_bytestr_const_ptr(
    const ib_bytestr_t *bs
) {
    IB_FTRACE_INIT();

    if (bs == NULL) {
        IB_FTRACE_RET_PTR(const uint8_t, NULL);
    }

    IB_FTRACE_RET_PTR(const uint8_t, bs->data);
}

ib_status_t ib_bytestr_create(
    ib_bytestr_t **pdst,
    ib_mpool_t    *pool,
    size_t         size
) {
    IB_FTRACE_INIT();

    assert(pdst != NULL);
    assert(pool != NULL);

    ib_status_t rc;

    /* Create the structure. */
    *pdst = (ib_bytestr_t *)ib_mpool_alloc(pool, sizeof(**pdst));
    if (*pdst == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    (*pdst)->data   = NULL;
    (*pdst)->mp     = pool;
    (*pdst)->flags  = 0;
    (*pdst)->size   = size;
    (*pdst)->length = 0;

    if (size != 0) {
        (*pdst)->data = (uint8_t *)ib_mpool_alloc(pool, size);
        if ((*pdst)->data == NULL) {
            rc = IB_EALLOC;
            goto failed;
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);

failed:
    *pdst = NULL;

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_bytestr_dup(
    ib_bytestr_t       **pdst,
    ib_mpool_t          *pool,
    const ib_bytestr_t  *src
) {
    IB_FTRACE_INIT();

    assert(pdst != NULL);
    assert(pool != NULL);

    ib_status_t rc;
    rc = ib_bytestr_dup_mem(
        pdst,
        pool,
        ib_bytestr_const_ptr(src),
        ib_bytestr_length(src)
    );

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_bytestr_dup_mem(
    ib_bytestr_t  **pdst,
    ib_mpool_t     *pool,
    const uint8_t  *data,
    size_t          data_length
) {
    IB_FTRACE_INIT();

    assert(pdst != NULL);
    assert(pool != NULL);

    if (data == NULL && data_length == 0) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_status_t rc = ib_bytestr_create(pdst, pool, data_length);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (data != NULL) {
        assert((*pdst)->size >= data_length);

        memcpy(ib_bytestr_ptr(*pdst), data, data_length);
        (*pdst)->length = data_length;
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_bytestr_dup_nulstr(
    ib_bytestr_t **pdst,
    ib_mpool_t *pool,
    const char *data
) {
    IB_FTRACE_INIT();

    assert(pdst != NULL);
    assert(pool != NULL);
    assert(data != NULL);

    ib_status_t rc = ib_bytestr_dup_mem(
        pdst,
        pool,
        (uint8_t *)data,
        strlen(data)
    );

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_bytestr_alias(
    ib_bytestr_t       **pdst,
    ib_mpool_t          *pool,
    const ib_bytestr_t  *src
)
{
    IB_FTRACE_INIT();

    assert(pdst != NULL);
    assert(pool != NULL);

    ib_status_t rc;

    if ((src == NULL) || (src->data == NULL)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_bytestr_alias_mem(
        pdst,
        pool,
        ib_bytestr_const_ptr(src),
        src->length
    );

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_bytestr_alias_mem(
    ib_bytestr_t   **pdst,
    ib_mpool_t      *pool,
    const uint8_t   *data,
    size_t           data_length
)
{
    IB_FTRACE_INIT();

    if (data == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_status_t rc = ib_bytestr_create(pdst, pool, 0);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* We use flags to enforce that the user can not recover an non-const
     * pointer.
     */
    (*pdst)->data   = (uint8_t*)data;
    (*pdst)->length = data_length;
    (*pdst)->size   = data_length;
    (*pdst)->flags |= IB_BYTESTR_FREADONLY;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_bytestr_alias_nulstr(ib_bytestr_t **pdst,
                                    ib_mpool_t *pool,
                                    const char *data)
{
    IB_FTRACE_INIT();

    assert(pdst != NULL);
    assert(pool != NULL);

    ib_status_t rc;
    rc = ib_bytestr_alias_mem(pdst, pool, (uint8_t *)data, strlen(data));

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_bytestr_setv(
    ib_bytestr_t *dst,
    uint8_t      *data,
    size_t        data_length
) {
    IB_FTRACE_INIT();

    if (dst == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (data == NULL && data_length != 0) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    dst->data   = data;
    dst->length = data_length;
    dst->size   = data_length;
    dst->flags  = 0;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_bytestr_setv_const(
    ib_bytestr_t  *dst,
    const uint8_t *data,
    size_t         data_length
)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    /* Use flags to enforce const. */
    rc = ib_bytestr_setv(dst, (uint8_t*)data, data_length);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    dst->flags |= IB_BYTESTR_FREADONLY;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_bytestr_append(
    ib_bytestr_t *dst,
    const ib_bytestr_t *src
)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    if ( src == NULL ) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_bytestr_append_mem(
        dst,
        ib_bytestr_const_ptr(src),
        ib_bytestr_length(src)
    );

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_bytestr_append_mem(
    ib_bytestr_t  *dst,
    const uint8_t *data,
    size_t         data_length
)
{
    IB_FTRACE_INIT();

    size_t dst_length = ib_bytestr_length(dst);
    size_t new_length;
    uint8_t *new_data = NULL;

    if (dst == NULL || IB_BYTESTR_CHECK_FREADONLY(dst->flags)) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (data == NULL && data_length != 0) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    new_length = dst_length + data_length;

    if (new_length > dst->size) {
        new_data = (uint8_t *)ib_mpool_alloc(dst->mp, new_length);
        if (new_data == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
        if (dst_length > 0) {
            memcpy(
                new_data,
                ib_bytestr_const_ptr(dst),
                ib_bytestr_length(dst)
            );
        }
        dst->data = new_data;
        dst->size = new_length;
    }
    assert(new_length <= dst->size);

    if (data_length > 0) {
        memcpy(dst->data + dst_length, data, data_length);
        dst->length = new_length;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_bytestr_append_nulstr(
    ib_bytestr_t *dst,
    const char *data
)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    if (data == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_bytestr_append_mem(dst, (const uint8_t*)data, strlen(data));

    IB_FTRACE_RET_STATUS(rc);
}

ib_num_t ib_bytestr_read_only( const ib_bytestr_t *bs )
{
    IB_FTRACE_INIT();

    assert(bs != NULL);

    IB_FTRACE_RET_INT(IB_BYTESTR_CHECK_FREADONLY(bs->flags));
}

void ib_bytestr_make_read_only( ib_bytestr_t *bs )
{
    IB_FTRACE_INIT();

    bs->flags |= IB_BYTESTR_FREADONLY;

    IB_FTRACE_RET_VOID();
}

int ib_bytestr_index_of_c(
    const ib_bytestr_t *haystack,
    const char   *needle
)
{
    IB_FTRACE_INIT();

    size_t i = 0;
    size_t j = 0;
    size_t haystack_length = ib_bytestr_length(haystack);
    size_t needle_length = strlen(needle);
    const uint8_t* haystack_data = ib_bytestr_const_ptr(haystack);
    int result = -1;

    if (
        haystack == NULL || needle == NULL ||
        needle_length == 0 || haystack_length == 0
    ) {
        IB_FTRACE_RET_INT(-1);
    }

    for (i = 0; i < haystack_length - (needle_length-1); ++i) {
        result = i;
        for (j = 0; j < needle_length; ++j) {
            if (haystack_data[i+j] != needle[j]) {
                result = -1;
                break;
            }
        }
        if (result != -1) {
            IB_FTRACE_RET_INT(result);
        }
    }

    IB_FTRACE_RET_INT(-1);
}
