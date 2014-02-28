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
 * @brief IronBee --- Memory Pool Lite Implementation
 *
 * See ib_mpool_lite_t for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/mpool_lite.h>

#include <assert.h>

/** Structure to hold cleanup function. */
struct ib_mpool_lite_cleanup_t
{
    /** Function. */
    ib_mpool_lite_cleanup_fn_t fn;
    /** Callback data. */
    void *cbdata;
    /** Pointer to next cleanup function. */
    struct ib_mpool_lite_cleanup_t *next;
};
typedef struct ib_mpool_lite_cleanup_t ib_mpool_lite_cleanup_t;

/** Structure to hold block of memory. */
struct ib_mpool_lite_block_t
{
    /** Next block. */
    struct ib_mpool_lite_block_t *next;
    /** Memory returned to user. */
    char data[];
};
typedef struct ib_mpool_lite_block_t ib_mpool_lite_block_t;

struct ib_mpool_lite_t
{
    /**
     * Pointer to first allocated block.
     *
     * Each block is a pointer to the next block followed by the memory
     * returns to the caller.
     **/
    void *first_block;

    /** First cleanup function. */
    ib_mpool_lite_cleanup_t *first_cleanup;
};

ib_status_t ib_mpool_lite_create(ib_mpool_lite_t **pool)
{
    assert(pool != NULL);

    ib_mpool_lite_t *local_pool = malloc(sizeof(*local_pool));
    if (local_pool == NULL) {
        return IB_EALLOC;
    }
    local_pool->first_block = NULL;
    local_pool->first_cleanup = NULL;

    *pool = local_pool;

    return IB_OK;
}

void ib_mpool_lite_destroy(ib_mpool_lite_t *pool)
{
    assert(pool != NULL);

    ib_mpool_lite_block_t *block;
    ib_mpool_lite_block_t *next_block;

    for (
        const ib_mpool_lite_cleanup_t *cleanup = pool->first_cleanup;
        cleanup != NULL;
        cleanup = cleanup->next
    ) {
        cleanup->fn(cleanup->cbdata);
    }

    block = pool->first_block;
    while (block != NULL) {
        next_block = block->next;
        free(block);
        block = next_block;
    }

    free(pool);
}

void *ib_mpool_lite_alloc(ib_mpool_lite_t *pool, size_t size)
{
    assert(pool != NULL);

    static const char* s_empty_mem = "0";

    ib_mpool_lite_block_t *block;

    if (size == 0) {
        return (void*)s_empty_mem;
    }

    block = malloc(sizeof(block->next) + size);
    if (block == NULL) {
        return NULL;
    }
    block->next = pool->first_block;
    pool->first_block = block;
    return block->data;
}

ib_status_t ib_mpool_lite_register_cleanup(
    ib_mpool_lite_t            *pool,
    ib_mpool_lite_cleanup_fn_t  fn,
    void                       *cbdata
)
{
    assert(pool != NULL);
    assert(fn != NULL);

    ib_mpool_lite_cleanup_t *cleanup =
        ib_mpool_lite_alloc(pool, sizeof(*cleanup));

    if (cleanup == NULL) {
        return IB_EALLOC;
    }

    cleanup->fn = fn;
    cleanup->cbdata = cbdata;
    cleanup->next = pool->first_cleanup;
    pool->first_cleanup = cleanup;

    return IB_OK;
}
