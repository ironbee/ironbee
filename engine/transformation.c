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
 * @brief IronBee --- Transformations
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/transformation.h>

#include "engine_private.h"

#include <ironbee/bytestr.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>

#include <assert.h>
#include <string.h>

/**
 * Transformation.
 */
struct ib_tfn_t {
    const char  *name;        /**< Name */
    bool         handle_list; /**< Handle list */
    ib_tfn_fn_t  fn_execute;  /**< Function */
    void        *cbdata;      /**< Callback data. */
};

/* -- Transformation Routines -- */

ib_status_t ib_tfn_register(
    ib_engine_t *ib,
    const char  *name,
    bool         handle_list,
    ib_tfn_fn_t  fn_execute,
    void        *cbdata
)
{
    assert(ib         != NULL);
    assert(name       != NULL);
    assert(fn_execute != NULL);

    ib_status_t rc;
    ib_tfn_t *tfn;
    char *name_copy;

    name_copy = ib_mpool_strdup(ib->mp, name);
    if (name_copy == NULL) {
        return IB_EALLOC;
    }

    tfn = (ib_tfn_t *)ib_mpool_alloc(ib->mp, sizeof(*tfn));
    if (tfn == NULL) {
        return IB_EALLOC;
    }
    tfn->name        = name_copy;
    tfn->fn_execute  = fn_execute;
    tfn->handle_list = handle_list;
    tfn->cbdata      = cbdata;

    rc = ib_hash_get(ib->tfns, NULL, name_copy);
    if (rc != IB_ENOENT) {
        /* Already exists. */
        return IB_EINVAL;
    }

    rc = ib_hash_set(ib->tfns, name_copy, tfn);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

const char DLL_PUBLIC *ib_tfn_name(const ib_tfn_t *tfn)
{
    return tfn->name;
}

bool DLL_PUBLIC ib_tfn_handle_list(const ib_tfn_t *tfn)
{
    return tfn->handle_list;
}

ib_status_t ib_tfn_lookup_ex(
    ib_engine_t  *ib,
    const char   *name,
    size_t        nlen,
    ib_tfn_t    **ptfn
)
{
    return ib_hash_get_ex(ib->tfns, ptfn, name, nlen);
}

ib_status_t ib_tfn_lookup(
    ib_engine_t  *ib,
    const char   *name,
    ib_tfn_t    **ptfn
)
{
    return ib_tfn_lookup_ex(ib, name, strlen(name), ptfn);
}

ib_status_t ib_tfn_transform(
    ib_engine_t       *ib,
    ib_mpool_t        *mp,
    const ib_tfn_t    *tfn,
    const ib_field_t  *fin,
    const ib_field_t **fout
)
{
    assert(tfn != NULL);
    assert(mp != NULL);
    assert(fin != NULL);
    assert(fout != NULL);

    ib_status_t rc = tfn->fn_execute(ib, mp, fin, fout, tfn->cbdata);

    return rc;
}
