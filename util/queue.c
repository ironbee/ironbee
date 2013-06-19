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

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

/* A power of 2. */
static const size_t DEFAULT_QUEUE_SIZE = 8;

/**
 * A queue structure.
 */
struct ib_queue_t {
    size_t  head;  /**< Index of the first element. */
    size_t  size;  /**< The size of the queue buffer. */
    size_t  depth; /**< The number of elements in the queue. */
    void   *queue;
};

ib_status_t DLL_PUBLIC ib_queue_create(
    ib_queue_t **queue
)
{
    assert(queue != NULL);

    ib_queue_t *q = (ib_queue_t*)malloc(sizeof(*q));
    
    if (q == NULL) {
        return IB_EALLOC;
    }

    q->size  = DEFAULT_QUEUE_SIZE;
    q->depth = 0;
    q->head  = 0;
    q->queue = malloc(q->size);

    if (q->queue == NULL) {
        free(q);
        return IB_EALLOC;
    }

    *queue = q;

    return IB_OK;
}

static inline void **to_addr(
    ib_queue_t *queue,
    size_t offset
)
{
    assert(queue != NULL);

    size_t idx = (queue->head + offset) % queue->size;

    return (queue->queue + idx);
}

static inline ib_status_t shrink(
    ib_queue_t *queue
)
{
    assert(queue != NULL);
    size_t new_size = (queue->size) / 2;

    /* Do nothing if the queue size is too small. */
    if (new_size < DEFAULT_QUEUE_SIZE || new_size == 0) {
        return IB_OK;
    }

    queue->size = new_size;

    queue->queue = realloc(
        queue->queue, sizeof(*(queue->queue)) * queue->size);

    if (queue->queue == NULL) {
        return IB_EALLOC;
    }

    return IB_OK;
}

static inline ib_status_t grow(
    ib_queue_t *queue
)
{
    assert(queue != NULL);

    (queue->size) *= 2;
    queue->queue = realloc(
        queue->queue, sizeof(*(queue->queue)) * queue->size);

    if (queue->queue == NULL) {
        return IB_EALLOC;
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

    --(queue->head);
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

    ++(queue->head);
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

size_t DLL_PUBLIC ib_queue_size(
    ib_queue_t *queue
)
{
    assert(queue != NULL);
    return queue->depth;
}

void DLL_PUBLIC ib_queue_destroy(
    ib_queue_t *queue
)
{
    assert(queue != NULL);

    if (queue->queue != NULL) {
        free(queue->queue);
    }

    free(queue);
}
/** @} */
