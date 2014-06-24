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
 * @brief IronBee --- Resource Pool Implementation
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 * @nosubgrouping
 */

#include "ironbee_config_auto.h"

#include <ironbee/resource_pool.h>
#include <ironbee/util.h>

#include <assert.h>
#include <unistd.h>

/**
 * This represents a resource to be managed by an ib_resource_pool_t.
 */
struct ib_resource_t {
    ib_resource_pool_t *owner;    /**< What pool did this come from. */
    void               *resource; /**< Pointer to the user resource. */
    size_t              use;      /**< Number of times this has been used. */
};

/**
 * A pool of resources.
 */
struct ib_resource_pool_t {
    ib_mm_t     mm;        /**< The memory manager for this pool. */
    ib_queue_t *resources; /**< List of free ib_resource_t objs. */
    /**
     * List of empty @ref ib_resource_t.
     *
     * Empty @ref ib_resource_t are pulled from here, if available,
     * instead of allocating new ones.
     */
    ib_queue_t *free_queue;
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
    const size_t max_count;

    /**
     * The total number of resource should never drop below this value.
     *
     * If a resource being destroyed reduce count below this value,
     * a new resource is immediately created and inserted into the
     * free queue.
     */
    const size_t min_count;
};

/**
 * This is registered with the memory pool passed to ib_resource_pool_create.
 *
 * @param[out] data The memory pool created.
 */
static void ib_resource_pool_destroy(void *data) {
    assert(data != NULL);

    ib_status_t rc;
    ib_resource_pool_t *rp = (ib_resource_pool_t *)data;

    while (ib_queue_size(rp->resources) > 0) {
        void *v;
        rc = ib_queue_pop_front(rp->resources, &v);
        if (rc != IB_OK) {
            return;
        }

        ib_resource_t *r = (ib_resource_t *)v;
        (rp->destroy_fn)(r->resource, rp->destroy_data);
    }
}

/**
 * Create a new resource, always.
 *
 * Both ib_resource_pool_create() and ib_resource_acquire()
 * may create new @ref ib_resource_t. This function isolates that
 * common code.
 *
 * @returns
 * - IB_OK On success.
 * - IB_DECLINED If the max limit is reached.
 * - IB_EALLOC If an allocation error occurs.
 * - Other from the user create function.
 */
static ib_status_t create_resource(
    ib_resource_pool_t *resource_pool,
    ib_resource_t **resource
)
{
    assert(resource_pool != NULL);
    assert(resource != NULL);

    ib_resource_t *tmp_resource = NULL;
    ib_status_t rc;

    void *user_resource = NULL;

    /* It is most likely that resource creation will fail.
    * Do this first to detect most likely errors fast. */
    rc = (resource_pool->create_fn)(
            &user_resource,
            resource_pool->create_data);
    if (rc != IB_OK) {
        return rc;
    }

    /* Attempt to get an already allocated resource struct. */
    if (ib_queue_size(resource_pool->free_queue) > 0) {
        rc = ib_queue_pop_front(
            resource_pool->free_queue,
            &tmp_resource);
        if (rc != IB_OK) {
            return rc;
        }
    }
    /* Otherwise, allocate a new one. */
    else {
        tmp_resource = ib_mm_alloc(resource_pool->mm, sizeof(*tmp_resource));
        if (tmp_resource == NULL) {
            return IB_EALLOC;
        }
    }

    tmp_resource->use = 0;
    tmp_resource->owner = resource_pool;
    tmp_resource->resource = user_resource;

    ++(resource_pool->count);

    *resource = tmp_resource;

    return IB_OK;
}

/**
 * Ensure that @a resource_pool has the minimum number of resources.
 *
 * @param[in] resource_pool The resource pool.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 * - Other on user function failures.
 */
