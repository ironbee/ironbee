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
#include <ironbee/mm.h>

#include <assert.h>
#include <string.h>

/**
 * Transformation.
 */
struct ib_tfn_t {
    const char          *name;           /**< Name. */
    /**
     * Should the rule engine give this transformation entire lists?
     *
     * When the rule engine calls a transformation on a field,
     * if that field is of type IB_FTYPE_LIST and this value is false,
     * then the rule engine must call this transformation once on each
     * element of the list.
     *
     * Otherwise, if this is true, the rule engine must give the entire
     * list field to the transformation.
     */
    bool                 handle_list;
    ib_tfn_create_fn_t   create_fn;      /**< Create function. */
    void                *create_cbdata;  /**< Create callback data. */
    ib_tfn_execute_fn_t  execute_fn;     /**< Execute function.*/
    void                *execute_cbdata; /**< Execute callback data. */
    ib_tfn_destroy_fn_t  destroy_fn;     /**< Destroy function. */
    void                *destroy_cbdata; /**< Destroy callback data. */
};

/**
 * Transformation instance.
 */
 struct ib_tfn_inst_t {
    const ib_tfn_t *tfn;      /**< Transformation this is an instance of. */
    const char     *param;    /**< Parameter used to construct the inst. */
    void           *instdata; /**< Instance data. */
 };

/* -- Transformation Routines -- */

ib_status_t ib_tfn_create(
    const ib_tfn_t      **ptfn,
    ib_mm_t               mm,
    const char           *name,
    bool                  handle_list,
    ib_tfn_create_fn_t    create_fn,
    void                 *create_cbdata,
    ib_tfn_execute_fn_t   execute_fn,
    void                 *execute_cbdata,
    ib_tfn_destroy_fn_t   destroy_fn,
    void                 *destroy_cbdata
)
{
    assert(ptfn       != NULL);
    assert(name       != NULL);
    assert(execute_fn != NULL);

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
    tfn->name           = name_copy;
    tfn->create_fn      = create_fn;
    tfn->create_cbdata  = create_cbdata;
    tfn->execute_fn     = execute_fn;
    tfn->execute_cbdata = execute_cbdata;
    tfn->destroy_fn     = destroy_fn;
    tfn->destroy_cbdata = destroy_cbdata;
    tfn->handle_list    = handle_list;

    *ptfn = tfn;

    return IB_OK;
}

static void tfn_inst_destroy_fn(void *cbdata) {

    ib_tfn_inst_t *tfn_inst = (ib_tfn_inst_t *)cbdata;

    assert(tfn_inst != NULL);
    assert(tfn_inst->tfn != NULL);

    /* If the destroy function was provided, destroy this object. */
    if (tfn_inst->tfn->destroy_fn != NULL) {
        tfn_inst->tfn->destroy_fn(
            tfn_inst->instdata,
            tfn_inst->tfn->destroy_cbdata
        );
    }
}

ib_status_t ib_tfn_inst_create(
    const ib_tfn_inst_t **ptfn_inst,
    ib_mm_t               mm,
    const ib_tfn_t       *tfn,
    const char           *param
)
{
    assert(ptfn_inst != NULL);
    assert(tfn != NULL);

    ib_tfn_inst_t *tmp_tfn_inst;
    ib_status_t    rc;

    tmp_tfn_inst = (ib_tfn_inst_t *)ib_mm_alloc(mm, sizeof(*tmp_tfn_inst));
    if (tmp_tfn_inst == NULL) {
        return IB_EALLOC;
    }

    if (tfn->create_fn == NULL) {
        tmp_tfn_inst->instdata = NULL;
    }
    else {
        tfn->create_fn(
            &(tmp_tfn_inst->instdata),
            mm,
            param,
            tfn->create_cbdata);
    }
    tmp_tfn_inst->tfn = tfn;
    tmp_tfn_inst->param = param;

    /* Register the destroy function. */
    rc = ib_mm_register_cleanup(mm, tfn_inst_destroy_fn, tmp_tfn_inst);
    if (rc != IB_OK) {
        return rc;
    }

    /* Commit back results and return OK. */
    *ptfn_inst = tmp_tfn_inst;
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
    const ib_tfn_t      **ptfn,
    ib_engine_t          *ib,
    const char           *name,
    bool                  handle_list,
    ib_tfn_create_fn_t    create_fn,
    void                 *create_cbdata,
    ib_tfn_execute_fn_t   execute_fn,
    void                 *execute_cbdata,
    ib_tfn_destroy_fn_t   destroy_fn,
    void                 *destroy_cbdata
)
{
    assert(ib != NULL);
    assert(name != NULL);
    assert(execute_fn != NULL);

    const ib_tfn_t *tfn;
    ib_status_t rc;

    rc = ib_tfn_create(
        &tfn,
        ib_engine_mm_main_get(ib),
        name,
        handle_list,
        create_fn, create_cbdata,
        execute_fn, execute_cbdata,
        destroy_fn, destroy_cbdata
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

const char *ib_tfn_name(const ib_tfn_t *tfn)
{
    assert(tfn != NULL);
    assert(tfn->name != NULL);

    return tfn->name;
}

const char *ib_tfn_inst_name(const ib_tfn_inst_t *tfn_inst)
{
    assert(tfn_inst != NULL);

    return ib_tfn_name(tfn_inst->tfn);
}

const char *ib_tfn_inst_param(const ib_tfn_inst_t *tfn_inst)
{
    assert(tfn_inst != NULL);

    return tfn_inst->param;
}

bool ib_tfn_handle_list(const ib_tfn_t *tfn)
{
    assert(tfn != NULL);

    return tfn->handle_list;
}

bool ib_tfn_inst_handle_list(const ib_tfn_inst_t *tfn_inst)
{
    assert(tfn_inst != NULL);

    return ib_tfn_handle_list(tfn_inst->tfn);
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

ib_status_t ib_tfn_inst_execute(
    const ib_tfn_inst_t  *tfn_inst,
    ib_mm_t               mm,
    const ib_field_t     *fin,
    const ib_field_t    **fout
)
{
    assert(tfn_inst  != NULL);
    assert(fin       != NULL);
    assert(fout      != NULL);

    ib_status_t       rc;
    const ib_field_t *out = NULL;

    if (fin->type == IB_FTYPE_LIST && ! ib_tfn_inst_handle_list(tfn_inst)) {
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

            rc = ib_tfn_inst_execute(tfn_inst, mm, in, &tfn_out);
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
        rc = tfn_inst->tfn->execute_fn(
            tfn_inst->instdata,
            mm,
            fin,
            &out,
            tfn_inst->tfn->execute_cbdata);
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
