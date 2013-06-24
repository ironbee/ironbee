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
 * @brief IronBee --- Queue
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 * @nosubgrouping
 */

#include "ironbee_config_auto.h"

#include <ironbee/queue.h>
#include <ironbee/mpool.h>

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

/* A power of 2. */
static const size_t DEFAULT_QUEUE_SIZE = 8;

/**
 * A queue structure.
 */
struct ib_queue_t {
    size_t      head;  /**< Index of the first element. */
    size_t      size;  /**< The size of the queue buffer. */
    size_t      depth; /**< The number of elements in the queue. */
    ib_mpool_t *mp;    /**< Pool for allocations. */
    void      **queue; /**< The queue. */
    ib_flags_t  flags; /**< Operation flags. */
};

/**
 * Return the index into the queue that the offset points to.
 * @param[in] queue The queue.
 * @param[in] offset The offset.
 * @returns The address to assign to.
 */
static inline size_t to_index(
    ib_queue_t *queue,
    size_t offset
)
{
    assert(queue != NULL);
    return (queue->head + offset) % queue->size;
}

/**
 * Return a void ** in the queue array that may be assigned to.
 * @param[in] queue The queue.
 * @param[in] offset The offset.
 * @returns The address to assign to.
 */
static inline void **to_addr(
    ib_queue_t *queue,
    size_t offset
)
{
    assert(queue != NULL);
    return (queue->queue + to_index(queue, offset));
}

ib_status_t DLL_PUBLIC ib_queue_create(
    ib_queue_t **queue,
    ib_mpool_t *mp,
    ib_flags_t flags
)
{
    assert(queue != NULL);
    assert(mp != NULL);

    ib_queue_t *q = (ib_queue_t*)ib_mpool_alloc(mp, sizeof(*q));
    ib_status_t rc;
    
    if (q == NULL) {
        return IB_EALLOC;
    }

    rc = ib_mpool_create(&(q->mp), "queue", mp);
    if (rc != IB_OK) {
        return rc;
    }

    q->size  = DEFAULT_QUEUE_SIZE;
    q->depth = 0;
    q->head  = 0;
    q->flags = flags;
    q->queue = ib_mpool_alloc(q->mp, sizeof(*(q->queue)) * q->size);
    if (q->queue == NULL) {
        free(q);
        return IB_EALLOC;
    }

    *queue = q;

    return IB_OK;
}

/**
 * Take the given @a queue and repack it's data into @a new_queue.
 *
 * When a queue backed by an array is resized, if it wraps from the 
 * end to the beginning, that wrap point is necessarily different. Much
 * like resizing a hash, we must repack the data in the new queue.
 *
 * This implementation repacks the queue to index 0 so the 
 * repacked queue does not wrap and the head must be set to = 0.
 *
 * The size of @a new_queue must be less than ib_queue_t::depth.
 *
 * @param[in] queue The queue to repack.
 * @param[out] new_queue The repacked queue.
 */
static void repack(
    ib_queue_t *queue,
    void      **new_queue
)
{
    /* If true, then the queue wraps around the end of the array. */
    if (queue->size - queue->head < queue->depth) {
        size_t size_1 = queue->size - queue->head;
        size_t size_2 = queue->depth - size_1;

        /* Copy the unwrapped half. */
        memcpy(new_queue, to_addr(queue, 0), sizeof(*new_queue) * size_1);

        /* Copy the wrapped half. */
        memcpy(new_queue + size_1, queue->queue, sizeof(*new_queue) * size_2);
    }
    /* The queue does not wrap. Simple copy case. */
    else {
        memcpy(new_queue, to_addr(queue, 0), sizeof(*new_queue)*queue->depth);
    }
}

/**
 * Resize the queue.
 *
 * The size must not be less than ib_queue_t::depth or this will corrupt 
 * memory.
 */
static inline ib_status_t resize(
    ib_queue_t *queue,
    size_t new_size
)
{
    assert(queue != NULL);
    assert(new_size >= queue->depth);

    ib_mpool_t  *new_mp;
    void       **new_queue;
    ib_status_t  rc;

    rc = ib_mpool_create(&new_mp, "queue", ib_mpool_parent(queue->mp));
    if (rc != IB_OK) {
        return rc;
    }

    new_queue = ib_mpool_alloc(new_mp, sizeof(*new_queue) * new_size);
    if (new_queue == NULL) {
        return IB_EALLOC;
    }

    repack(queue, new_queue);

    ib_mpool_release(queue->mp);

    queue->size  = new_size;
    queue->head  = 0;
    queue->queue = new_queue;
    queue->mp    = new_mp;
    queue->size  = new_size;

    return IB_OK;
}

