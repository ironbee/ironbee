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

#include <assert.h>
#include <unistd.h>

/**
 * This is registered with the memory pool passed to ib_resource_pool_create.
 *
 * @param[out] data The memory pool created.
 */
static void ib_resource_pool_destroy(void *data) {
    assert(data != NULL);

    ib_status_t rc;
    ib_resource_pool_t *rp = (ib_resource_pool_t*)data;

    while (ib_queue_size(rp->resources) > 0) {
        void *v;
        rc = ib_queue_pop_front(rp->resources, &v);
        if (rc != IB_OK) {
            return;
        }

        ib_resource_t *r = (ib_resource_t*)v;
        (rp->destroy_fn)(r->resource, rp->destroy_data);
    }
}

ib_status_t DLL_PUBLIC ib_resource_pool_create(
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
)
{
    assert(resource_pool != NULL);
    assert(mp != NULL);
    assert(create_fn != NULL);
    assert(destroy_fn != NULL);

    ib_resource_pool_t *rp;
    ib_status_t         rc;

    if (min_count > 0 && max_count > 0 && min_count > max_count) {
        return IB_EINVAL;
    }

    rp = ib_mpool_calloc(mp, sizeof(*rp), 1);
    if (rp == NULL) {
        return IB_EALLOC;
    }

    rc = ib_queue_create(&(rp->resources), mp, IB_QUEUE_NONE);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_queue_create(&(rp->free_list), mp, IB_QUEUE_NONE);
    if (rc != IB_OK) {
        return rc;
    }

    rp->mp = mp;

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
    rp->max_count = max_count;
    rp->min_count = min_count;
    rp->max_use   = max_use;

    rc = ib_mpool_cleanup_register(mp, ib_resource_pool_destroy, rp);
    if (rc != IB_OK) {
        return rc;
    }

    /* Return to user and report OK. */
    *resource_pool = rp;

    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_resource_get(
    ib_resource_pool_t *resource_pool,
    bool block,
    ib_resource_t **resource
)
{
    assert(resource_pool != NULL);
    assert(resource != NULL);

    ib_resource_t *tmp_resource = NULL;
    ib_status_t rc;

    for (;;) {
        /* If there is a free resource, aquire it. */
        if (ib_queue_size(resource_pool->resources) > 0) {
            rc = ib_queue_pop_front(
                resource_pool->resources,
                (void**)&tmp_resource);
            if (rc != IB_OK) {
                goto failure;
            }

            goto success;
        }
        /* If we may create a new resource, do so. */
        else if (  (resource_pool->max_count == 0) 
                || (resource_pool->max_count > resource_pool->count) )
        {
            void *user_resource = NULL;

            /* It is most likely that resource creation will fail.
             * Do this first to detect most likely errors fast. */
            rc = (resource_pool->create_fn)(
                    &user_resource,
                    resource_pool->create_data);
            if (rc != IB_OK) {
                goto failure;
            }

            /* Attempt to get an already allocated resource struct. */
            if (ib_queue_size(resource_pool->free_list) > 0) {
                rc = ib_queue_pop_front(
                    resource_pool->free_list,
                    (void **)(&tmp_resource));
                if (rc != IB_OK) {
                    goto failure;
                }
            }
            /* Otherwise, allocate a new one. */
            else {
                tmp_resource = ib_mpool_alloc(
                    resource_pool->mp,
                    sizeof(*tmp_resource));
                if (tmp_resource == NULL) {
                    rc = IB_EALLOC;
                    goto failure;
                }
            }

            tmp_resource->use = 0;
            tmp_resource->owner = resource_pool;
            tmp_resource->resource = user_resource;

            ++(resource_pool->count);

            goto success;
        }
        /* If we may not wait for the resource, fail w/ IB_DECLINED. */
        else if (!block) {
            rc = IB_DECLINED;
            goto failure;
        }

        sleep(1);
    }

success:

    /* At this point we have a resource. */
    assert(tmp_resource != NULL);

    if (resource_pool->preuse_fn != NULL) {
        (resource_pool->preuse_fn)(
            tmp_resource->resource,
            resource_pool->preuse_data);
    }

    ++(tmp_resource->use);

    *resource = tmp_resource;
    return IB_OK;

failure:
    return rc;
}

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

    /* Store the empty resource struct on the free list for reused. */
    rc = ib_queue_push_back(resource->owner->free_list, resource);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_resource_return(
    ib_resource_t *resource
)
{
    assert(resource != NULL);
    assert(resource->owner != NULL);

    if (resource->owner->postuse_fn != NULL) {
        (resource->owner->postuse_fn)(
            resource->resource,
            resource->owner->postuse_data);
    }

    if (resource->use >= resource->owner->max_use) {
        ib_status_t rc;

        rc = destroy_resource(resource);
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        ib_queue_push_back(resource->owner->resources, resource);
    }


    return IB_OK;
}

ib_status_t ib_resource_use(
    ib_resource_pool_t   *resource_pool,
    bool                  block,
    ib_resource_use_fn_t  fn,
    ib_status_t          *user_rc,
    void                 *cbdata
)
{
    ib_status_t    rc;
    ib_resource_t *resource = NULL;

    rc = ib_resource_get(resource_pool, block, &resource);
    if (rc != IB_OK) {
        return rc;
    }

    *user_rc = IB_OK;

    rc = fn(resource, user_rc, cbdata);
    if (rc != IB_OK) {
        destroy_resource(resource);
        return rc;
    }

    rc = ib_resource_return(resource);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}


/** @} */
