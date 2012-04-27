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
 *****************************************************************************/

#ifndef _IB_MPOOL_H_
#define _IB_MPOOL_H_

/**
 * @file
 * @brief IronBee &mdash; Memory Pool Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
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
 * @{
 */

typedef ib_status_t (*ib_mpool_cleanup_fn_t)(void *);

/**
 * Create a new memory pool.
 *
 * @note If a pool has a parent specified, then any call to clear/destroy
 * on the parent will propagate to all descendants.
 *
 * @param pmp Address which new pool is written
 * @param name Logical name of the pool (for debugging purposes)
 * @param parent Optional parent memory pool (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_mpool_create(ib_mpool_t **pmp,
                                       const char *name,
                                       ib_mpool_t *parent);

/**
 * Create a new memory pool with predefined page size.
 * Minimum size is IB_MPOOL_MIN_PAGE_SIZE (currently 512)
 *
 * @note If a pool has a parent specified, then any call to clear/destroy
 * on the parent will propagate to all descendants.
 *
 * @param pmp Address which new pool is written
 * @param name Logical name of the pool (for debugging purposes)
 * @param parent Optional parent memory pool (or NULL)
 * @param size Custom page size (to be used by default in pmp)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_mpool_create_ex(ib_mpool_t **pmp,
                                          const char *name,
                                          ib_mpool_t *parent,
                                          size_t size);


/**
 * Set the name of a memory pool.
 *
 * @param mp Memory pool
 * @param name New name
 */
void DLL_PUBLIC ib_mpool_setname(ib_mpool_t *mp, const char *name);

/**
 * Get the name of a memory pool.
 *
 * @param mp Memory pool.
 * @returns name
 */
const char DLL_PUBLIC *ib_mpool_name(const ib_mpool_t* mp);

/**
 * Allocate memory from a memory pool.
 *
 * @param mp Memory pool
 * @param size Size in bytes
 *
 * @returns Address of allocated memory
 */
void DLL_PUBLIC *ib_mpool_alloc(ib_mpool_t *mp, size_t size);

/**
 * Allocate memory from a memory pool and initialize to zero.
 *
 * @param mp Memory pool
 * @param nelem Number of elements to allocate
 * @param size Size of each element in bytes
 *
 * @returns Address of allocated memory
 */
void DLL_PUBLIC *ib_mpool_calloc(ib_mpool_t *mp, size_t nelem, size_t size);

/**
 * Duplicate a string.
 *
 * @param mp Memory pool
 * @param src String to copy
 *
 * @returns Address of the duplicated string
 */
char DLL_PUBLIC *ib_mpool_strdup(ib_mpool_t *mp, const char *src);

/**
 * Duplicate a memory block.
 *
 * @param mp Memory pool
 * @param src Memory addr
 * @param size Size of memory
 *
 * @returns Address of duplicated memory
 */
void DLL_PUBLIC *ib_mpool_memdup(ib_mpool_t *mp, const void *src, size_t size);

/**
 * Deallocate all memory allocated from the pool and any descendant pools.
 *
 * @param mp Memory pool
 */
void DLL_PUBLIC ib_mpool_clear(ib_mpool_t *mp);

/**
 * Destroy pool and any descendant pools.
 *
 * @param mp Memory pool
 */
void DLL_PUBLIC ib_mpool_destroy(ib_mpool_t *mp);

/**
 * Register a function to be called when a memory pool is destroyed.
 *
 * @param mp Memory pool
 * @param cleanup Cleanup function
 * @param data Data passed to cleanup function
 *
 * @returns Status code
 */
ib_status_t ib_mpool_cleanup_register(ib_mpool_t *mp,
                                      ib_mpool_cleanup_fn_t cleanup,
                                      void *data);

/** @} IronBeeUtilMemPool */

#ifdef __cplusplus
}
#endif

#endif /* _IB_MPOOL_H_ */
