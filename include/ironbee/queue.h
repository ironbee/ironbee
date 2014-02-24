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
 * @brief IronBee --- IronBee Queue
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/mpool_lite.h>
#include <ironbee/mm.h>
#include <ironbee/types.h>

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilQueue Queue
 * @ingroup IronBeeUtil
 *
 * A queue structure that minimizes memory allocations and
 * recycles internal structures to prevent unbounded memory use. Child
 * memory pools are used to reclaim memory when the queue must reallocate
 * its internal storage.
 *
 * While @ref ib_list_t may be used as a queue, it is ill suited to use
 * as a long-lived object with many elements being added and removed,
 * such as a work queue. Each node element created cannot be released
 * back to the memory pool, by design, and @ref ib_list_t does not
 * recycle list nodes.
 *
 * While the intent of this object is that of a queue, it provides support
 * for un-queue like operations, such as
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

/**
 * Empty flags.
 */
#define IB_QUEUE_NONE         (0x0)

/**
 * Never reduce the size of the allocated internal storage.
 */
#define IB_QUEUE_NEVER_SHRINK (1 << 0)

/**
 * Create a queue.
 *
 * @param[out] queue The created Queue.
 * @param[in] mm The memory manager that @a queue will be allocated from.
 *            The @a queue will be destroyed when @a mm is destroyed.
 * @param[in] flags Options that influence the use of this data structure.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 */
ib_status_t DLL_PUBLIC ib_queue_create(
    ib_queue_t **queue,
    ib_mm_t      mm,
    ib_flags_t   flags
)
NONNULL_ATTRIBUTE(1);

/**
 * Enqueue an element.
 *
 * @param[in] queue The queue.
 * @param[in] element The element to push.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC If a resize cannot get enough memory.
 */
ib_status_t DLL_PUBLIC ib_queue_push_back(
    ib_queue_t *queue,
    void       *element
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
ib_status_t DLL_PUBLIC ib_queue_push_front(
    ib_queue_t *queue,
    void       *element
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
ib_status_t DLL_PUBLIC ib_queue_pop_back(
    ib_queue_t *queue,
    void       *element
);

/**
 * Dequeue and element.
 *
 * @param[in] queue The queue.
 * @param[out] element The element to retrieve.
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC If memory for a new queue cannot be obtained when resizing.
 */
ib_status_t DLL_PUBLIC ib_queue_pop_front(
    ib_queue_t *queue,
    void       *element
);

/**
 * An alias for ib_queue_push_back().
 *
 * @param[in] queue The queue.
 * @param[out] element The element to retrieve.
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC If memory for a new queue cannot be obtained when resizing.
 */
ib_status_t DLL_PUBLIC ib_queue_enqueue(
    ib_queue_t *queue,
    void       *element
);

/**
 * An alias for ib_queue_pop_front().
 *
 * @param[in] queue The queue.
 * @param[out] element The element to retrieve.
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC If memory for a new queue cannot be obtained when resizing.
 */
ib_status_t DLL_PUBLIC ib_queue_dequeue(
    ib_queue_t *queue,
    void       *element
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
ib_status_t DLL_PUBLIC ib_queue_peek(
    const ib_queue_t *queue,
    void             *element
);

/**
 * @param[in] queue The queue.
 * @param[in] index The index to get.
 * @param[out] element The element.
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If the element is not in the range of the queue.
 */
ib_status_t DLL_PUBLIC ib_queue_get(
    const ib_queue_t *queue,
    size_t            index,
    void             *element
);

/**
 * @param[in] queue The queue.
 * @param[in] index The index to set.
 * @param[in] element The element to set.
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If the element is not in the range of the queue.
 */
ib_status_t DLL_PUBLIC ib_queue_set(
    ib_queue_t *queue,
    size_t      index,
    void       *element
);

/**
 * Reserve @a allocation total spaces in the queue.
 *
 * If the new allocation of the queue is greater than ib_queue_size().
 * then only the allocated memory is expanded to accommodate new
 * pushes without requesting more memory.
 *
 * If the new allocation of the queue is less than ib_queue_size().
 * then the queue is truncated and ib_queue_size() will return @a allocation.
 * Any push, in this situation, will cause a resizing of the queue.
 *
 * If @ref IB_QUEUE_NEVER_SHRINK is set and @a allocation is less than
 * the current allocation, no action is taken and @ref IB_OK is returned.
 *
 * @param[in] queue The queue.
 * @param[in] allocation The new allocation.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On an allocation error.
 */
ib_status_t DLL_PUBLIC ib_queue_reserve(
    ib_queue_t *queue,
    size_t      allocation
);

/**
 * Return the current size (depth) of the queue.
 * @param[in] queue The queue.
 * @returns the size (depth) of the queue.
 */
size_t DLL_PUBLIC ib_queue_size(
    const ib_queue_t *queue
);

/**
 * Callback function to process elements in the queue.
 *
 * @param[in] element The element.
 * @param[in] cbdata The callback data for this function call.
 *
 */
typedef void (*ib_queue_element_fn_t)(void *element, void *cbdata);

/**
 * Dequeue all elements by passing them to the function given.
 *
 * This will result in the queue having a size of zero.
 *
 * @param[in] queue The queue to empty.
 * @param[in] fn The function to handle each element.
 * @param[in] cbdata Callback data for @a fn.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on queue resize errors.
 */
ib_status_t DLL_PUBLIC ib_queue_dequeue_all_to_function(
    ib_queue_t             *queue,
    ib_queue_element_fn_t   fn,
    void                   *cbdata
);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* _IB_QUEUE_H_ */
