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
 * While @ref ib_list_t may be used as a queue, it is illsuited to use
 * as a long-lived object with many elements being added and removed,
 * such as a work queue. Each node element created cannot be released
 * back to the memory pool, by design, and @ref ib_list_t does not
 * recycle list nodes.
 *
 * To accomdate the need for long-lived objects, queue does not use memory
 * pools for its allocations.
 *
 * @{
 */

/**
 * Queue data structure.
 */
typedef struct ib_queue_t ib_queue_t;

/**
 * Create a queue.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 */
ib_status_t ib_queue_create(ib_queue_t **queue);

ib_status_t ib_queue_push_back(ib_queue_t *queue, void *element);
ib_status_t ib_queue_push_front(ib_queue_t *queue, void *element);
ib_status_t ib_queue_pop_back(ib_queue_t *queue, void **element);
ib_status_t ib_queue_pop_front(ib_queue_t *queue, void **element);
ib_status_t ib_queue_peek(ib_queue_t *queue, void **element);
ib_status_t ib_queue_get(ib_queue_t *queue, size_t index, void **element);
ib_status_t ib_queue_set(ib_queue_t *queue, size_t index, void *element);


/**
 * Return the current size (depth) of the queue.
 * @param[in] queue The queue.
 * @returns the size (depth) of the queue.
 */
size_t ib_queue_size(
    ib_queue_t *queue
);

/**
 * Destroy the given queue.
 */
void ib_queue_destroy(ib_queue_t *queue);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* _IB_QUEUE_H_ */

