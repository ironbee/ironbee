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
#include <ironbee/types.h>

#include <ironbee/queue.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilResourcePool Resource Pool
 * @ingroup IronBeeUtil
 * @{
 */

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
    void **resource,
    void  *cbdata
);

/**
 * Callback to destroy a resource.
 * @param[out] resource The resource that will be destroyed.
 * @param[in] cbdata Callback data.
 * @returns
 * - IB_OK On success.
 * - Other On error.
 */
typedef ib_status_t (*ib_resource_destroy_fn_t)(
    void *resource,
    void *cbdata
);

/**
 * Callback to inform a resource that it is being signed out for use.
 *
 * Use this to clear temporary values, reset counters, etc.
 *
 * @param[out] resource The resource that will be destroyed.
 * @param[in] cbdata Callback data.
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If this object has become invalid and should be destroyed.
 * - Other On error.
 */
typedef ib_status_t (*ib_resource_preuse_fn_t)(
    void *resource,
    void *cbdata
);

/**
 * Callback to inform a resource that it is being returned to the pool.
 *
 * Use this to clear temporary values, reset counters, etc.
 *
 * @param[out] resource The resource that will be destroyed.
 * @param[in] cbdata Callback data.
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If this object has become invalid and should be destroyed.
 * - Other On error.
 */
typedef ib_status_t (*ib_resource_postuse_fn_t)(
    void *resource,
    void *cbdata
);

/**
 * A pool of resources.
 */
struct ib_resource_pool_t {
    ib_mpool_t *mp;        /**< Memory pool this pool comes from. */
    ib_queue_t *resources; /**< List of free ib_resource_t objs. */
    /**
     * List of empty ib_resource_t structs.
     *
     * Empty ib_resource_t struts are pulled from here, if available,
     * insetad of allocating new ones.
     */
    ib_queue_t *free_list;
    size_t      count;     /**< Number of created resources. */

    /* Callbacks. */
    ib_resource_create_fn_t    create_fn;    /**< Create a resource. */
    void                      *create_data;  /**< Callback data. */
    ib_resource_destroy_fn_t   destroy_fn;   /**< Destroy a resource. */
    void                      *destroy_data; /**< Callback data. */
    ib_resource_preuse_fn_t    preuse_fn;    /**< Pre use callback. */
    void                      *preuse_data;  /**< Callback data. */
    ib_resource_postuse_fn_t   postuse_fn;   /**< Post use callback. */
    void                      *postuse_data; /**< Callback data. */

    /**
     * The total number of resources should never exceed this value.
     */
    size_t      max_count;

    /**
     * The total number of resource should never drop below this value.
     *
     * If a resource being destroyed reduce count below this value,
     * a new resource is immediately created and inserted into the
     * free list.
     */
    size_t      min_count; /**< Never have fewer than these. */

    /**
     * Resources used more than this value are destroyed.
     *
     * To prevent possible memory leaks in resources IronBee may not 
     * control, this limit gives IronBee the means to occasionally
     * destroy a resource.
     */
    size_t      max_use;   /**< Limit the lifetime of a resource. */
};
typedef struct ib_resource_pool_t ib_resource_pool_t;

/**
 * This represents a resource to be managed by an ib_resource_pool_t.
 */
struct ib_resource_t {
    ib_resource_pool_t *owner;    /**< What pool did this come from. */
    void               *resource; /**< Pointer to the user resource. */
    size_t              use;      /**< Number of times this has been used. */
};
typedef struct ib_resource_t ib_resource_t;

/**
 * This callback cleanly allows a caller to use a resource.
 *
 * This callback is passed to the ib_resource_use() function
 * and facilitates signing out a resource, and signing it back in.
 * The user may use the resource between those calls.
 *
 * This is a safer use pattern than signing out a resource as it 
 * guarnatees that the resource will be returned.
 *
 * If the resource is damanged during use the user must set 
 * @a ib_resource_rc to a value that is not IB_OK. This will 
 * tell ib_resource_use() to destroy the object and replace it.
 *
 * @param[in] resource The resource to use.
 * @param[in] user_rc The return code for the user to use.
 * @param[in] cbdata Callback data.
 * @returns The return status that the user would like
 *          ib_resource_use() to return.
 */
