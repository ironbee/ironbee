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
 * @brief IronBee --- Stream IO Implementation
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/queue.h>
#include <ironbee/mm_mpool_lite.h>
#include <ironbee/mpool_freeable.h>
#include <ironbee/mpool_lite.h>
#include <ironbee/stream_io.h>

#include <assert.h>

struct ib_stream_io_t {
    ib_mm_t              mm;
    ib_mpool_freeable_t *mp;
};

struct ib_stream_io_tx_t {
    ib_stream_io_t *io;         /**< The stream this is for. */
    ib_queue_t     *input;      /**< Input queue. */
    ib_queue_t     *output;     /**< Output queue. */
};

struct ib_stream_io_data_t {
    ib_mpool_freeable_segment_t *segment; /**< Memory backing. */
    uint8_t                     *ptr;     /**< Pointer into segment. */
    size_t                       len;     /**< The length in bytes. */
    ib_stream_io_type_t          type;    /**< Type of data this is. */
};

static void stream_io_cleanup(void *cbdata)
{
    assert(cbdata != NULL);

    ib_stream_io_t *io = (ib_stream_io_t *)cbdata;

    assert(io->mp != NULL);

    ib_mpool_freeable_destroy(io->mp);
}

ib_status_t ib_stream_io_create(
    ib_stream_io_t      **io,
    ib_mm_t               mm
)
{
    assert(io != NULL);

    ib_stream_io_t *tmp;
    ib_status_t     rc;

    tmp = ib_mm_alloc(mm, sizeof(*tmp));
    if (tmp == NULL) {
        return IB_EALLOC;
    }

    rc = ib_mpool_freeable_create(&tmp->mp);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_mm_register_cleanup(mm, stream_io_cleanup, tmp);
    if (rc != IB_OK) {
        return rc;
    }

    tmp->mm = mm;

    *io = tmp;
    return IB_OK;
}

ib_status_t ib_stream_io_tx_create(
    ib_stream_io_tx_t **io_tx,
    ib_stream_io_t     *io
)
{
    assert(io_tx != NULL);
    assert(io != NULL);

    ib_status_t        rc;
    ib_stream_io_tx_t *tmp;

    tmp = ib_mm_alloc(io->mm, sizeof(*tmp));
    if (tmp == NULL) {
        return IB_EALLOC;
    }

    rc = ib_queue_create(&tmp->input, io->mm, IB_QUEUE_NONE);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_queue_create(&tmp->output, io->mm, IB_QUEUE_NONE);
    if (rc != IB_OK) {
        return rc;
    }

    tmp->io = io;

    *io_tx = tmp;
    return IB_OK;
}

