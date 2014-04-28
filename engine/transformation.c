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
 * @brief IronBee --- Transformation interface
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/transformation.h>

#include "engine_private.h"

#include <assert.h>

struct ib_transformation_t {
    /*! Name of the transformation. */
    const char *name;

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

    /*! Instance creation function. */
    ib_transformation_create_fn_t create_fn;

    /*! Create callback data. */
    void *create_cbdata;

    /*! Instance destroy function. */
    ib_transformation_destroy_fn_t destroy_fn;

    /*! Destroy callback data. */
    void *destroy_cbdata;

    /*! Instance execution function. */
    ib_transformation_execute_fn_t execute_fn;

    /*! Execute callback data. */
    void *execute_cbdata;
};

struct ib_transformation_inst_t
{
    /*! Transformation. */
    const ib_transformation_t *tfn;

    /*! Parameters. */
    const char *parameters;

    /*! Instance data. */
    void *instance_data;
};

ib_status_t ib_transformation_create(
    ib_transformation_t            **tfn,
    ib_mm_t                          mm,
    const char                      *name,
    bool                             handle_list,
    ib_transformation_create_fn_t    create_fn,
    void                            *create_cbdata,
    ib_transformation_destroy_fn_t   destroy_fn,
    void                            *destroy_cbdata,
    ib_transformation_execute_fn_t   execute_fn,
    void                            *execute_cbdata
)
{
    assert(tfn        != NULL);
    assert(name       != NULL);
    assert(execute_fn != NULL);

    ib_transformation_t *local_tfn;

    local_tfn = (ib_transformation_t *)ib_mm_alloc(mm, sizeof(*local_tfn));
    if (local_tfn == NULL) {
        return IB_EALLOC;
    }
    local_tfn->name = ib_mm_strdup(mm, name);
    if (local_tfn->name == NULL) {
        return IB_EALLOC;
    }
    local_tfn->handle_list    = handle_list;
    local_tfn->create_fn      = create_fn;
    local_tfn->create_cbdata  = create_cbdata;
    local_tfn->destroy_fn     = destroy_fn;
    local_tfn->destroy_cbdata = destroy_cbdata;
    local_tfn->execute_fn     = execute_fn;
    local_tfn->execute_cbdata = execute_cbdata;

    *tfn = local_tfn;

    return IB_OK;
}

ib_status_t ib_transformation_register(
    ib_engine_t               *ib,
    const ib_transformation_t *tfn
)
{
    assert(ib  != NULL);
    assert(tfn != NULL);

    ib_status_t rc;

    rc = ib_hash_get(ib->tfns, NULL, ib_transformation_name(tfn));
    if (rc != IB_ENOENT) {
        /* Already exists. */
        return IB_EINVAL;
    }

    rc = ib_hash_set(ib->tfns, ib_transformation_name(tfn), (void *)tfn);

    return rc;
}

