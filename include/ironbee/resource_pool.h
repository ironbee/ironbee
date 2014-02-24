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

#ifndef _IB_RESOURCE_POOL_H_
#define _IB_RESOURCE_POOL_H_

/**
 * @file
 * @brief IronBee --- Resource Pool Utility Functions
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/queue.h>
#include <ironbee/types.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilResourcePool Resource Pool
 * @ingroup IronBeeUtil
 * @{
 */

typedef struct ib_resource_pool_t ib_resource_pool_t;
typedef struct ib_resource_t ib_resource_t;

/**
 * Callback to create a new resource.
 *
 * @param[out] resource The resource created is placed here.
 * @param[in] cbdata Callback data passed.
 * @returns
 * - IB_OK On success.
 * - Other On error.
 */
typedef ib_status_t (*ib_resource_create_fn_t)(
    void *resource,
    void *cbdata
);

/**
 * Callback to destroy a resource.
 *
 * @param[out] resource The resource that will be destroyed.
 * @param[in] cbdata Callback data.
 */
typedef void (*ib_resource_destroy_fn_t)(
    void *resource,
    void *cbdata
);

/**
 * Callback to inform a resource that it is being acquired for use.
 *
 * Use this to clear temporary values, reset counters, etc.
 *
 * @param[out] resource The resource that will be used.
 * @param[in] cbdata Callback data.
 */
typedef void (*ib_resource_preuse_fn_t)(
    void *resource,
    void *cbdata
);

/**
 * Callback to inform a resource that it is being returned to the pool.
 *
 * Use this to clear temporary values, reset counters, etc.
 *
 * @param[out] resource The resource that will be returned to the pool.
 * @param[in] cbdata Callback data.
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If this object has become invalid and should be destroyed.
 */
typedef ib_status_t (*ib_resource_postuse_fn_t)(
    void *resource,
    void *cbdata
);

/**
 * Create a new resource pool.
 *
 * @param[out] resource_pool The resource pool created.
 * @param[in] mm The memory manager that this resource pool is
 *            allocated from, as well as future resources
 *            that are created.
 *            This memory pool will destroy the resource pool
 *            and all resources. It must not do so while
 *            other threads hold resources into this resource pool.
 * @param[in] min_count If non-zero, this limits the minimum number
 *            of resources that are managed by this pool.
 *            If a resource is destroyed and the count drops below
 *            this number, another resource will be created.
 * @param[in] max_count If non-zero, this limits the maximum number
 *            of resources that are managed by this pool.
 *            If an attempt to create a new resource is made
 *            when the count is already equal to this value,
 *            the creation routing must spin-wait or
 *            return IB_DECLINED.
 * @param[in] create_fn This function creates the resource.
 * @param[in] create_data Callback data.
 * @param[in] destroy_fn Destroy a resource.
 * @param[in] destroy_data Callback data.
 * @param[in] preuse_fn Called when a resource is acquired from the pool
 *            for use by the client. This is not called when a resource
 *            is removed from the pool for destruction.
 *            This may be NULL.
 * @param[in] preuse_data Callback data.
 * @param[in] postuse_fn Called when a resource is released to the pool.
 *            This may be NULL.
 * @param[in] postuse_data Callback data.
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If @a max_count and @a min_count are greater than 0 and
 *             @a max_count is less than @a min_count.
 * - IB_EALLOC On allocation errors.
 */
ib_status_t DLL_PUBLIC ib_resource_pool_create(
    ib_resource_pool_t       **resource_pool,
    ib_mm_t                    mm,
    size_t                     min_count,
    size_t                     max_count,
    ib_resource_create_fn_t    create_fn,
    void                      *create_data,
    ib_resource_destroy_fn_t   destroy_fn,
    void                      *destroy_data,
    ib_resource_preuse_fn_t    preuse_fn,
    void                      *preuse_data,
    ib_resource_postuse_fn_t   postuse_fn,
    void                      *postuse_data
)
NONNULL_ATTRIBUTE(1);

/**
 * Acquire a resource, creating a new one if necessary.
 *
 * All resources that are acquired through ib_resource_acquire() must be
 * returned with ib_resource_return().
 *
 * @param[in] resource_pool The resource pool.
 * @param[out] resource The resource to get.
 *
 * @returns
 * - IB_OK If a resource is acquired.
 * - IB_DECLINED If there are no resources in the free queue.
 * - Other on unexpected errors.
 */
ib_status_t DLL_PUBLIC ib_resource_acquire(
    ib_resource_pool_t *resource_pool,
    ib_resource_t **resource
);

/**
 * Return the given resource to its resource pool.
 *
 * This resource will be put in the free queue or, possibly,
 * destroyed if its use count is too high.
 *
 * @param[in] resource The resource to return.
 * @returns
 * - IB_OK On success.
 */
ib_status_t DLL_PUBLIC ib_resource_release(
    ib_resource_t *resource
);

/**
 * Destroy all elements in the pool and re-fill it to the minimum value.
 *
 * @param[in] resource_pool The resource pool
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC Allocation failures.
 * - Other if the user create function fails when refilling the pool.
 */
ib_status_t DLL_PUBLIC ib_resource_pool_flush(
    ib_resource_pool_t *resource_pool
);

/**
 * Get the user's resource from a @ref ib_resource_t.
 */
void DLL_PUBLIC *ib_resource_get(
    const ib_resource_t* resource
);

/**
 * Get the number of times this resource has been used.
 * @returns The number of times this resource has been used.
 */
size_t DLL_PUBLIC ib_resource_use_get(
    const ib_resource_t* resource
);

/** @} IronBeeUtilResourcePool */

#ifdef __cplusplus
}
#endif

#endif /* _IB_RESOURCE_POOL_H_ */
