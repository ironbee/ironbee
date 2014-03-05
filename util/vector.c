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
 * @brief IronBee --- Utility Vector Functions
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

/**
 * This is a dynamic vector implementation.
 *
 * When growing, it will dynamically double its size while tracking its
 * length.
 */

#include "ironbee_config_auto.h"

#include <ironbee/vector.h>

#include <ironbee/mm.h>
#include <ironbee/mpool_lite.h>

#include <assert.h>
#include <math.h>

static const size_t DEFAULT_VECTOR_SIZE = 0;

/**
 * Given the length of data in a buffer, compute the size to hold it.
 *
 * Size constraints are (size/2) < length <= (size).
 */
static ib_status_t buffer_size(size_t length, size_t *size)
{
    /* If the high-order bit of data_length is 1, we can't
     * represent a buffer length
     * len = 2^x such that l is greater than data_length. */
    if (length & ~( ~0U >> 1U ) ) {
        return IB_EINVAL;
    }

    if (length == 0) {
        return 0;
    }

    *size = 1;

    /* Special thanks to Sean Anderson for this code (public domain):
     * http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogObvious
     * The loop conditions have been slightly altered for our purposes. */
    for (; length > 0; length >>= 0x1) {
        (*size)<<=1;
    }

    return IB_OK;
}

/**
 * Destroys a vector.
 */
static void vector_destroy(void *vector) {
    ib_mpool_lite_destroy(((ib_vector_t *)vector)->mp);
}


ib_status_t ib_vector_create(
    ib_vector_t **vector,
    ib_mm_t       mm,
    ib_flags_t    flags
)
{

    assert(vector != NULL);

    ib_status_t rc;

    /* Allocate from the parent pool. */
    ib_vector_t *v = ib_mm_alloc(mm, sizeof(*v));
    if (v == NULL) {
        return IB_EALLOC;
    }

    /* Register the cleanup routine if the allocation succeeds. */
    rc = ib_mm_register_cleanup(mm, vector_destroy, v);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_mpool_lite_create(&(v->mp));
    if (rc != IB_OK) {
        return rc;
    }

    v->data  = ib_mpool_lite_alloc(v->mp, DEFAULT_VECTOR_SIZE);
    v->size  = DEFAULT_VECTOR_SIZE;
    v->len   = 0;
    v->flags = flags;

    *vector = v;
    return IB_OK;
}

ib_status_t ib_vector_resize(
    ib_vector_t *vector,
    size_t size
)
{
    assert(vector != NULL);
    assert(vector->mp != NULL);

    ib_status_t rc;
    ib_mpool_lite_t *new_mp = NULL;
    void *new_data;

    if (size == vector->size) {
        return IB_OK;
    }

    if ((vector->flags & IB_VECTOR_NEVER_SHRINK) && (vector->size > size)) {
        return IB_OK;
    }

    rc = ib_mpool_lite_create(&new_mp);
    if (rc != IB_OK) {
        return rc;
    }

    /* Allocate and copy data. */
    new_data = ib_mpool_lite_alloc(new_mp, size);
    if (new_data == NULL) {
        ib_mpool_lite_destroy(new_mp);
        return IB_EALLOC;
    }
    memcpy(new_data, vector->data, (size < vector->size)? size : vector->size);

    /* Destroy old data. */
    ib_mpool_lite_destroy(vector->mp);

    /* Re-assign everything. */
    vector->mp = new_mp;
    vector->data = new_data;
    vector->size = size;
    if (vector->len > size) {
        vector->len = size;
    }

    return IB_OK;
}

ib_status_t ib_vector_truncate(
    ib_vector_t *vector,
    size_t len
)
{
    assert(vector != NULL);
    assert(vector->mp != NULL);

    /* Invalid request - IB_EINVAL. */
    if (len > vector->len) {
        return IB_EINVAL;
    }

    /* No change - IB_OK. */
    if (len == vector->len) {
        return IB_OK;
    }

    vector->len = len;

    /* If we never shrink, don't check if we should shrink the buffer. */
    if (vector->flags & IB_VECTOR_NEVER_SHRINK) {
        return IB_OK;
    }

    /* Do we need to resize things? */
    if (len < vector->size / 4) {
        ib_status_t rc;
        size_t vector_size = 0;
        rc = buffer_size(len, &vector_size);
        if (rc != IB_OK) {
            return rc;
        }

        return ib_vector_resize(vector, vector_size);
    }

    return IB_OK;
}

ib_status_t ib_vector_append(
    ib_vector_t *vector,
    const void *data,
    size_t data_length
)
{
    assert(vector != NULL);
    assert(vector->mp != NULL);
    assert(data != NULL);

    ib_status_t rc;
    size_t vector_size = 0;
    size_t new_len = data_length + vector->len;

    /* Check for overflow. */
    if ( vector->size + data_length < vector->size ) {
        return IB_EINVAL;
    }

    if (data_length == 0) {
        return IB_OK;
    }

    rc = buffer_size(new_len, &vector_size);
    if (rc != IB_OK) {
        return rc;
    }

    /* Resize the vector.
     * The resize function checks if vector_size == v->size.*/
    rc = ib_vector_resize(vector, vector_size);
    if (rc != IB_OK) {
        return IB_EALLOC;
    }

    memcpy(vector->data + vector->len, data, data_length);
    vector->len = new_len;

    return IB_OK;
}