ib_status_t ib_transformation_create_and_register(
    const ib_transformation_t      **tfn,
    ib_engine_t                     *ib,
    const char                      *name,
    bool                             handle_list,
    ib_transformation_create_fn_t    create_fn,
    void                            *create_cbdata,
    ib_transformation_destroy_fn_t   destroy_fn,
    void                            *destroy_cbdata,
    ib_transformation_execute_fn_t   execute_fn,
    void                            *execute_cbdata
)
{
    assert(ib != NULL);
    assert(name != NULL);
    assert(execute_fn != NULL);

    ib_transformation_t *local_tfn;
    ib_status_t rc;

    rc = ib_transformation_create(
        &local_tfn,
        ib_engine_mm_main_get(ib),
        name,
        handle_list,
        create_fn, create_cbdata,
        destroy_fn, destroy_cbdata,
        execute_fn, execute_cbdata
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_transformation_register(ib, local_tfn);
    if (rc != IB_OK) {
        return rc;
    }

    if (tfn != NULL) {
        *tfn = local_tfn;
    }

    return IB_OK;
}

ib_status_t ib_transformation_lookup(
    ib_engine_t                *ib,
    const char                 *name,
    size_t                      name_length,
    const ib_transformation_t **tfn
)
{
    assert(ib != NULL);
    assert(name != NULL);

    return ib_hash_get_ex(ib->tfns, tfn, name, name_length);
}

const char *ib_transformation_name(
    const ib_transformation_t *tfn
)
{
    assert(tfn != NULL);

    return tfn->name;
}

bool ib_transformation_handle_list(
    const ib_transformation_t *tfn
)
{
    assert(tfn != NULL);

    return tfn->handle_list;
}

/*! Cleanup function to destroy transformation. */
static
void cleanup_tfn(
    void *cbdata
)
{
    const ib_transformation_inst_t *tfn_inst =
        (const ib_transformation_inst_t *)cbdata;
    assert(tfn_inst != NULL);
    const ib_transformation_t *tfn =
        ib_transformation_inst_transformation(tfn_inst);
    assert(tfn != NULL);

    /* Will only be called if there is a destroy function. */
    assert(tfn->destroy_fn);
    tfn->destroy_fn(
        tfn_inst->instance_data,
        tfn->destroy_cbdata
    );
}

ib_status_t ib_transformation_inst_create(
    ib_transformation_inst_t  **tfn_inst,
    ib_mm_t                     mm,
    const ib_transformation_t  *tfn,
    const char                 *parameters
)
{
    assert(tfn_inst != NULL);
    assert(tfn != NULL);

    ib_transformation_inst_t *local_tfn_inst;
    ib_status_t rc;

    local_tfn_inst =
        (ib_transformation_inst_t *)ib_mm_alloc(mm, sizeof(*local_tfn_inst));
    if (local_tfn_inst == NULL) {
        return IB_EALLOC;
    }

    if (parameters != NULL) {
        local_tfn_inst->parameters = ib_mm_strdup(mm, parameters);
        if (local_tfn_inst->parameters == NULL) {
            return IB_EALLOC;
        }
    }
    else {
        local_tfn_inst->parameters = NULL;
    }
    local_tfn_inst->tfn = tfn;

    if (tfn->create_fn == NULL) {
        local_tfn_inst->instance_data = NULL;
    }
    else {
        rc = tfn->create_fn(
            mm,
            local_tfn_inst->parameters,
            &(local_tfn_inst->instance_data),
            tfn->create_cbdata
        );
        if (rc != IB_OK) {
            return rc;
        }
    }

    if (tfn->destroy_fn != NULL) {
        /* Register the destroy function. */
        rc = ib_mm_register_cleanup(mm, cleanup_tfn, local_tfn_inst);
        if (rc != IB_OK) {
            return rc;
        }
    }

    *tfn_inst = local_tfn_inst;

    return IB_OK;
}

const ib_transformation_t *ib_transformation_inst_transformation(
    const ib_transformation_inst_t *tfn_inst
)
{
    assert(tfn_inst != NULL);

    return tfn_inst->tfn;
}

const char *ib_transformation_inst_parameters(
    const ib_transformation_inst_t *tfn_inst
)
{
    assert(tfn_inst != NULL);

    return tfn_inst->parameters;
}

void *ib_transformation_inst_data(
    const ib_transformation_inst_t *tfn_inst
)
{
    assert(tfn_inst != NULL);

    return tfn_inst->instance_data;
}

ib_status_t ib_transformation_inst_execute(
    const ib_transformation_inst_t  *tfn_inst,
    ib_mm_t                          mm,
    const ib_field_t                *fin,
    const ib_field_t               **fout
)
{
    assert(tfn_inst  != NULL);
    assert(fin       != NULL);
    assert(fout      != NULL);

    ib_status_t       rc;
    const ib_field_t *out = NULL;

    const ib_transformation_t *tfn =
        ib_transformation_inst_transformation(tfn_inst);

    assert(tfn != NULL);

    if (
        fin->type == IB_FTYPE_LIST && !
        ib_transformation_handle_list(tfn)
    ) {
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

            rc = ib_transformation_inst_execute(tfn_inst, mm, in, &tfn_out);
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
        rc = tfn->execute_fn(
            mm,
            fin,
            &out,
            ib_transformation_inst_data(tfn_inst),
            tfn->execute_cbdata
        );
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