/**
 * Shrink the queue by half.
 * @param[in] queue The queue to halve in size.
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 */
static inline ib_status_t shrink(
    ib_queue_t *queue
)
{
    assert(queue != NULL);

    size_t      new_size;
    ib_status_t rc;

    if (queue->flags & IB_QUEUE_NEVER_SHRINK) {
        return IB_OK;
    }

    new_size = (queue->size) / 2;

    /* Do nothing if the queue size is too small. */
    if (new_size < DEFAULT_QUEUE_SIZE || new_size == 0) {
        return IB_OK;
    }

    rc = resize(queue, new_size);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * @param[in] queue The queue to double in size.
 *
 * @returns 
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 * - IB_EINVAL If overflow is detected.
 */
static inline ib_status_t grow(
    ib_queue_t *queue
)
{
    assert(queue != NULL);

    size_t      new_size;
    ib_status_t rc;

    new_size = queue->size * 2;

    /* Guard against overflow. */
    if (new_size < queue->size) {
        return IB_EINVAL;
    }

    rc = resize(queue, new_size);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_queue_push_back(
    ib_queue_t *queue,
    void *element
)
{
    assert(queue != NULL);

    if (queue->depth == queue->size) {
        ib_status_t rc = grow(queue);
        if (rc != IB_OK) {
            return IB_OK;
        }
    }

    *(to_addr(queue, queue->depth)) = element;

    ++(queue->depth);

    return IB_OK;
}
ib_status_t DLL_PUBLIC ib_queue_push_front(
    ib_queue_t *queue,
    void *element
)
{
    assert(queue != NULL);

    if (queue->depth == queue->size) {
        ib_status_t rc = grow(queue);
        if (rc != IB_OK) {
            return IB_OK;
        }
    }

    queue->head = (queue->head == 0)?  queue->size - 1 : queue->head - 1;
    ++(queue->depth);

    *(to_addr(queue, 0)) = element;

    return IB_OK;
}
ib_status_t DLL_PUBLIC ib_queue_pop_back(
    ib_queue_t *queue,
    void **element
)
{
    assert(queue != NULL);
    assert(element != NULL);

    if (queue->depth == 0) {
        return IB_EINVAL;
    }

    --(queue->depth);

    *element = *to_addr(queue, queue->depth);

    if (queue->depth * 2 < queue->size) {
        ib_status_t rc = shrink(queue);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}
ib_status_t DLL_PUBLIC ib_queue_pop_front(
    ib_queue_t  *queue,
    void       **element
)
{
    assert(queue != NULL);
    assert(element != NULL);

    if (queue->depth == 0) {
        return IB_EINVAL;
    }

    *element = *to_addr(queue, 0);

    queue->head = (queue->head == queue->size - 1)?  0 : queue->head + 1;
    --(queue->depth);

    if (queue->depth * 2 < queue->size) {
        ib_status_t rc = shrink(queue);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}
ib_status_t DLL_PUBLIC ib_queue_peek(
    ib_queue_t  *queue,
    void       **element
)
{
    assert(queue != NULL);
    assert(element != NULL);

    if (queue->depth == 0) {
        return IB_EINVAL;
    }

    *element = *to_addr(queue, 0);

    return IB_OK;
}
ib_status_t DLL_PUBLIC ib_queue_get(
    ib_queue_t *queue,
    size_t index,
    void **element
)
{
    assert(queue != NULL);
    assert(element != NULL);

    if (queue->depth == 0 || queue->depth <= index) {
        return IB_EINVAL;
    }

    *element = *to_addr(queue, index);

    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_queue_set(
    ib_queue_t *queue,
    size_t index,
    void *element
)
{
    assert(queue != NULL);

    if (queue->depth == 0 || queue->depth <= index) {
        return IB_EINVAL;
    }

    *(to_addr(queue, index)) = element;

    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_queue_resize(
    ib_queue_t *queue,
    size_t      size
)
{
    assert(queue != NULL);

    if (size < queue->depth) {
        queue->depth = size;
    }

    return resize(queue, size);
}

size_t DLL_PUBLIC ib_queue_size(
    ib_queue_t *queue
)
{
    assert(queue != NULL);
    return queue->depth;
}

/** @} */
