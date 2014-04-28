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
 * @brief IronBee --- Operator interface
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/operator.h>

#include "engine_private.h"

#include <assert.h>

struct ib_operator_t {
    /*! Name of the operator. */
    char *name;

    /*! Operator capabilities */
    ib_flags_t capabilities;

    /*! Instance creation function. */
    ib_operator_create_fn_t create_fn;

    /*! Create callback data. */
    void *create_cbdata;

    /*! Instance destroy function. */
    ib_operator_destroy_fn_t destroy_fn;

    /*! Destroy callback data. */
    void *destroy_cbdata;

    /*! Instance execution function. */
    ib_operator_execute_fn_t execute_fn;

    /*! Execute callback data. */
    void *execute_cbdata;
};

struct ib_operator_inst_t
{
    /*! Operator. */
    const ib_operator_t *op;

    /*! Parameters. */
    const char *parameters;

    /*! Instance data. */
    void *instance_data;
};

ib_status_t ib_operator_create(
    ib_operator_t            **op,
    ib_mm_t                    mm,
    const char                *name,
    ib_flags_t                 capabilities,
    ib_operator_create_fn_t    create_fn,
    void                      *create_cbdata,
    ib_operator_destroy_fn_t   destroy_fn,
    void                      *destroy_cbdata,
    ib_operator_execute_fn_t   execute_fn,
    void                      *execute_cbdata
)
{
    assert(op != NULL);

    ib_operator_t *local_op;
    local_op = (ib_operator_t *)ib_mm_alloc(mm, sizeof(*local_op));
    if (local_op == NULL) {
        return IB_EALLOC;
    }

    local_op->name = ib_mm_strdup(mm, name);
    if (local_op->name == NULL) {
        return IB_EALLOC;
    }
    local_op->capabilities   = capabilities;
    local_op->create_fn      = create_fn;
    local_op->create_cbdata  = create_cbdata;
    local_op->destroy_fn     = destroy_fn;
    local_op->destroy_cbdata = destroy_cbdata;
    local_op->execute_fn     = execute_fn;
    local_op->execute_cbdata = execute_cbdata;

    *op = local_op;

    return IB_OK;
}

ib_status_t ib_operator_register(
    ib_engine_t         *ib,
    const ib_operator_t *op
)
{
    assert(ib != NULL);
    assert(op != NULL);

    ib_status_t rc;

    rc = ib_hash_get(ib->operators, NULL, op->name);
    if (rc == IB_OK) {
        /* Already exists. */
        return IB_EINVAL;
    }

    rc = ib_hash_set(ib->operators, ib_operator_name(op), (void *)op);

    return rc;
}

ib_status_t ib_operator_stream_register(
    ib_engine_t         *ib,
    const ib_operator_t *op
)
{
    assert(ib != NULL);
    assert(op != NULL);

    ib_status_t rc;

    rc = ib_hash_get(ib->stream_operators, NULL, op->name);
    if (rc == IB_OK) {
        /* Already exists. */
        return IB_EINVAL;
    }

    rc = ib_hash_set(ib->stream_operators, ib_operator_name(op), (void *)op);

    return rc;
}