typedef ib_status_t (*ib_resource_use_fn_t)(
    ib_resource_t *resource,
    ib_status_t   *user_rc,
    void          *cbdata
);

/**
 * Create a new resource pool.
 *
 * This pool will be destroyed when the memory pool is destroyed.
 *
 * @param[out] resource_pool The resource pool we are creating.
 * @param[in] mp The memory pool that manages this resource pool.
 *               This memory pool will destroy the resource pool
 *               and all resources. It must not do so while
 *               other threads hold resources into this resource pool.
 * @param[in] min_count If non-zero, this limits the minimum number
 *            of resources that are managed by this pool.
 *            If a resource is destroyed and the count drops below
 *            this number, another resource will be created.
 * @param[in] max_count If non-zero, this limits the maximum number
 *            of resources that are managed by this pool.
 *            If an attempt to create a new resource
 *            when the count is already equal to this value,
 *            the creation routing must spin-wait or
 *            return IB_DECLINE.
 * @param[in] max_use If non-zero, this limits the number of times a resource
 *            can be used. When that number is exceeded, the resource
 *            is destroyed. 
 * @param[in] create_fn This function creates the resource.
 * @param[in] create_data Callback data.
 * @param[in] destroy_fn Destroy a resource.
 * @param[in] destroy_data Callback data.
 * @param[in] preuse_fn Called when a resource is removed from the pool
 *            for use by the client. This is not called when a resource
 *            is removed from the pool to be destroyed.
 *            This may be NULL.
 * @param[in] preuse_data Callback data.
 * @param[in] postuse_fn Called when a resource is returned to the pool.
 *            This may be NULL.
 * @param[in] postuse_data Callback data.
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If @a max_count is less than @a min_count.
 * - IB_EALLOC On allocation errors.
 */
ib_status_t ib_resource_pool_create(
    ib_resource_pool_t       **resource_pool,
    ib_mpool_t                *mp,
    size_t                     min_count,
    size_t                     max_count,
    size_t                     max_use,
    ib_resource_create_fn_t    create_fn,
    void                      *create_data,
    ib_resource_destroy_fn_t   destroy_fn,
    void                      *destroy_data,
    ib_resource_preuse_fn_t    preuse_fn,
    void                      *preuse_data,
    ib_resource_postuse_fn_t   postuse_fn,
    void                      *postuse_data
);

/**
 * Get a resource from the free list, or create it.
 * 
 * If there are no resources in the free list, this calls the create function.
 *
 * All resources that are aquired through ib_resource_get() 
 * must be retured with ib_resource_return().
 *
 * @param[in] resource_pool The resource pool.
 * @param[in] block If true, then this function will wait 1 second
 *            and retry until it can aquire a resource.
 * @param[out] resource The resource to get.
 *
 * @returns
 * - IB_OK If a resource is aquired.
 * - IB_DECLINED If @a block is false and there are no resources
 *               in the free list.
 * - Other on unexpected errors.
 */
ib_status_t ib_resource_get(
    ib_resource_pool_t *resource_pool,
    bool block,
    ib_resource_t **resource
);

/**
 * Return the given resource to its resource pool.
 *
 * This resource will be put in the free list or, possibly,
 * destroyed if its use count is too high.
 *
 * @param[in] resource The resource to return.
 * @returns
 * - IB_OK On success.
 */
ib_status_t ib_resource_return(
    ib_resource_t *resource
);

/**
 * Sign out a resource and use it.
 * @param[in] resource_pool The pool to take a resource from.
 * @param[in] block If true, this will block to get a resource.
 * @param[in] fn The user function that will use the resource.
 * @param[out] user_rc This is passed to @a fn with the value of IB_OK set.
 *             The user may use this to communicate success or failure
 *             of @ fn.
 * @param[in] cbdata Callback data passed to @a fn.
 * @returns 
 * - IB_OK If @a fn was executed successfully.
 * - IB_DECLINE If no resource is available and block is false.
 */
ib_status_t ib_resource_use(
    ib_resource_pool_t   *resource_pool,
    bool                  block,
    ib_resource_use_fn_t  fn,
    ib_status_t          *user_rc,
    void                 *cbdata
);

/** @} IronBeeUtilResourcePool */

#ifdef __cplusplus
}
#endif

#endif /* _IB_RESOURCE_POOL_H_ */

