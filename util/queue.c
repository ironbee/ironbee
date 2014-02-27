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
 * @brief IronBee --- Queue Implementation
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/queue.h>
#include <ironbee/mm.h>
#include <ironbee/mpool_lite.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* A power of 2. */
static const size_t DEFAULT_QUEUE_SIZE = 1 << 3;

/**
 * A queue structure.
 */
struct ib_queue_t {
    size_t           head;       /**< Index of the first element. */
    size_t           allocation; /**< The allocation of the queue buffer. */
    size_t           size;       /**< The number of elements in the queue. */
    ib_mpool_lite_t *mp;         /**< Pool for allocations. */
    void            *queue;      /**< The queue. */
    ib_flags_t       flags;      /**< Flags. @sa IB_QUEUE_NEVER_SHRINK. */
};

/**
 * Return the index into queue that the offset points to.
 *
 * @param[in] queue The queue.
 * @param[in] offset The logical offset from the head of the queue. And
 *            offset of 0 will return the index in the queue at which
 *            the first element is found. This value is equal
 *            queue->head.
 *
 * @returns The index in queue->queue at which the requested element resides.
 */
static inline size_t to_index(
    const ib_queue_t *queue,
    size_t            offset
)
{
    assert(queue != NULL);
    return (queue->head + offset) % queue->allocation;
}

/**
 * Destory a queue.
 */
static void queue_destroy(void *q) {
    ib_mpool_lite_destroy(((ib_queue_t *)(q))->mp);
}

/**
 * Return a @c void * in the queue array that may be assigned to.
 *
 * @param[in] queue The queue.
 * @param[in] offset The offset.
 *
 * @returns The address to assign to.
 */
static inline void *to_addr(
    const ib_queue_t *queue,
    size_t offset
)
{
    assert(queue != NULL);
    return ((void **)(queue->queue) + to_index(queue, offset));
}

ib_status_t ib_queue_create(
    ib_queue_t **queue,
    ib_mm_t      mm,
    ib_flags_t   flags
)
{
    assert(queue != NULL);

    ib_queue_t *q = (ib_queue_t *)ib_mm_alloc(mm, sizeof(*q));
    ib_status_t rc;

    if (q == NULL) {
        return IB_EALLOC;
    }

    rc = ib_mpool_lite_create(&(q->mp));
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_mm_register_cleanup(mm, queue_destroy, q);
    if (rc != IB_OK) {
        return rc;
    }

    q->allocation  = DEFAULT_QUEUE_SIZE;
    q->size = 0;
    q->head  = 0;
    q->flags = flags;
    q->queue = ib_mpool_lite_alloc(q->mp, sizeof(void *) * q->allocation);
    if (q->queue == NULL) {
        free(q);
        return IB_EALLOC;
    }

    *queue = q;

    return IB_OK;
}

/**
 * Take the given @a queue and repack its data into @a new_queue.
 *
 * When a queue backed by an array is resized, if it wraps from the
 * end to the beginning, that wrap point is necessarily different. Much
 * like resizing a hash, we must repack the data in the new queue.
 *
 * This implementation repacks the queue to index 0 so the
 * repacked queue does not wrap and the head must be set to 0.
 *
 * The allocation of @a new_queue must be less than ib_queue_t::size.
 *
 * @param[in] queue The queue to repack.
 * @param[out] new_queue The repacked queue.
 */
static void repack(
    ib_queue_t *queue,
    void       *new_queue
)
{
    /* If true, then the queue wraps around the end of the array. */
    if (queue->allocation - queue->head < queue->size) {
        size_t size_1 = queue->allocation - queue->head;
        size_t size_2 = queue->size - size_1;

        /* Copy the unwrapped half. */
        memcpy(new_queue, to_addr(queue, 0), sizeof(void *) * size_1);

        /* Copy the wrapped half. */
        memcpy(
            (void **)new_queue + size_1,
            queue->queue,
            sizeof(void *) * size_2);
    }
    /* The queue does not wrap. Simple copy case. */
    else {
        memcpy(new_queue, to_addr(queue, 0), sizeof(void *) * queue->size);
    }
}

/**
 * Resize the queue.
 *
 * The allocation must not be less than ib_queue_t::size or this will corrupt
 * memory.
 */
static ib_status_t resize(
    ib_queue_t *queue,
    size_t      new_size
)
{
    assert(queue != NULL);
    assert(new_size >= queue->size);

    ib_mpool_lite_t  *new_mp;
    void             *new_queue;
    ib_status_t       rc;

    rc = ib_mpool_lite_create(&new_mp);
    if (rc != IB_OK) {
        return rc;
    }

    new_queue = ib_mpool_lite_alloc(new_mp, sizeof(void *) * new_size);
    if (new_queue == NULL) {
        return IB_EALLOC;
    }

    repack(queue, (void **)new_queue);

    ib_mpool_lite_destroy(queue->mp);

    queue->allocation  = new_size;
    queue->head        = 0;
    queue->queue       = new_queue;
    queue->mp          = new_mp;
    queue->allocation  = new_size;

    return IB_OK;
}

