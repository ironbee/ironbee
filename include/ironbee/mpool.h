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

#ifndef _IB_MPOOL_H_
#define _IB_MPOOL_H_

/**
 * @file
 * @brief IronBee --- Memory Pool Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilMemPool Memory Pool
 * @ingroup IronBeeUtil
 *
 * Memory pool routines.
 *
 * @section thread_safety Thread Safety
 *
 * Let A and B be distinct memory pools.  Say that B is a descendant of A if
 * B is a child of A or a child of a descendant of A.
 *
 * The memory pool code is written to be thread safe for different memory
 * pool families.  That is, if A and B are distinct memory pools where
 * neither is descendant of the other, than any memory pool routine of A
 * can coexist with a simultaneous routine on B.
 *
 * Two import thread safe cases are:
 * - A and B can be simultaneously destroyed or released even if they share a
 *   common parent.
 * - A and B can be simultaneously created even if they share a common parent.
 *
 * Furthermore, all allocation routines can be called on A and B as long as
 * A and B are distinct, i.e, even if one is a descendant of the other.
 *
 * Common scenarios that are not thread safe include:
 * - Simultaneously allocations from the same pool.
 * - Any use of a descendant of a pool while that pool is being cleared,
 *   released, or destroyed.
 *
 * @section Performance
 *
 * This implementation is focused on runtime performance.  In particular,
 * allocation should be very fast and clearing and destroying very fast.  This
 * involves both internal data structures and algorithms and in reducing the
 * number of calls to malloc() and free().
 *
 * As a consequence of the focus on runtime performance, space performance is
 * not optimal.  This trade-off can be somewhat tuned by setting the pagesize
 * for a memory pool.  Largely page sizes will mean higher runtime performance
 * and higher memory wastage.  The minimum pagesize is currently 1024.
 *
 * @section Valgrind
 *
 * If mpool.c is compiled with IB_MPOOL_VALGRIND defined then additional code
 * will be added to communicate with valgrind.  This will allow valgrind to
 * properly track mpool usage and report memory errors.  It adds a small
 * performance penalty and ~8 additional bytes per allocation (slightly more,
 * amortized).
 *
 * @{
 */

/**
 * A memory pool.
 *
 * This type should be used only be pointer and treated as opaque.
 */
typedef struct ib_mpool_t ib_mpool_t;

/**
 * Callback clean up function.
 *
 * Parameter is a pointer to callback data.
 */
typedef void (*ib_mpool_cleanup_fn_t)(void *data);

/**
 * Malloc function.
 *
 * Function memory pool can use to allocate memory.
 * Semantics are as malloc().
 **/
typedef void *(*ib_mpool_malloc_fn_t)(size_t size);

/**
 * Malloc function.
 *
 * Function memory pool can use to free memory.
 * Semantics are as free().
 **/
typedef void (*ib_mpool_free_fn_t)(void *ptr);


/**
 * Create a new memory pool.
 *
 * @note If a pool has a parent specified, then any call to
 * clear/destroy/release on the parent will propagate to all descendants.
 *
 * @param[out] pmp    Address which new pool is written
 * @param[in]  name   Logical name of the pool (used in reports), can be NULL.
 * @param[in]  parent Optional parent memory pool (or NULL)
 *
 * @returns
 * - IB_OK     -- Success.
 * - IB_EALLOC -- Allocation error.
 * - Other     -- Locking failure, see ib_lock_lock().
 */
ib_status_t DLL_PUBLIC ib_mpool_create(
    ib_mpool_t **pmp,
    const char  *name,
    ib_mpool_t  *parent
)
NONNULL_ATTRIBUTE(1);

/**
 * Create a new memory pool with predefined page size.
 *
 * Minimum page size is currently 1024.  Page size should be a power of 2 for
 * best memory usage.
 *
 * @note If a pool has a parent specified, then any call to clear/destroy
 * on the parent will propagate to all descendants.
 *
 * Default parameter values means copy from parent if present or use
 * 4096, malloc(), and free() otherwise.
 *
 * @param[out] pmp       Address which new pool is written
 * @param[in]  name      Logical name of the pool (used in reports), can be
 *                       NULL.
 * @param[in]  parent    Optional parent memory pool (or NULL)
 * @param[in]  pagesize  Custom page size (to be used by default in pmp);
 *                       0 means use default; less than 1024 means 1024.
 * @param[in]  malloc_fn Malloc function to use; NULL means use default.
 * @param[in]  free_fn   Free function to use; NULL means use default.
 *
 * @returns
 * - IB_OK     -- Success.
 * - IB_EALLOC -- Allocation error.
 * - Other     -- Locking failure, see ib_lock_lock().
 */
