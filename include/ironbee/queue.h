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

#ifndef _IB_QUEUE_H_
#define _IB_QUEUE_H_

/**
 * @file
 * @brief IronBee --- Queue Functions
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/mpool.h>
#include <ironbee/types.h>

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilQueue Queue
 * @ingroup IronBeeUtil
 *
 * A queue structure that minimizes memory allocations.
 *
 * While @ref ib_list_t may be used as a queue, it is ill suited to use
 * as a long-lived object with many elements being added and removed,
 * such as a work queue. Each node element created cannot be released
 * back to the memory pool, by design, and @ref ib_list_t does not
 * recycle list nodes.
 *
 * To accommodate the need for long-lived objects. While the intent of 
 * this object is that of a queue, it provides support for un-queue like
 * operations, such as
 * - ib_queue_push_front()
 * - ib_queue_pop_back()
 * - ib_queue_set()
 * - ib_queue_get()
 * 
 * @{
 */

/**
 * Queue data structure.
 */
typedef struct ib_queue_t ib_queue_t;

#define IB_QUEUE_NONE         (0x0)
#define IB_QUEUE_NEVER_SHRINK (1 << 0)

/**
 * Create a queue.
 *
 * @param[out] queue The created Queue.
 * @param[in] mp The memory pool that allocations will be made out of.
 *            This uses child pools to accomplish freeing memory.
 * @param[in] flags Options that influence the data structure's
 *            function's behavior.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 */
ib_status_t ib_queue_create(
    ib_queue_t **queue,
    ib_mpool_t   *mp,
    ib_flags_t   flags
);

/**
 * Enqueue an element.
 *
 * @param[in] queue The queue.
 * @param[in] element The element to push.
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC If a resize cannot get enough memory.
 */
ib_status_t ib_queue_push_back(
    ib_queue_t *queue,
    void *element
);

/**
 * Insert an element in the front of the queue.
 *
 * @param[in] queue The queue.
 * @param[in] element The element to push.
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC If a resize cannot get enough memory.
 */
ib_status_t ib_queue_push_front(
    ib_queue_t *queue,
    void *element
);

/**
 * Remove an element from the back of the queue.
 *
 * @param[in] queue The queue.
 * @param[out] element The element to retrieve.
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC If memory for a new queue cannot be obtained when resizing.
 */
ib_status_t ib_queue_pop_back(ib_queue_t *queue, void **element);

/**
 * Dequeue and element.
 *
 * @param[in] queue The queue.
 * @param[out] element The element to retrieve.
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC If memory for a new queue cannot be obtained when resizing.
 */
ib_status_t ib_queue_pop_front(
    ib_queue_t *queue,
    void **element
);

/**
 * Get the value at the front of the queue (index 0).
 *
 * @param[in] queue The queue.
 * @param[out] element The element to retrieve.
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If the queue is empty.
 */
ib_status_t ib_queue_peek(
    ib_queue_t *queue,
    void **element
);

/**
 * @param[in] queue The queue.
 * @param[in] index The index to get.
 * @param[out] element The element.
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If the element is not in the range of the queue.
 */
ib_status_t ib_queue_get(
    ib_queue_t *queue,
    size_t index,
    void **element
);

/**
 * @param[in] queue The queue.
 * @param[in] index The index to set.
 * @param[in] element The element to set.
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If the element is not in the range of the queue.
 */
ib_status_t ib_queue_set(
    ib_queue_t *queue,
    size_t index,
    void *element
);

/**
 * Set the size of the allocated memory in the queue. 
 *
 * This is useful when you know that many pushes are going to occur.
 *
 * If the new size of the queue is greater than ib_queue_size() returns,
 * then only the allocated memory is expanded to accommodate new
 * pushes without requesting more memory.
 *
 * If the new size of the queue is less than ib_queue_size() returns,
 * then the queue is truncated and ib_queue_size() will return @a size.
 *
 * Any push will cause another resizing of the queue.
 *
 * @param[in] queue The queue.
 * @param[in] size The new size.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On an allocation error.
 */
ib_status_t ib_queue_resize(
    ib_queue_t *queue,
    size_t size
);

/**
 * Return the current size (depth) of the queue.
 * @param[in] queue The queue.
 * @returns the size (depth) of the queue.
 */
size_t ib_queue_size(
    ib_queue_t *queue
);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* _IB_QUEUE_H_ */