/**
 * Shrink the queue by half unless prevented from doing so.
 *
 * - If IB_QUEUE_NEVER_SHRINK is set, no action is taken.
 * - If the queue is too small (allocation=1), no action is taken.
 *
 * @param[in] queue The queue to halve in allocation.
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 */
static ib_status_t shrink(
    ib_queue_t *queue
)
{
    assert(queue != NULL);

    size_t      new_size;
    ib_status_t rc;

    if (queue->flags & IB_QUEUE_NEVER_SHRINK) {
        return IB_OK;
    }

    new_size = (queue->allocation) / 2;

    /* Do nothing if the queue allocation is too small. */
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
 * @param[in] queue The queue to double in allocation.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 * - IB_EINVAL If overflow is detected.
 */
static ib_status_t grow(
    ib_queue_t *queue
)
{
    assert(queue != NULL);

    size_t      new_size;
    ib_status_t rc;

    new_size = queue->allocation * 2;

    /* Guard against overflow. */
    if (new_size < queue->allocation) {
        return IB_EINVAL;
    }

    rc = resize(queue, new_size);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_queue_push_back(
    ib_queue_t *queue,
    void       *element
)
{
    assert(queue != NULL);

    if (queue->size == queue->allocation) {
        ib_status_t rc = grow(queue);
        if (rc != IB_OK) {
            return IB_OK;
        }
    }

    *(void **)to_addr(queue, queue->size) = element;

    ++(queue->size);

    return IB_OK;
}

ib_status_t ib_queue_push_front(
    ib_queue_t *queue,
    void       *element
)
{
    assert(queue != NULL);

    if (queue->size == queue->allocation) {
        ib_status_t rc = grow(queue);
        if (rc != IB_OK) {
            return IB_OK;
        }
    }

    queue->head = (queue->head == 0)?  queue->allocation - 1 : queue->head - 1;
    ++(queue->size);

    *(void **)to_addr(queue, 0) = element;

    return IB_OK;
}

ib_status_t ib_queue_pop_back(
    ib_queue_t *queue,
    void       *element
)
{
    assert(queue != NULL);
    assert(element != NULL);

    if (queue->size == 0) {
        return IB_EINVAL;
    }

    --(queue->size);

    *(void **)element = *(void **)to_addr(queue, queue->size);

    if (queue->size * 2 < queue->allocation) {
        ib_status_t rc = shrink(queue);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

ib_status_t ib_queue_pop_front(
    ib_queue_t *queue,
    void       *element
)
{
    assert(queue != NULL);
    assert(element != NULL);

    if (queue->size == 0) {
        return IB_EINVAL;
    }

    *(void **)element = *(void **)to_addr(queue, 0);

    queue->head = (queue->head == queue->allocation - 1)?  0 : queue->head + 1;
    --(queue->size);

    if (queue->size * 2 < queue->allocation) {
        ib_status_t rc = shrink(queue);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

ib_status_t ib_queue_peek(
    const ib_queue_t *queue,
    void             *element
)
{
    assert(queue != NULL);
    assert(element != NULL);

    if (queue->size == 0) {
        return IB_EINVAL;
    }

    *(void **)element = *(void **)to_addr(queue, 0);

    return IB_OK;
}

ib_status_t ib_queue_get(
    const ib_queue_t *queue,
    size_t            index,
    void             *element
)
{
    assert(queue != NULL);
    assert(element != NULL);

    if (queue->size == 0 || queue->size <= index) {
        return IB_EINVAL;
    }

    *(void **)element = *(void **)to_addr(queue, index);

    return IB_OK;
}

ib_status_t ib_queue_set(
    ib_queue_t *queue,
    size_t      index,
    void       *element
)
{
    assert(queue != NULL);

    if (queue->size == 0 || queue->size <= index) {
        return IB_EINVAL;
    }

    *(void **)to_addr(queue, index) = element;

    return IB_OK;
}

ib_status_t ib_queue_reserve(
    ib_queue_t *queue,
    size_t      allocation
)
{
    assert(queue != NULL);

    if (allocation < queue->size) {
        queue->size = allocation;
    }

    return resize(queue, allocation);
}

size_t ib_queue_size(
    const ib_queue_t *queue
)
{
    assert(queue != NULL);
    return queue->size;
}

ib_status_t ib_queue_enqueue(
    ib_queue_t *queue,
    void       *element
)
{
    return ib_queue_push_back(queue, element);
}

ib_status_t ib_queue_dequeue(
    ib_queue_t *queue,
    void       *element
)
{
    return ib_queue_pop_front(queue, element);
}

ib_status_t ib_queue_dequeue_all_to_function(
    ib_queue_t             *queue,
    ib_queue_element_fn_t   fn,
    void                   *cbdata
)
{
    assert(queue != NULL);
    ib_status_t rc;

    for (size_t i = 0; i < queue->size; ++i) {
        /* Get the element at index i. */
        void *element = *(void **)to_addr(queue, i);

        /* Call the user's function. */
        fn(element, cbdata);
    }

    /* Now shrink the queue to zero size. */
    queue->size = 0;

    rc = resize(queue,  DEFAULT_QUEUE_SIZE);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/** @} */
