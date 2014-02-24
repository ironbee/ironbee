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
 * @brief IronBee --- JSON YAJL wrapper functions
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "json_yajl_private.h"

#include <ironbee/types.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif
#include <yajl/yajl_tree.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <assert.h>
#include <string.h>
#include <inttypes.h>

typedef struct {
    size_t size;
    char   data[0];
} chunk_t;

void *json_yajl_alloc(
    void    *ctx,
    size_t   size)
{
    json_yajl_alloc_context_t *alloc_ctx = (json_yajl_alloc_context_t *)ctx;
    chunk_t *chunk;
    size_t alloc = size + sizeof(*chunk);

    chunk = (chunk_t *)ib_mm_alloc(alloc_ctx->mm, size + alloc);
    if (chunk == NULL) {
        alloc_ctx->status = IB_EALLOC;
        return NULL;
    }

    chunk->size = size;
    return chunk->data;
}

void *json_yajl_realloc(
    void   *ctx,
    void   *ptr,
    size_t  size)
{
    if (ptr == NULL) {
        return json_yajl_alloc(ctx, size);
    }

    json_yajl_alloc_context_t *alloc_ctx = (json_yajl_alloc_context_t *)ctx;
    chunk_t *chunk;
    chunk_t *new;
    size_t bytes;

    chunk = (chunk_t *)((char *)ptr - sizeof(chunk_t));
    if (chunk->size >= size) {
        return chunk->data;
    }

    bytes = size + sizeof(*chunk);
    new = (chunk_t *)ib_mm_alloc(alloc_ctx->mm, bytes);
    if (new == NULL) {
        alloc_ctx->status = IB_EALLOC;
        return NULL;
    }

    memcpy(new->data, ptr, chunk->size);
    new->size = size;
    return new->data;
}

void json_yajl_free(
    void *ctx,
    void *ptr)
{
    return;
}