ib_status_t DLL_PUBLIC ib_mpool_create_ex(
    ib_mpool_t           **pmp,
    const char            *name,
    ib_mpool_t            *parent,
    size_t                 pagesize,
    ib_mpool_malloc_fn_t   malloc_fn,
    ib_mpool_free_fn_t     free_fn
)
NONNULL_ATTRIBUTE(1);

/**
 * Set the name of a memory pool.
 *
 * @param[in] mp   Memory pool to set name of.
 * @param[in] name New name; copied.
 * @returns
 * - IB_OK     -- Success.
 * - IB_EALLOC -- Allocation error.
 */
ib_status_t DLL_PUBLIC ib_mpool_setname(
    ib_mpool_t *mp,
    const char *name
)
NONNULL_ATTRIBUTE(1);

/**
 * Get the name of a memory pool.
 *
 * @param[in] mp Memory pool to fetch name of.
 * @returns Name of @a mp.
 */
const char DLL_PUBLIC *ib_mpool_name(
    const ib_mpool_t *mp
)
NONNULL_ATTRIBUTE(1);

/**
 * Get the amount of memory allocated by a memory pool.
 *
 * This is the sum of the allocations asked for, not the total memory used by
 * the memory pool.
 *
 * @param[in] mp Memory pool to query.
 * @returns Bytes in use.
 */
size_t DLL_PUBLIC ib_mpool_inuse(
    const ib_mpool_t* mp
);

/**
 * Assure that at least @a pages pages are preallocated in the free pages list.
 *
 * If there are already enough pages preallocated, then do nothing.
 *
 * @param[in,out] mp Memory pool to preallocate pages for.
 * @param[in] pages Number of pages to preallocate (must be >0).
 * @returns
 * - IB_OK     -- Success.
 * - IB_EINVAL -- Invalid parameters.
 * - IB_EALLOC -- Allocation error.
 */
ib_status_t DLL_PUBLIC ib_mpool_prealloc_pages(
    ib_mpool_t *mp,
    int pages
);

/**
 * Allocate memory from a memory pool.
 *
 * @param[in] mp   Memory pool to allocate from.
 * @param[in] size Size in bytes to allocate.
 *
 * If @a size is 0, a non-NULL pointer will be returned, but that pointer
 * should never be dereferenced.
 *
 * @returns Address of allocated memory or NULL on any error.
 */
void DLL_PUBLIC *ib_mpool_alloc(
    ib_mpool_t *mp,
    size_t     size
)
NONNULL_ATTRIBUTE(1);

/**
 * Deallocate all memory allocated from the pool and any descendant pools.
 *
 * This does not free the memory but retains it for use in future allocations.
 * To actually return the memory to the underlying memory system, use
 * ib_mpool_destroy().
 *
 * This will call all cleanup functions of @a mp and its descendants.
 *
 * Nothing happens if @a mp is NULL.
 *
 * @param[in] mp Memory pool to clear.
 */
void DLL_PUBLIC ib_mpool_clear(
    ib_mpool_t *mp
);

/**
 * Destroy pool and any descendant pools.
 *
 * This is similar to ib_mpool_clear() except that it returns the memory to
 * the underlying memory system and destroys itself and its descendants.
 *
 * @a mp or any descendant should not be used after calling this.
 *
 * Nothing happens if @a mp is NULL.
 *
 * @param[in] mp Memory pool to destroy.
 */
void DLL_PUBLIC ib_mpool_destroy(
    ib_mpool_t *mp
)
NONNULL_ATTRIBUTE(1);

/**
 * Clear pool and release to parent.
 *
 * If @a mp has no parent, this is identical to ib_mpool_destroy().  If @a mp
 * has a parent, then this is semantically identical to ib_mpool_destroy(),
 * but instead of freeing the pool, it is added to the free subpool list of
 * its parent and will be reused the next time ib_mpool_create() is called
 * with the parent.
 *
 * In the presence of a parent, release is significantly faster than destroy
 * but does not return memory to malloc/free.  It is a good choice if new
 * subpools will be created soon.
 *
 * Release should only be used if all subpools of the parent have the same
 * pagesize, malloc, and free functions.  If these parameters vary, the
 * subpools may not be reused leading to excess memory usage.
 *
 * @param[in] mp Memory pool to release.
 */
