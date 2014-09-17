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
 * @brief IronBee --- Lock Utilities
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#ifndef _IB_LOCK_H_
#define _IB_LOCK_H_

#include <ironbee/build.h>
#include <ironbee/types.h>
#include <ironbee/mm.h>

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilLocking Locking
 * @ingroup IronBeeUtil
 * Locking primitives.
 * @{
 */

/**
 * @brief The lock type for ironbee locks.
 */
typedef pthread_mutex_t ib_lock_t;

/**
 * Create a new lock using the given memory manager.
 *
 * Locks exist only as pointers because it is invalid to copy a lock.
 * To put this another way, doing
 * @code
 * *lock_copy = *lock
 * @endcode
 * results in undefined behavior.
 *
 * @param[in] lock The lock.
 * @param[in] mm The memory manager.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC If the lock cannot be allocated or initialized.
 * - IB_EOTHER If the lock cannot be schedule for destruction in @a mm.
 */
ib_status_t DLL_PUBLIC ib_lock_create(ib_lock_t **lock, ib_mm_t mm);

/**
 * @param[in] lock The lock.
 */
ib_status_t DLL_PUBLIC ib_lock_lock(ib_lock_t *lock);

/**
 * @param[in] lock The lock.
 */
ib_status_t DLL_PUBLIC ib_lock_unlock(ib_lock_t *lock);

/**
 * Create a lock when there is no memory manager available.
 *
 * This function should not be used except to implement memory managers.
 *
 * @param[in] lock The lock.
 */
ib_status_t DLL_PUBLIC ib_lock_create_malloc(ib_lock_t **lock);

/**
 * Destroy a lock created by ib_lock_create_malloc().
 *
 * This function should not be used except to implement memory managers.
 *
 * @param[in] lock The lock.
 */
void DLL_PUBLIC ib_lock_destroy_malloc(ib_lock_t *lock);

/**
 * @} IronBeeUtilLocking Locking
 */

#ifdef __cplusplus
}
#endif

#endif // _IB_LOCK_H_