static ib_status_t fill_to_min(
    ib_resource_pool_t *resource_pool
)
{
    assert(resource_pool != NULL);

    /* Pre-create the minimum number of items. */
    while (resource_pool->min_count > resource_pool->count) {
        ib_resource_t *r;
        ib_status_t rc;
        rc = create_resource(resource_pool, &r);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_queue_push_back(resource_pool->resources, r);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

ib_status_t ib_resource_pool_create(
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
{
    assert(resource_pool != NULL);
    assert(create_fn != NULL);
    assert(destroy_fn != NULL);

    ib_resource_pool_t *rp;
    ib_status_t         rc;

    if (min_count > 0 && max_count > 0 && min_count > max_count) {
        return IB_EINVAL;
    }

    rp = ib_mm_calloc(mm, sizeof(*rp), 1);
    if (rp == NULL) {
        return IB_EALLOC;
    }

    rc = ib_queue_create(&(rp->resources), mm, IB_QUEUE_NONE);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_queue_create(&(rp->free_queue), mm, IB_QUEUE_NONE);
    if (rc != IB_OK) {
        return rc;
    }

    rp->mm = mm;

    /* Assign callbacks. */
    rp->create_fn    = create_fn;
    rp->create_data  = create_data;
    rp->destroy_fn   = destroy_fn;
    rp->destroy_data = destroy_data;
    rp->preuse_fn    = preuse_fn;
    rp->preuse_data  = preuse_data;
    rp->postuse_fn   = postuse_fn;
    rp->postuse_data = postuse_data;

    /* Assign limits, initialize counters. */
    rp->count     = 0;

    /* Arcane casting to get around the C constness of {max,min}_count. */
    *(size_t *)&(rp->max_count) = max_count;
    *(size_t *)&(rp->min_count) = min_count;

    rc = ib_mm_register_cleanup(mm, ib_resource_pool_destroy, rp);
    if (rc != IB_OK) {
        return rc;
    }

    rc = fill_to_min(rp);
    if (rc != IB_OK) {
        return rc;
    }

    /* Return to user and report OK. */
    *resource_pool = rp;

    return IB_OK;
}

ib_status_t ib_resource_acquire(
    ib_resource_pool_t *resource_pool,
    ib_resource_t **resource
)
{
    assert(resource_pool != NULL);
    assert(resource != NULL);

    ib_resource_t *tmp_resource = NULL;
    ib_status_t rc;

    /* If there is a free resource, acquire it. */
    if (ib_queue_size(resource_pool->resources) > 0) {
        rc = ib_queue_pop_front(
            resource_pool->resources,
            &tmp_resource);
        if (rc != IB_OK) {
            goto failure;
        }

        /* Ensure that we have a resource. */
        if (tmp_resource == NULL) {
            ib_util_log_error("Resource queue returned NULL.");
            rc = IB_DECLINED;
            goto failure;
        }

        goto success;
    }
    /* If we may create a new resource, do so. */
    else if (   (resource_pool->max_count == 0)
             || (resource_pool->max_count > resource_pool->count) )
    {
        rc = create_resource(resource_pool, &tmp_resource);
        if (rc != IB_OK) {
            goto failure;
        }

        /* Ensure that we have a resource. */
        if (tmp_resource == NULL) {
            ib_util_log_error("Creation function created NULL resource.");
            rc = IB_DECLINED;
            goto failure;
        }

        goto success;
    }
    /* If we may not wait for the resource, fail w/ IB_DECLINED. */
    else {
        rc = IB_DECLINED;
        goto failure;
    }

success:

    if (resource_pool->preuse_fn != NULL) {
        (resource_pool->preuse_fn)(
            tmp_resource->resource,
            resource_pool->preuse_data);
    }

    ++(tmp_resource->use);

    *resource = tmp_resource;
    return rc;

failure:
    return rc;
}

/**
 * Destroy the resource @a resource.
 * @param[in] resource Destroy this resource.
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC If the empty ib_resource_t cannot be put in the free queue.
 */
static ib_status_t destroy_resource(
    ib_resource_t *resource
)
{
    assert(resource != NULL);
    assert(resource->owner != NULL);

    ib_status_t rc;

    (resource->owner->destroy_fn)(
        resource->resource,
        resource->owner->destroy_data);
    resource->use = 0;
    resource->resource = NULL;

    --(resource->owner->count);

    /* Store the empty resource struct on the free queue for reused. */
    rc = ib_queue_push_back(resource->owner->free_queue, resource);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_resource_release(
    ib_resource_t *resource
)
{
    assert(resource != NULL);
    assert(resource->owner != NULL);

    ib_status_t rc;

    /* If a postuse function is defined, handle it. */
    if (resource->owner->postuse_fn != NULL) {
        rc = (resource->owner->postuse_fn)(
            resource->resource,
            resource->owner->postuse_data);

        /* If the user says that the resource is invalid, destroy it. */
        if (rc == IB_EINVAL) {
            return destroy_resource(resource);
        }
    }

    rc = ib_queue_push_back(resource->owner->resources, resource);

    return rc;
}

void *ib_resource_get(const ib_resource_t* resource)
{
    assert(resource != NULL);
    return resource->resource;
}

size_t ib_resource_use_get(const ib_resource_t* resource)
{
    assert(resource != NULL);
    return resource->use;
}

ib_status_t ib_resource_pool_flush(
    ib_resource_pool_t *resource_pool
)
{
    assert(resource_pool != NULL);

    ib_status_t rc;

    /* Destroy all the resources. */
    while (resource_pool->count > 0) {
        ib_resource_t *r;

        rc = ib_queue_pop_front(resource_pool->resources, &r);
        if (rc != IB_OK) {
            return rc;
        }

        destroy_resource(r);
    }

    /* Fill to the minimum. */
    rc = fill_to_min(resource_pool);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/** @} */