void DLL_PUBLIC ib_mpool_release(
    ib_mpool_t *mp
);

/**
 * Register a function to be called when a memory pool is cleared or
 * destroyed.
 *
 * All cleanup functions associated with a memory pool are invoked before any
 * memory associated with @a mp is freed.  Thus, it is safe for a cleanup
 * function to access memory in the pool.
 *
 * @param[in] mp      Memory pool to associate function with.
 * @param[in] cleanup Cleanup function
 * @param[in] data    Data passed to @a cleanup.
 *
 * @returns
 * - IB_OK     -- Success.
 * - IB_EALLOC -- Allocation error.
 */
ib_status_t DLL_PUBLIC ib_mpool_cleanup_register(
    ib_mpool_t            *mp,
    ib_mpool_cleanup_fn_t  cleanup,
    void                  *data
)
NONNULL_ATTRIBUTE(1);

/**
 * Full path of a memory pool.
 *
 * Caller is responsible for freeing return.
 *
 * @param[in] mp Memory pool to get full path of.
 * @return String or NULL on any allocation error.
 */
char DLL_PUBLIC *ib_mpool_path(
    const ib_mpool_t *mp
)
NONNULL_ATTRIBUTE(1);

/**
 * Validate internal consistency of memory pool.
 *
 * This function will analyze @a mp and its children for invariant
 * violations.  Any return value of IB_EOTHER should be reported as a bug
 * along with the result of ib_mpool_debug_report() and any other information.
 *
 * The caller is responsible for freeing @c *message.
 *
 * @param[in]  mp      Memory pool to analyze.
 * @param[out] message Message describing failure or NULL if IB_OK or
 *                     IB_EALLOC.
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EOTHER on failure -- please report as bug.
 */
ib_status_t DLL_PUBLIC ib_mpool_validate(
    const ib_mpool_t  *mp,
    char             **message
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Dump debugging information on memory pool.
 *
 * This provides an extensive report on memory pool intended for developers
 * debugging memory pool related issues.
 *
 * The caller is responsible for freeing the return value.
 *
 * This function is slow.
 *
 * @param[in] mp Memory pool to dump.
 * @returns Debug report.
 */
char DLL_PUBLIC *ib_mpool_debug_report(
    const ib_mpool_t *mp
)
NONNULL_ATTRIBUTE(1);

/**
 * Analyze memory pool usage and return a human consumable report.
 *
 * The caller is responsible for freeing the return value.
 *
 * This function is slow.
 *
 * The report contains the following datapoints for a number of items:
 * - cost       -- Memory allocated, including mpool overhead.
 * - use        -- Memory returned to client.
 * - waste      -- cost - use
 * - efficiency -- use / cost
 * - free       -- Memory allocated and waiting for reuse.
 *
 * The items are:
 * - Tracks           -- Lists allocations by range.  Each track is for all
 *                       allocations to large for the previous track and
 *                       below the listed limit.
 * - Pages            -- Aggregate of all the tracks.
 * - PointerPages     -- Internal structures used to track large allocations.
 * - LargeAllocations -- Bytes returned to caller too large for any track.
 * - Cleanups         -- Overhead for cleanup functions.
 * - Total            -- Aggregate of all of the above.
 *
 * @param[in] mp Memory pool to analyze.
 * @returns Usage report.
 */
char DLL_PUBLIC *ib_mpool_analyze(
    const ib_mpool_t *mp
)
NONNULL_ATTRIBUTE(1);

/**
 * Return the memory pool parent.
 *
 * @param[in] mp The child memory pool whose parent will be returned.
 *
 * @returns Memory pool parent.
 */
ib_mpool_t DLL_PUBLIC *ib_mpool_parent(
    ib_mpool_t *mp
)
NONNULL_ATTRIBUTE(1);

/** @} IronBeeUtilMemPool */

#ifdef __cplusplus
}
#endif

#endif /* _IB_MPOOL_H_ */