ib_status_t ib_operator_create_and_register(
    ib_operator_t            **op,
    ib_engine_t               *ib,
    const char                *name,
    ib_flags_t                 capabilities,
    ib_operator_create_fn_t    create_fn,
    void                      *create_cbdata,
    ib_operator_destroy_fn_t   destroy_fn,
    void                      *destroy_cbdata,
    ib_operator_execute_fn_t   execute_fn,
    void                      *execute_cbdata
)
{
    assert(ib != NULL);
    assert(name != NULL);

    ib_operator_t *local_op;
    ib_status_t rc;

    rc = ib_operator_create(
        &local_op,
        ib_engine_mm_main_get(ib),
        name,
        capabilities,
        create_fn, create_cbdata,
        destroy_fn, destroy_cbdata,
        execute_fn, execute_cbdata
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_operator_register(ib, local_op);
    if (rc != IB_OK) {
        return rc;
    }

    if (op != NULL) {
        *op = local_op;
    }

    return IB_OK;
}

ib_status_t ib_operator_stream_create_and_register(
    ib_operator_t            **op,
    ib_engine_t               *ib,
    const char                *name,
    ib_flags_t                 capabilities,
    ib_operator_create_fn_t    create_fn,
    void                      *create_cbdata,
    ib_operator_destroy_fn_t   destroy_fn,
    void                      *destroy_cbdata,
    ib_operator_execute_fn_t   execute_fn,
    void                      *execute_cbdata
)
{
    assert(ib != NULL);
    assert(name != NULL);

    ib_operator_t *local_op;
    ib_status_t rc;

    rc = ib_operator_create(
        &local_op,
        ib_engine_mm_main_get(ib),
        name,
        capabilities,
        create_fn, create_cbdata,
        destroy_fn, destroy_cbdata,
        execute_fn, execute_cbdata
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_operator_stream_register(ib, local_op);
    if (rc != IB_OK) {
        return rc;
    }

    if (op != NULL) {
        *op = local_op;
    }

    return IB_OK;
}

ib_status_t ib_operator_lookup(
    ib_engine_t          *ib,
    const char           *name,
    size_t                name_length,
    const ib_operator_t **op
)
{
    assert(ib != NULL);
    assert(name != NULL);

    return ib_hash_get_ex(ib->operators, op, name, name_length);
}

ib_status_t ib_operator_stream_lookup(
    ib_engine_t          *ib,
    const char           *name,
    size_t                name_length,
    const ib_operator_t **op
)
{
    assert(ib != NULL);
    assert(name != NULL);

    return ib_hash_get_ex(ib->stream_operators, op, name, name_length);
}

const char *ib_operator_name(
    const ib_operator_t *op
)
{
    assert(op != NULL);

    return op->name;
}

ib_flags_t ib_operator_capabilities(
    const ib_operator_t *op
)
{
    assert(op != NULL);

    return op->capabilities;
}

/*! Cleanup function to destroy operator. */
static
void cleanup_op(
    void *cbdata
)
{
    const ib_operator_inst_t *op_inst =
        (const ib_operator_inst_t *)cbdata;
    assert(op_inst != NULL);
    const ib_operator_t *op =
        ib_operator_inst_operator(op_inst);
    assert(op != NULL);

    /* Will only be called if there is a destroy function. */
    assert(op->destroy_fn);
    op->destroy_fn(
        op_inst->instance_data,
        op->destroy_cbdata
    );
}


ib_status_t ib_operator_inst_create(
    ib_operator_inst_t  **op_inst,
    ib_mm_t               mm,
    ib_context_t         *ctx,
    const ib_operator_t  *op,
    ib_flags_t            required_capabilities,
    const char           *parameters
)
{
    assert(op_inst != NULL);
    assert(ctx != NULL);
    assert(op != NULL);

    /* Verify that this operator is valid for this rule type */
    if (
        (op->capabilities & required_capabilities) !=
        required_capabilities
    ) {
        return IB_EINVAL;
    }

    ib_operator_inst_t *local_op_inst;
    ib_status_t rc;

    local_op_inst =
        (ib_operator_inst_t *)ib_mm_alloc(mm, sizeof(*local_op_inst));
    if (local_op_inst == NULL) {
        return IB_EALLOC;
    }

    if (parameters != NULL) {
        local_op_inst->parameters = ib_mm_strdup(mm, parameters);
        if (local_op_inst->parameters == NULL) {
            return IB_EALLOC;
        }
    }
    else {
        local_op_inst->parameters = NULL;
    }
    local_op_inst->op = op;

    if (op->create_fn == NULL) {
        local_op_inst->instance_data = NULL;
    }
    else {
        rc = op->create_fn(
            ctx,
            mm,
            local_op_inst->parameters,
            &(local_op_inst->instance_data),
            op->create_cbdata
        );
        if (rc != IB_OK) {
            return rc;
        }
    }

    if (op->destroy_fn != NULL) {
        /* Register the destroy function. */
        rc = ib_mm_register_cleanup(mm, cleanup_op, local_op_inst);
        if (rc != IB_OK) {
            return rc;
        }
    }

    *op_inst = local_op_inst;

    return IB_OK;
}


const ib_operator_t *ib_operator_inst_operator(
    const ib_operator_inst_t *op_inst
)
{
    assert(op_inst != NULL);

    return op_inst->op;
}

const char *ib_operator_inst_parameters(
    const ib_operator_inst_t *op_inst
)
{
    assert(op_inst != NULL);

    return op_inst->parameters;
}

void *ib_operator_inst_data(
    const ib_operator_inst_t *op_inst
)
{
    assert(op_inst != NULL);

    return op_inst->instance_data;
}

ib_status_t ib_operator_inst_execute(
    const ib_operator_inst_t *op_inst,
    ib_tx_t                  *tx,
    const ib_field_t         *input,
    ib_field_t               *capture,
    ib_num_t                 *result
)
{
    assert(op_inst != NULL);
    assert(result != NULL);

    const ib_operator_t *op = ib_operator_inst_operator(op_inst);

    assert(op != NULL);

    if (op->execute_fn == NULL) {
        *result = 1;
        return IB_OK;
    }
    else {
        return op->execute_fn(
            tx,
            input,
            capture,
            result,
            ib_operator_inst_data(op_inst),
            op->execute_cbdata
        );
    }
}
