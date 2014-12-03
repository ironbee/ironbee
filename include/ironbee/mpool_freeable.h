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

#ifndef _IB_MPOOL_FREEABLE_H_
#define _IB_MPOOL_FREEABLE_H_

/**
 * @file
 * @brief IronBee --- Memory Pool Freeable Utility Functions
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilMemPoolFreeable Memory Pool, Freeable
 * @ingroup IronBeeUtil
 *
 * A memory pool that, during its life time, can release most memory
 * back to the OS. When this memory pool is destroyed, all
 * memory is released.
 *
 * To efficiently support this, this memory pool is conceptually two
 * memory pools coexisting.
 *
 * The first allocates small segments in a pattern similar to an
 * @ref ib_mpool_t. There are allocation lists. When freeing
 * memory from these segments the lists must be searched.
 * Small allocations have zero tracking information associated
 * with them.
 *
 * The second memory pool allocates big segments of memory, defined as
 * any memory segment that the small allocator does not allocate.
 * Large allocators have tracking information preceding the actual
 * segment starting address returned. This tracking information
 * is used to quickly find and free segments.
 *
 * Allocating is fast for all types of memory. Freeing memory is
 * slow in that the small allocation lists are first searched for
 * a segment. If a developer knows that they want to allocate
 * and handle only large memory segments, they may do so with a special
 * API dealing with an ib_mpool_freeable_segment_t.
 *
 * @{
 */

/**
 * Type used for directly managing large allocations.
 *
 * Operations on this incur a constant cost, in memory, for tracking
 * the allocation but are constant time to allocate and free.
 */
typedef struct ib_mpool_freeable_segment_t ib_mpool_freeable_segment_t;

/**
 * Callback function to cleanup when a page is destroyed.
 *
 * @param[in] cbdata Callback data.
 */
typedef void(* ib_mpool_freeable_segment_cleanup_fn_t)(void *cbdata);

/**
 * Callback function to cleanup when a pool is destroyed.
 *
 * @param[in] cbdata Callback data.
 */
typedef void(* ib_mpool_freeable_cleanup_fn_t)(void *cbdata);

/**
 * Freeable memory pool.
 */
typedef struct ib_mpool_freeable_t ib_mpool_freeable_t;

/**
 * @name Memory Pool API
 *
 * @{
 */

/**
 * Create a memory pool that can free segments.
 *
 * @param[out] mp The memory pool.
 *
 * @returns
 * - IB_OK On success.
 * -IB_EALLOC If a memory allocation fails.
 * - Other on mutex initialization failure.
 */
ib_status_t DLL_PUBLIC ib_mpool_freeable_create(
    ib_mpool_freeable_t **mp
) NONNULL_ATTRIBUTE(1);

/**
 * Allocate from the pool.
 *
 * @param[in] mp The pool to allocate from.
 * @param[in] size The size of the allocation. If 0 is given
 *            a non-NULL pointer to static memory is returned.
 *
 * @returns A pointer to the memory segment or NULL on error.
 */
void DLL_PUBLIC * ib_mpool_freeable_alloc(
    ib_mpool_freeable_t *mp,
    size_t               size
) NONNULL_ATTRIBUTE(1);

/**
 * Reduce the reference count of this memory by 1 and free of equal to 0.
 *
 * If the memory segment is freed then all the associated cleanup functions
 * are called in reverse order of their registration.
 *
 * @param[in] mp The memory pool
 * @param[in] segment The segment to release.
 */
void DLL_PUBLIC ib_mpool_freeable_free(
    ib_mpool_freeable_t *mp,
    void                *segment
) NONNULL_ATTRIBUTE(1);

/**
 * Add a reference to this memory segment.
 *
 * This increases the reference count of a particular memory segment.
 *
 * @param[in] mp Memory pool.
 * @param[in] segment The segment.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EOTHER On a locking failure.
 * - IB_EINVAL If this segment was not allocated from the given memory pool.
 */
ib_status_t DLL_PUBLIC ib_mpool_freeable_ref(
    ib_mpool_freeable_t *mp,
    void                *segment
) NONNULL_ATTRIBUTE(1);

/**
 * Register a cleanup function for when this pool is destroyed.
 *
 * @param[in] mp The memory pool.
 * @param[in] fn Function pointer.
 * @param[in] cbdata Callback data passed to @a fn.
 *
 * @returns
 * - IB_OK On scucess.
 * - IB_EOTHER If locking fails.
 * - IB_EALLOC If a cleanup structure cannot be allocated.
 */
