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

#ifndef _IB_MM_H_
#define _IB_MM_H_

/**
 * @file
 * @brief IronBee --- Memory Manager Interface
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
 * @defgroup IronBeeUtilMM Memory Manager
 * @ingroup IronBeeUtil
 *
 * Memory Manager interface and helpers.
 *
 * Memory Manager (MM) is a lightweight interface to a memory management
 * system.  It is a pass-by-copy structure, @ref ib_mm_t, and some helper
 * routines.
 *
 * A user will instantiate a concrete memory managemet system such as a
 * memory pools (@ref ib_mpool_t) and then provide an @ref ib_mm_t to routines
 * that need to ask for memory.  It is assumed that, at some point, the
 * memory management system will reclaim all the memory it has given out.
 *
 * @{
 */

/**
 * Cleanup function.
 *
 * A cleanup function to be called when all memory managed is released.
 *
 * @param[in] cbdata Callback data.
 **/
typedef void (*ib_mm_cleanup_fn_t)(
    void *cbdata
);

/**
 * Allocation function.
 *
 * Routine to ask for memory.
 *
 * @param[in] size Number of bytes to provide.
 * @param[in] cbdata Callback data.
 * @return Allocated buffer or NULL on error.
 **/
typedef void *(*ib_mm_alloc_fn_t)(
    size_t size,
    void   *cbdata
);

/**
 * Cleanup registration function.
 *
 * @param[in] fn Function to register.
 * @param[in] fndata Callback data for @a fn.
 * @param[in] cbdata Callback data.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - Other on different failure.
 **/
typedef ib_status_t (*ib_mm_register_cleanup_fn_t)(
    ib_mm_cleanup_fn_t  fn,
    void               *fndata,
    void               *cbdata
);

/**
 * Memory Manager
 *
 * An interface to a memory management system.  Allows for allocation and
 * registration of cleanup functions.  Should be passed by copy.
 **/
struct ib_mm_t
{
    /** Allocation function. */
    ib_mm_alloc_fn_t alloc;
    /** Callback data for alloc.*/
    void *alloc_data;
    /** Cleanup registration function. */
    ib_mm_register_cleanup_fn_t register_cleanup;
    /** Callback data for register_cleanup. */
    void *register_cleanup_data;
};
typedef struct ib_mm_t ib_mm_t;

/**
 * Allocate memory.
 *
 * @param[in] mm Memory manager to ask for memory.
 * @param[in] size Size of buffer to allocate.
 * @return Pointer to buffer or NULL on error.
 **/
void DLL_PUBLIC *ib_mm_alloc(
    ib_mm_t mm,
    size_t  size
);

/**
 * Register a cleanup function.
 *
 * Cleanup functions should be called in reverse order of registration and
 * before any memory is released.
 *
 * @param[in] mm Memory manager to register with.
 * @param[in] fn Cleanup function to register.
 * @param[in] fndata Callback data for @a fn.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - Other on different failure.
 **/
ib_status_t DLL_PUBLIC ib_mm_register_cleanup(
    ib_mm_t             mm,
    ib_mm_cleanup_fn_t  fn,
    void               *fndata
)
NONNULL_ATTRIBUTE(2);

/* Helpers */

/**
 * Allocate memory and fill with zeros.
 *
 * @param[in] mm Memory manager to ask for memory.
 * @param[in] count Number of elements.
 * @param[in] size Size of an element.
 * @return Pointer to buffer of size `count*size` filled with zeros or NULL
 *         on error.
 **/
void DLL_PUBLIC *ib_mm_calloc(
    ib_mm_t mm,
    size_t  count,
    size_t  size
);

/**
 * Duplicate a NUL-terminated string.
 *
 * @param[in] mm Memory manager to ask for memory.
 * @param[in] src NUL-terminated string to duplicate.
 * @return Copy of @a src including NUL or NULL on error.
 **/
char DLL_PUBLIC *ib_mm_strdup(
    ib_mm_t     mm,
    const char *src
)
NONNULL_ATTRIBUTE(2);

/**
 * Duplicate a span of memory.
 *
 * @param[in] mm Memory manager to ask for memory.
 * @param[in] src Beginning of memory to duplicate.
 * @param[in] size Number of bytes to duplicate.
 * @return Copy of `[src, src + size)` or NULL on error.
 **/
void DLL_PUBLIC *ib_mm_memdup(
    ib_mm_t     mm,
    const void *src,
    size_t      size
)
NONNULL_ATTRIBUTE(2);

/**
 * Duplicae a span of memory and append a NUL.
 * @param[in] mm Memory manager to ask for memory.
 * @param[in] src Beginning of memory to duplicate.
 * @param[in] size Number of bytes to duplicate.
 * @return Copy of `[src, src + size)` followed by NUL or NULL on error.
 **/
char DLL_PUBLIC *ib_mm_memdup_to_str(
    ib_mm_t     mm,
    const void *src,
    size_t      size
)
NONNULL_ATTRIBUTE(2);

/** @} IronBeeUtilMM */

#ifdef __cplusplus
}
#endif

#endif /* _IB_MM_H_ */
