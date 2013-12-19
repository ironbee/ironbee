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
 * @brief IronBee --- Utility Array Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 *
 * This is a dynamic array implemented as an array of arrays allocated
 * in chunks (size ninit) as needed.
 *
 * This is essentially a two dimensional array storing a pointer to arbitrary
 * data.  The first dimension, the extent index (row), is allocated
 * immediately. The second dimension, the data index (column), is allocated
 * on demand (though the first set is allocated immediately).
 *
 * When data is added, enough extents are allocated to accommodate for the
 * index. If the number of extents required exceeds what is available, then
 * a replacement extents array is allocated, double the size it was
 * previously, the data arrays are copied into the new extents array, and
 * new data arrays are allocated. Because the extents array will expand
 * exponentially, it is important to set ninit/nextents correctly
 * for the task.
 */

#include "ironbee_config_auto.h"
#include <ironbee/array.h>
#include <string.h>

/**
 * Dynamic array structure.
 */
struct ib_array_t {
    ib_mpool_t       *mp;
    size_t            ninit;
    size_t            nextents;
    size_t            nelts;
    size_t            size;
    void             *extents;
};

/**
 * Calculate the extent index from the array index for an array.
 *
 * @param arr Array
 * @param idx Array index
 *
 * @returns Extent index where data resides
 */
#define IB_ARRAY_EXTENT_INDEX(arr, idx) \
    ((idx) / (arr)->ninit)

/**
 * Calculate the data index from the array and extent indexes for an array.
 *
 * @param arr Array
 * @param idx Array index
 * @param extent_idx Extent index (via @ref IB_ARRAY_EXTENT_INDEX)
 *
 * @returns Data index where data resides within the given extent
 */
#define IB_ARRAY_DATA_INDEX(arr, idx, extent_idx) \
    ((idx) - ((extent_idx) * (arr)->ninit))

ib_status_t ib_array_create(ib_array_t **parr, ib_mpool_t *pool,
                            size_t ninit, size_t nextents)
{
    ib_status_t rc;

    /* Validate. */
    if ((ninit == 0) || (nextents == 0)) {
        return IB_EINVAL;
    }

    /* Create the structure. */
    *parr = (ib_array_t *)ib_mpool_alloc(pool, sizeof(**parr));
    if (*parr == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    (*parr)->mp = pool;
    (*parr)->ninit = ninit;
    (*parr)->nextents = nextents;
    (*parr)->nelts = 0;
    (*parr)->size = ninit;

    /* Create the extents array. */
    (*parr)->extents = (void *)ib_mpool_calloc(pool,
                                               nextents, sizeof(void *));
    if ((*parr)->extents == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    /* Create the first data array in the first extent slot. */
    *((void ***)(*parr)->extents) = (void **)ib_mpool_calloc(pool,
        ninit, sizeof(void *)
    );
    if (*((void ***)(*parr)->extents) == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }

    return IB_OK;

failed:
    /* Make sure everything is cleaned up on failure. */
    *parr = NULL;

    return rc;
}

ib_status_t ib_array_get(ib_array_t *arr, size_t idx, void *pval)
{
    size_t r, c;
    void **data;

    if (idx >= arr->nelts) {
        *(void **)pval = NULL;
        return IB_ENOENT;
    }

    /* Calculate the row/column where the data resides. */
    r = IB_ARRAY_EXTENT_INDEX(arr, idx);
    c = IB_ARRAY_DATA_INDEX(arr, idx, r);

    data = ((void ***)arr->extents)[r];
    *(void **)pval = data[c];

    return IB_OK;
}

ib_status_t ib_array_setn(ib_array_t *arr, size_t idx, void *val)
{
    size_t r, c;
    void **data;

    /* Keep extending by ninit elements until the index fits in the array. */
    while (idx >= arr->size) {
        r = arr->size / arr->ninit;

        /* If this will exceed the max, then reallocate the extents
         * to double its previous value to make room.
         */
        if (r >= arr->nextents) {
            void *new_extents = (void *)ib_mpool_calloc(arr->mp,
                                                        arr->nextents * 2,
                                                        sizeof(void *));
            if (new_extents == NULL) {
                return IB_EALLOC;
            }
            memcpy(new_extents, arr->extents, sizeof(void *) * arr->nextents);
            arr->extents = new_extents;
            arr->nextents *= 2;
        }

        ((void ***)arr->extents)[r] = (void **)ib_mpool_calloc(arr->mp,
            arr->ninit, sizeof(void *)
        );
        arr->size += arr->ninit;
    }

    /* Calculate the row/column where the data resides. */
    r = IB_ARRAY_EXTENT_INDEX(arr, idx);
    c = IB_ARRAY_DATA_INDEX(arr, idx, r);

    data = ((void ***)arr->extents)[r];
    data[c] = val;

    /* Keep track of the number of elements stored. */
    if (idx >= arr->nelts) {
        arr->nelts = idx + 1;
    }

    return IB_OK;
}

ib_status_t ib_array_appendn(ib_array_t *arr, void *val)
{
    ib_status_t rc = ib_array_setn(arr, arr->nelts, val);
    return rc;
}

size_t ib_array_elements(ib_array_t *arr)
{
    return arr->nelts;
}

size_t ib_array_size(ib_array_t *arr)
{
    return arr->size;
}