ib_status_t ib_stream_io_tx_data_add(
    ib_stream_io_tx_t *io_tx,
    const uint8_t     *data,
    size_t             len
)
{
    assert(io_tx != NULL);
    assert((data != NULL && len > 0) || (len == 0));

    ib_status_t          rc;
    ib_stream_io_data_t *stream_data;
    uint8_t             *ptr;

    rc = ib_stream_io_data_alloc(io_tx, len, &stream_data, &ptr);
    if (rc != IB_OK) {
        return rc;
    }

    /* Copy data into the new pointer. */
    memcpy(ptr, data, len);

    rc = ib_queue_enqueue(io_tx->input, stream_data);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_stream_io_tx_flush_add(
    ib_stream_io_tx_t *io_tx
)
{
    assert(io_tx != NULL);
    assert(io_tx->input != NULL);

    ib_status_t                  rc;
    ib_mpool_freeable_segment_t *segment;
    ib_stream_io_data_t         *data;
    ib_mpool_freeable_t         *mp = io_tx->io->mp;

    segment = ib_mpool_freeable_segment_alloc(mp, sizeof(*data));
    if (segment == NULL) {
        return IB_EALLOC;
    }

    data          = ib_mpool_freeable_segment_ptr(segment);
    data->segment = segment;
    data->ptr     = NULL;
    data->len     = 0;
    data->type    = IB_STREAM_IO_FLUSH;

    rc = ib_queue_enqueue(io_tx->input, data);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

static void stream_clear_queue(ib_queue_t *q, ib_stream_io_tx_t *io_tx)
{
    assert(q != NULL);

    while(ib_queue_size(q) > 0) {
        ib_status_t          rc;
        ib_stream_io_data_t *data;

        rc = ib_queue_dequeue(q, &data);
        if (rc != IB_OK) {
            return;
        }

        ib_stream_io_data_unref(io_tx, data);
    }
}

ib_status_t DLL_PUBLIC ib_stream_io_tx_redo(
    ib_stream_io_tx_t *io_tx
)
{
    assert(io_tx != NULL);
    assert(io_tx->output != NULL);

    stream_clear_queue(io_tx->output, io_tx);

    return IB_OK;
}

ib_status_t ib_stream_io_tx_reuse(
    ib_stream_io_tx_t *io_tx
)
{
    assert(io_tx != NULL);
    assert(io_tx->input != NULL);
    assert(io_tx->output != NULL);

    ib_queue_t  *queue;

    /* Swap the input and output queues. */
    queue         = io_tx->input;
    io_tx->input  = io_tx->output;
    io_tx->output = queue;

    stream_clear_queue(io_tx->output, io_tx);

    return IB_OK;
}

void ib_stream_io_tx_cleanup(
    ib_stream_io_tx_t *io_tx
)
{
    assert(io_tx->io != NULL);
    assert(io_tx->io->mp != NULL);
    assert(io_tx->input != NULL);
    assert(io_tx->output != NULL);

    stream_clear_queue(io_tx->output, io_tx);

    stream_clear_queue(io_tx->input, io_tx);
}

size_t ib_stream_io_data_depth(
    ib_stream_io_tx_t *io_tx
)
{
    assert(io_tx != NULL);
    assert(io_tx->input != NULL);

    return ib_queue_size(io_tx->input);
}

ib_status_t ib_stream_io_data_peek(
    ib_stream_io_tx_t   *io_tx,
    uint8_t             **ptr,
    size_t               *len,
    ib_stream_io_type_t  *type
)
{
    assert(io_tx != NULL);

    /* NOTE: This will never return IB_EINVAL because
     * if the size == 0, IB_ENOENT is returned. Any other value
     * of size will satisfy index = 0. */
    return ib_stream_io_data_peek_at(io_tx, 0, ptr, len, type);
}

ib_status_t ib_stream_io_data_peek_at(
    ib_stream_io_tx_t    *io_tx,
    size_t                index,
    uint8_t             **ptr,
    size_t               *len,
    ib_stream_io_type_t  *type
)
{
    assert(io_tx != NULL);
    assert(io_tx->input != NULL);

    ib_stream_io_data_t *data;
    ib_status_t          rc;

    if (ib_queue_size(io_tx->input) == 0) {
        return IB_ENOENT;
    }

    rc = ib_queue_get(io_tx->input, index, &data);
    if (rc != IB_OK) {
        return rc;
    }

    if (ptr != NULL) {
        *ptr = data->ptr;
    }

    if (len != NULL) {
        *len = data->len;
    }

    if (type != NULL) {
        *type = data->type;
    }

    return IB_OK;
}

ib_status_t ib_stream_io_data_take(
    ib_stream_io_tx_t    *io_tx,
    ib_stream_io_data_t **data,
    uint8_t             **ptr,
    size_t               *len,
    ib_stream_io_type_t  *type
)
{
    assert(io_tx != NULL);
    assert(io_tx->input != NULL);
    assert(data != NULL);

    ib_stream_io_data_t *d;
    ib_status_t          rc;

    if (ib_queue_size(io_tx->input) == 0) {
        return IB_ENOENT;
    }

    rc = ib_queue_dequeue(io_tx->input, &d);
    if (rc != IB_OK) {
        return rc;
    }
    *data = d;

    if (ptr != NULL) {
        *ptr = d->ptr;
    }

    if (len != NULL) {
        *len = d->len;
    }

    if (type != NULL) {
        *type = d->type;
    }

    return IB_OK;
}

ib_status_t ib_stream_io_data_put(
    ib_stream_io_tx_t   *io_tx,
    ib_stream_io_data_t *data
)
{
    assert(io_tx != NULL);
    assert(io_tx->output != NULL);
    assert(data != NULL);

    ib_status_t rc;

    rc = ib_queue_enqueue(io_tx->output, data);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_stream_io_data_flush(
    ib_stream_io_tx_t *io_tx
)
{
    assert(io_tx != NULL);
    assert(io_tx->io != NULL);
    assert(io_tx->io->mp != NULL);

    ib_status_t                  rc;
    ib_mpool_freeable_segment_t *segment;
    ib_stream_io_data_t         *data;
    ib_mpool_freeable_t         *mp = io_tx->io->mp;

    segment = ib_mpool_freeable_segment_alloc(mp, sizeof(*data));
    if (segment == NULL) {
        return IB_EALLOC;
    }

    data          = ib_mpool_freeable_segment_ptr(segment);
    data->segment = segment;
    data->ptr     = NULL;
    data->len     = 0;
    data->type    = IB_STREAM_IO_FLUSH;

    rc = ib_stream_io_data_put(io_tx, data);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_stream_io_data_alloc(
    ib_stream_io_tx_t    *io_tx,
    size_t                len,
    ib_stream_io_data_t **data,
    uint8_t             **ptr
)
{
    assert(io_tx != NULL);
    assert(io_tx->io != NULL);
    assert(io_tx->io->mp != NULL);

    ib_mpool_freeable_segment_t *segment;
    ib_stream_io_data_t         *d;
    ib_mpool_freeable_t         *mp = io_tx->io->mp;

    segment = ib_mpool_freeable_segment_alloc(mp, len + sizeof(*d));
    if (segment == NULL) {
        return IB_EALLOC;
    }

    d          = ib_mpool_freeable_segment_ptr(segment);
    d->segment = segment;
    d->ptr     = (uint8_t *)(((char *)d) + sizeof(*d));
    d->len     = len;
    d->type    = IB_STREAM_IO_DATA;

    *data = d;
    *ptr  = d->ptr;
    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_stream_io_data_slice(
    ib_stream_io_tx_t    *io_tx,
    size_t                start,
    size_t                length,
    ib_stream_io_data_t **dst,
    uint8_t             **ptr
)
{
    assert(io_tx != NULL);
    assert(io_tx->input != NULL);
    assert(io_tx->io != NULL);
    assert(io_tx->io->mp != NULL);
    assert(dst != NULL);
    assert(ptr != NULL);

    ib_mpool_freeable_t *mp = io_tx->io->mp;
    ib_stream_io_data_t *src;
    ib_stream_io_data_t *d;
    ib_status_t          rc;

    /* Peek at the data. */
    rc = ib_queue_peek(io_tx->input, &src);
    if (rc != IB_OK) {
        return rc;
    }

    /* Make sure this is a data type. */
    if (src->type != IB_STREAM_IO_DATA) {
        return IB_EINVAL;
    }

    /* If the user askes us to copy a segment that lands outside of src. */
    if (start + length > src->len) {
        return IB_EINVAL;
    }

    d = (ib_stream_io_data_t *)ib_mpool_freeable_alloc(mp, sizeof(*d));
    if (d == NULL) {
        return IB_EALLOC;
    }

    /* Increase the references to the segment. */
    rc = ib_mpool_freeable_segment_ref(mp, src->segment);
    if (rc != IB_OK) {
        return rc;
    }

    d->segment = src->segment;
    d->ptr     = (void *)((((char *)src->ptr)) + start);
    d->len     = length;
    d->type    = src->type;

    *dst = d;
    *ptr = d->ptr;
    return IB_OK;
}

ib_status_t ib_stream_io_data_discard(
    ib_stream_io_tx_t *io_tx
)
{
    assert(io_tx != NULL);
    assert(io_tx->input != NULL);

    ib_stream_io_data_t *data;
    ib_status_t          rc;

    rc = ib_queue_dequeue(io_tx->input, &data);
    if (rc != IB_OK) {
        return rc;
    }

    ib_stream_io_data_unref(io_tx, data);

    return IB_OK;
}

ib_status_t ib_stream_io_data_forward(
    ib_stream_io_tx_t *io_tx
)
{
    assert(io_tx != NULL);
    assert(io_tx->input != NULL);
    assert(io_tx->output != NULL);

    ib_stream_io_data_t *data;
    ib_status_t          rc;

    rc = ib_queue_dequeue(io_tx->input, &data);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_queue_enqueue(io_tx->output, data);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

void ib_stream_io_data_ref(
    ib_stream_io_tx_t   *io_tx,
    ib_stream_io_data_t *data
)
{
    assert(io_tx != NULL);
    assert(io_tx->io != NULL);
    assert(io_tx->io->mp != NULL);

    ib_mpool_freeable_t *mp = io_tx->io->mp;

    ib_mpool_freeable_segment_ref(mp, data->segment);
}

void ib_stream_io_data_unref(
    ib_stream_io_tx_t   *io_tx,
    ib_stream_io_data_t *data
)
{
    assert(io_tx != NULL);
    assert(io_tx->io != NULL);
    assert(io_tx->io->mp != NULL);

    ib_mpool_freeable_t *mp = io_tx->io->mp;

    ib_mpool_freeable_segment_free(mp, data->segment);
}


