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
 * @brief IronBee --- Utility Byte String Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/bytestr.h>

#include <ironbee/mm.h>
#include <ironbee/string.h>

#include <assert.h>
#include <stdbool.h>

struct ib_bytestr_t {
    ib_mm_t     mm;
    ib_flags_t  flags;
    uint8_t    *data;
    size_t      length;
    size_t      size;
};

size_t ib_bytestr_length(
    const ib_bytestr_t *bs
) {
    assert(bs != NULL);

    return bs->length;
}

size_t ib_bytestr_size(
    const ib_bytestr_t *bs
) {
    assert(bs != NULL);

    return bs->size;
}

ib_mm_t ib_bytestr_mm(const ib_bytestr_t *bs)
{
    return bs->mm;
}

uint8_t *ib_bytestr_ptr(
    ib_bytestr_t *bs
) {
    if (bs == NULL || IB_BYTESTR_CHECK_FREADONLY(bs->flags)) {
        return NULL;
    }

    return bs->data;
}

const uint8_t DLL_PUBLIC *ib_bytestr_const_ptr(
    const ib_bytestr_t *bs
) {
    if (bs == NULL) {
        return NULL;
    }

    return bs->data;
}

ib_status_t ib_bytestr_create(
    ib_bytestr_t **pdst,
    ib_mm_t        mm,
    size_t         size
) {
    assert(pdst != NULL);

    ib_status_t rc;

    /* Create the structure. */
    *pdst = (ib_bytestr_t *)ib_mm_alloc(mm, sizeof(**pdst));
    if (*pdst == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    (*pdst)->data   = NULL;
    (*pdst)->mm     = mm;
    (*pdst)->flags  = 0;
    (*pdst)->size   = size;
    (*pdst)->length = 0;

    if (size != 0) {
        (*pdst)->data = (uint8_t *)ib_mm_alloc(mm, size);
        if ((*pdst)->data == NULL) {
            rc = IB_EALLOC;
            goto failed;
        }
    }

    return IB_OK;

failed:
    *pdst = NULL;

    return rc;
}

ib_status_t ib_bytestr_dup(
    ib_bytestr_t       **pdst,
    ib_mm_t              mm,
    const ib_bytestr_t  *src
) {
    assert(pdst != NULL);

    ib_status_t rc;
    rc = ib_bytestr_dup_mem(
        pdst,
        mm,
        ib_bytestr_const_ptr(src),
        ib_bytestr_length(src)
    );

    return rc;
}

ib_status_t ib_bytestr_dup_mem(
    ib_bytestr_t  **pdst,
    ib_mm_t         mm,
    const uint8_t  *data,
    size_t          data_length
) {
    assert(pdst != NULL);

    if (data == NULL && data_length != 0) {
        return IB_EINVAL;
    }

    ib_status_t rc = ib_bytestr_create(pdst, mm, data_length);
    if (rc != IB_OK) {
        return rc;
    }

    if (data != NULL) {
        assert((*pdst)->size >= data_length);

        memcpy(ib_bytestr_ptr(*pdst), data, data_length);
        (*pdst)->length = data_length;
    }
    return IB_OK;
}

ib_status_t ib_bytestr_dup_nulstr(
    ib_bytestr_t **pdst,
    ib_mm_t        mm,
    const char    *data
) {
    assert(pdst != NULL);
    assert(data != NULL);

    ib_status_t rc = ib_bytestr_dup_mem(
        pdst,
        mm,
        (uint8_t *)data,
        strlen(data)
    );

    return rc;
}

ib_status_t ib_bytestr_alias(
    ib_bytestr_t       **pdst,
    ib_mm_t              mm,
    const ib_bytestr_t  *src
)
{
    assert(pdst != NULL);

    ib_status_t rc;

    if ((src == NULL) || (src->data == NULL)) {
        return IB_EINVAL;
    }

    rc = ib_bytestr_alias_mem(
        pdst,
        mm,
        ib_bytestr_const_ptr(src),
        src->length
    );

    return rc;
}

ib_status_t ib_bytestr_alias_mem(
    ib_bytestr_t   **pdst,
    ib_mm_t          mm,
    const uint8_t   *data,
    size_t           data_length
)
{
    if (data == NULL && data_length > 0) {
        return IB_EINVAL;
    }

    ib_status_t rc = ib_bytestr_create(pdst, mm, 0);
    if (rc != IB_OK) {
        return rc;
    }

    /* We use flags to enforce that the user can not recover an non-const
     * pointer.
     */
    (*pdst)->data   = (uint8_t*)data;
    (*pdst)->length = data_length;
    (*pdst)->size   = data_length;
    (*pdst)->flags |= IB_BYTESTR_FREADONLY;

    return IB_OK;
}

ib_status_t ib_bytestr_alias_nulstr(
    ib_bytestr_t **pdst,
    ib_mm_t        mm,
    const char    *data
)
{
    assert(pdst != NULL);

    ib_status_t rc;
    rc = ib_bytestr_alias_mem(pdst, mm, (uint8_t *)data, strlen(data));

    return rc;
}

ib_status_t ib_bytestr_setv(
    ib_bytestr_t *dst,
    uint8_t      *data,
    size_t        data_length
) {
    if (dst == NULL) {
        return IB_EINVAL;
    }
    if (data == NULL && data_length != 0) {
        return IB_EINVAL;
    }

    dst->data   = data;
    dst->length = data_length;
    dst->size   = data_length;
    dst->flags  = 0;

    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_bytestr_setv_const(
    ib_bytestr_t  *dst,
    const uint8_t *data,
    size_t         data_length
)
{
    ib_status_t rc;

    /* Use flags to enforce const. */
    rc = ib_bytestr_setv(dst, (uint8_t*)data, data_length);
    if (rc != IB_OK) {
        return rc;
    }

    dst->flags |= IB_BYTESTR_FREADONLY;

    return IB_OK;
}

ib_status_t ib_bytestr_append(
    ib_bytestr_t *dst,
    const ib_bytestr_t *src
)
{
    ib_status_t rc;

    if ( src == NULL ) {
        return IB_EINVAL;
    }

    rc = ib_bytestr_append_mem(
        dst,
        ib_bytestr_const_ptr(src),
        ib_bytestr_length(src)
    );

    return rc;
}

ib_status_t ib_bytestr_append_mem(
    ib_bytestr_t  *dst,
    const uint8_t *data,
    size_t         data_length
)
{
    size_t dst_length = ib_bytestr_length(dst);
    size_t new_length;
    uint8_t *new_data = NULL;

    if (dst == NULL || IB_BYTESTR_CHECK_FREADONLY(dst->flags)) {
        return IB_EINVAL;
    }
    if (data == NULL && data_length != 0) {
        return IB_EINVAL;
    }

    new_length = dst_length + data_length;

    if (new_length > dst->size) {
        new_data = (uint8_t *)ib_mm_alloc(dst->mm, new_length);
        if (new_data == NULL) {
            return IB_EALLOC;
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

    return IB_OK;
}

ib_status_t ib_bytestr_append_nulstr(
    ib_bytestr_t *dst,
    const char *data
)
{
    ib_status_t rc;

    if (data == NULL) {
        return IB_EINVAL;
    }

    rc = ib_bytestr_append_mem(dst, (const uint8_t*)data, strlen(data));

    return rc;
}

int ib_bytestr_read_only( const ib_bytestr_t *bs )
{
    assert(bs != NULL);

    return IB_BYTESTR_CHECK_FREADONLY(bs->flags);
}

void ib_bytestr_make_read_only( ib_bytestr_t *bs )
{
    bs->flags |= IB_BYTESTR_FREADONLY;

    return;
}