ib_status_t DLL_PUBLIC ib_mpool_freeable_register_cleanup(
    ib_mpool_freeable_t            *mp,
    ib_mpool_freeable_cleanup_fn_t  fn,
    void                           *cbdata
) NONNULL_ATTRIBUTE(1);

/**
 * Register a cleanup function for when this allocation is freed.
 *
 * @param[in] mp The memory pool.
 * @param[in] alloc The memory allocation.
 * @param[in] fn Function pointer.
 * @param[in] cbdata Callback data passed to @a fn.
 *
 * @returns
 * - IB_OK On scucess.
 * - IB_EINVAL If @a segment is the result of a NULL allocation
 *             it may never be freed and does not have
 *             a list of cleanup functions.
 * - IB_EOTHER If locking fails.
 * - IB_EALLOC If a cleanup structure cannot be allocated.
 */
ib_status_t DLL_PUBLIC ib_mpool_freeable_alloc_register_cleanup(
    ib_mpool_freeable_t                    *mp,
    void                                   *alloc,
    ib_mpool_freeable_segment_cleanup_fn_t  fn,
    void                                   *cbdata
) NONNULL_ATTRIBUTE(1);

/**
 * Destroy this memory pool and all undestroyed segments allocated from it.
 *
 * @param[in] mp The memory pool.
 */
void DLL_PUBLIC ib_mpool_freeable_destroy(ib_mpool_freeable_t *mp)
NONNULL_ATTRIBUTE(1);

/**
 * @}
 */

/**
 * @name Segment API
 *
 * Segment API for using this memory pool to manage large allocations.
 * Large allocations incur tracking cost on the order of less than 100 bytes
 * but gain O(1) complexity for all operations.
 *
 * @{
 */

/**
 * Allocate memory using a segment.
 * Like uses of malloc, this returns the segment object or NULL on an error.
 *
 * @returns A pointer to the segment object or NULL on an error.
 */
ib_mpool_freeable_segment_t DLL_PUBLIC * ib_mpool_freeable_segment_alloc(
    ib_mpool_freeable_t *mp,
    size_t size
)
NONNULL_ATTRIBUTE(1);

/**
 * Free the given segment.
 *
 * If the segment has multiple references to it then the segment is not
 * actually freed and the callback list of functions is not called.
 *
 * @param[in] mp The memory pool of this segment.
 * @param[out] segment Free the segment if there is only reference to it.
 */
void DLL_PUBLIC ib_mpool_freeable_segment_free(
    ib_mpool_freeable_t         *mp,
    ib_mpool_freeable_segment_t *segment
) NONNULL_ATTRIBUTE(1);

/**
 * Add a reference to this memory segment.
 *
 * @param[in] mp The memory pool of this segment.
 * @param[out] segment The segment to increase the reference of.
 *
 * @return
 * - IB_OK On success.
 * - IB_EINVAL If this segment was allocated from different memory pool.
 * - Other on another failure.
 */
ib_status_t DLL_PUBLIC ib_mpool_freeable_segment_ref(
    ib_mpool_freeable_t         *mp,
    ib_mpool_freeable_segment_t *segment
) NONNULL_ATTRIBUTE(1);

/**
 * Return the pointer to the base of the memory segment the user requested.
 *
 * @param[in] segment The segment to get the reference out of.
 *
 * @returns The pointer to the requested memory segment.
 */
void DLL_PUBLIC * ib_mpool_freeable_segment_ptr(
    ib_mpool_freeable_segment_t *segment
) NONNULL_ATTRIBUTE(1);

/**
 * Register a cleanup function to be called when this segment is freed.
 *
 * @param[in] mp The memory of this segment.
 * @param[in] segment The memory segment.
 * @param[in] fn Function pointer.
 * @param[in] cbdata Callback data passed to @a fn.
 *
 * @returns
 * - IB_OK On scucess.
 * - IB_EINVAL If @a segment is the result of a NULL allocation
 *             it may never be freed and does not have
 *             a list of cleanup functions.
 * - IB_EOTHER If locking fails.
 * - IB_EALLOC If a cleanup structure cannot be allocated.
 */
ib_status_t DLL_PUBLIC ib_mpool_freeable_segment_register_cleanup(
    ib_mpool_freeable_t                    *mp,
    ib_mpool_freeable_segment_t            *segment,
    ib_mpool_freeable_segment_cleanup_fn_t  fn,
    void                                   *cbdata
) NONNULL_ATTRIBUTE(1);

/** @} IronBeeUtilMemPoolFreeableSegment */

/** @} IronBeeUtilMemPoolFreeable */

#ifdef __cplusplus
}
#endif

#endif /* _IB_MPOOL_FREEABLE_H_ */
