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

#ifndef _IB_MPOOL_LITE_H_
#define _IB_MPOOL_LITE_H_

/**
 * @file
 * @brief IronBee --- Memory Pool Lite
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilMemPoolLite Memory Pool Lite
 * @ingroup IronBeeUtil
 *
 * A simple memory pool.
 *
 * A lite mpool provides minimal functionality needed by @ref ib_mm_t and
 * lacks the memory reuse, hierarchical nature, introspection, and so forth of
 * @ref ib_mpool_t.  However, it has lower memory overhead, especially
 * for small numbers of allocations.
 *
 * A lite mpool uses `2+3C+N` pointers where `C` is the number of cleanup
 * functions and `N` is the number of allocations.
 *
 * To keep this code minimal, malloc() and free() are used.  There is no
 * support for alternative allocators.
 *
 * @{
 */

/**
 * Cleanup function.
 *
 * @param[in] cbdata Callback data.
 **/
typedef void (*ib_mpool_lite_cleanup_fn_t)(void *cbdata);

/**
 * Simple memory pool.
 *
 * @sa IronBeeUtilMemPoolLite
 **/
typedef struct ib_mpool_lite_t ib_mpool_lite_t;

/**
 * Create mpool lite.
 *
 * @param[out] pool Where to store new pool.
 * @returns
 * - IB_EALLOC on allocation failure.
 * - IB_OK on success.
 **/
ib_status_t DLL_PUBLIC ib_mpool_lite_create(
    ib_mpool_lite_t **pool
)
NONNULL_ATTRIBUTE(1);

/**
 * Destroy mpool lite.
 *
 * Cleanup functions will be called in reverse order of registration and
 * before any memory is freed.
 *
 * @param[in] pool Pool to destroy.
 **/
void ib_mpool_lite_destroy(
    ib_mpool_lite_t *pool
)
NONNULL_ATTRIBUTE(1);

/**
 * Allocate memory from mpool lite.
 *
 * @param[in] pool Pool to allocate from.
 * @param[in] size Number of bytes to allocate.
 * @returns
 * - NULL on allocation error.
 * - Pointer to buffer of size @a size on success.
 **/
void *ib_mpool_lite_alloc(
    ib_mpool_lite_t *pool,
    size_t           size
)
NONNULL_ATTRIBUTE(1);

/**
 * Register cleanup function.
 *
 * @param[in] pool   Pool to register with.
 * @param[in] fn     Function to register.
 * @param[in] cbdata Callback data for @a fn.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 **/
ib_status_t ib_mpool_lite_register_cleanup(
    ib_mpool_lite_t            *pool,
    ib_mpool_lite_cleanup_fn_t  fn,
    void                       *cbdata
)
NONNULL_ATTRIBUTE(1, 2);

/** @} IronBeeUtilMemPoolLite */

#ifdef __cplusplus
}
#endif

#endif /* _IB_MPOOL_LITE_H_ */
