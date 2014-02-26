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

ib_status_t ib_tfn_create(
    const ib_tfn_t **ptfn,
    ib_mm_t          mm,
    const char      *name,
    bool             handle_list,
    ib_tfn_fn_t      fn_execute,
    void            *cbdata
)
{
    assert(ptfn       != NULL);
    assert(name       != NULL);
    assert(fn_execute != NULL);

    ib_tfn_t *tfn;
    char *name_copy;

    name_copy = ib_mm_strdup(mm, name);
    if (name_copy == NULL) {
        return IB_EALLOC;
    }

    tfn = (ib_tfn_t *)ib_mm_alloc(mm, sizeof(*tfn));
    if (tfn == NULL) {
        return IB_EALLOC;
    }
    tfn->name        = name_copy;
    tfn->fn_execute  = fn_execute;
    tfn->handle_list = handle_list;
    tfn->cbdata      = cbdata;

    *ptfn = tfn;

    return IB_OK;
}

ib_status_t ib_tfn_register(
    ib_engine_t    *ib,
    const ib_tfn_t *tfn
)
{
    assert(ib  != NULL);
    assert(tfn != NULL);

    ib_status_t rc;

    rc = ib_hash_get(ib->tfns, NULL, ib_tfn_name(tfn));
    if (rc != IB_ENOENT) {
        /* Already exists. */
        return IB_EINVAL;
    }

    rc = ib_hash_set(ib->tfns, ib_tfn_name(tfn), (void *)tfn);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_tfn_create_and_register(
    const ib_tfn_t **ptfn,
    ib_engine_t     *ib,
    const char      *name,
    bool             handle_list,
    ib_tfn_fn_t      fn_execute,
    void            *cbdata
)
{
    assert(ib != NULL);
    assert(name != NULL);
    assert(fn_execute != NULL);

    const ib_tfn_t *tfn;
    ib_status_t rc;

    rc = ib_tfn_create(
        &tfn,
        ib_engine_mm_main_get(ib),
        name,
        handle_list,
        fn_execute, cbdata
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_tfn_register(ib, tfn);
    if (rc != IB_OK) {
        return rc;
    }

    if (ptfn != NULL) {
        *ptfn = tfn;
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
    ib_engine_t     *ib,
    const char      *name,
    size_t           nlen,
    const ib_tfn_t **ptfn
)
{
    return ib_hash_get_ex(ib->tfns, ptfn, name, nlen);
}

ib_status_t ib_tfn_lookup(
    ib_engine_t     *ib,
    const char      *name,
    const ib_tfn_t **ptfn
)
{
    return ib_tfn_lookup_ex(ib, name, strlen(name), ptfn);
}

ib_status_t ib_tfn_execute(
    ib_mm_t            mm,
    const ib_tfn_t    *tfn,
    const ib_field_t  *fin,
    const ib_field_t **fout
)
{
    assert(tfn  != NULL);
    assert(fin  != NULL);
    assert(fout != NULL);

    ib_status_t       rc;
    const ib_field_t *out = NULL;

    if (fin->type == IB_FTYPE_LIST && ! ib_tfn_handle_list(tfn)) {
        /* Unroll list */
        const ib_list_t *value_list;
        const ib_list_node_t *node;
        ib_list_t *out_list;
        ib_field_t *fnew;

        rc = ib_field_value(fin, ib_ftype_list_out(&value_list));
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_list_create(&out_list, mm);
        if (rc != IB_OK) {
            return rc;
        }

        IB_LIST_LOOP_CONST(value_list, node) {
            const ib_field_t *in;
            const ib_field_t *tfn_out;

            in = (const ib_field_t *)ib_list_node_data_const(node);
            assert(in != NULL);

            rc = ib_tfn_execute(mm, tfn, in, &tfn_out);
            if (rc != IB_OK) {
                return rc;
            }
            if (tfn_out == NULL) {
                return IB_EINVAL;
            }

            rc = ib_list_push(out_list, (void *)tfn_out);
            if (rc != IB_OK) {
                return rc;
            }
        }

        /* Finally, create the output field (list) and return it */
        rc = ib_field_create(&fnew, mm,
                             fin->name, fin->nlen,
                             IB_FTYPE_LIST, ib_ftype_list_in(out_list));
        if (rc != IB_OK) {
            return rc;
        }
        out = fnew;
    }
    else {
        /* Don't unroll */
        rc = tfn->fn_execute(mm, fin, &out, tfn->cbdata);
        if (rc != IB_OK) {
            return rc;
        }

        if (out == NULL) {
            return IB_EINVAL;
        }
    }

    assert(rc == IB_OK);
    *fout = out;

    return IB_OK;
}
