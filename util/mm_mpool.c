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
 * @brief IronBee --- Memory Manager: Mpool Implementation
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @nosubgrouping
 */

#include "ironbee_config_auto.h"

#include <ironbee/mm_mpool.h>

#include <assert.h>

/** See @ref ib_mm_alloc_fn_t.  Do not call directly. */
static void *ib_mm_mpool_alloc(
    size_t  size,
    void   *cbdata
)
{
    return ib_mpool_alloc((ib_mpool_t*)cbdata, size);
}

/** See @ref ib_mm_register_cleanup_fn_t.  Do not call directly. */
static ib_status_t ib_mm_mpool_register_cleanup(
    ib_mm_cleanup_fn_t  fn,
    void               *fndata,
    void               *cbdata
)
{
    return ib_mpool_cleanup_register((ib_mpool_t*)cbdata, fn, fndata);
}

ib_mm_t ib_mm_mpool(ib_mpool_t *mp)
{
    assert(mp != NULL);

    ib_mm_t mm = {
        &ib_mm_mpool_alloc, mp,
        &ib_mm_mpool_register_cleanup, mp
    };
    return mm;
}
