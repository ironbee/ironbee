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

#include <ironbee/mpool.h>

#include <assert.h>

struct ib_operator_t {
    /*! Name of the operator. */
    char *name;

    /*! Operator capabilities */
    ib_flags_t capabilities;

    /*! Instance creation function. */
    ib_operator_create_fn_t fn_create;

    /*! Create callback data. */
    void *cbdata_create;

    /*! Instance destroy function. */
    ib_operator_destroy_fn_t fn_destroy;

    /*! Destroy callback data. */
    void *cbdata_destroy;

    /*! Instance execution function. */
    ib_operator_execute_fn_t fn_execute;

    /*! Execute callback data. */
    void *cbdata_execute;
};

ib_status_t ib_operator_create(
    ib_operator_t            **op,
    ib_mpool_t                *mp,
    const char                *name,
    ib_flags_t                 capabilities,
    ib_operator_create_fn_t    fn_create,
    void                      *cbdata_create,
    ib_operator_destroy_fn_t   fn_destroy,
    void                      *cbdata_destroy,
    ib_operator_execute_fn_t   fn_execute,
    void                      *cbdata_execute
)
{
    assert(op != NULL);
    assert(mp != NULL);

    *op = (ib_operator_t *)ib_mpool_alloc(mp, sizeof(**op));
    if (*op == NULL) {
        return IB_EALLOC;
    }

    (*op)->name           = ib_mpool_strdup(mp, name);
    if ((*op)->name == NULL) {
        return IB_EALLOC;
    }
    (*op)->capabilities   = capabilities;
    (*op)->fn_create      = fn_create;
    (*op)->cbdata_create  = cbdata_create;
    (*op)->fn_destroy     = fn_destroy;
    (*op)->cbdata_destroy = cbdata_destroy;
    (*op)->fn_execute     = fn_execute;
    (*op)->cbdata_execute = cbdata_execute;

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
    ib_operator_t *existing_op;

    rc = ib_hash_get(ib->operators, &existing_op, op->name);
    if (rc == IB_OK) {
        /* name already is registered */
        return IB_EINVAL;
    }

    rc = ib_hash_set(ib->operators, op->name, (void *)op);

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
    ib_operator_t *existing_op;

    rc = ib_hash_get(ib->stream_operators, &existing_op, op->name);
    if (rc == IB_OK) {
        /* name already is registered */
        return IB_EINVAL;
    }

    rc = ib_hash_set(ib->stream_operators, op->name, (void *)op);

    return rc;
}

ib_status_t ib_operator_create_and_register(
    ib_operator_t            **op,
    ib_engine_t               *ib,
    const char                *name,
    ib_flags_t                 capabilities,
    ib_operator_create_fn_t    fn_create,
    void                      *cbdata_create,
    ib_operator_destroy_fn_t   fn_destroy,
    void                      *cbdata_destroy,
    ib_operator_execute_fn_t   fn_execute,
    void                      *cbdata_execute
)
{
    assert(ib   != NULL);
    assert(name != NULL);

    ib_mpool_t *mp = ib_engine_pool_main_get(ib);
    assert(mp != NULL);

    ib_status_t rc;

    ib_operator_t *local_op;
    if (op == NULL) {
        op = &local_op;
    }

    rc = ib_operator_create(
        op, mp, name, capabilities,
        fn_create,  cbdata_create,
        fn_destroy, cbdata_destroy,
        fn_execute, cbdata_execute
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_operator_register(ib, *op);
    if (rc  != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_operator_stream_create_and_register(
    ib_operator_t            **op,
    ib_engine_t               *ib,
    const char                *name,
    ib_flags_t                 capabilities,
    ib_operator_create_fn_t    fn_create,
    void                      *cbdata_create,
    ib_operator_destroy_fn_t   fn_destroy,
    void                      *cbdata_destroy,
    ib_operator_execute_fn_t   fn_execute,
    void                      *cbdata_execute
)
{
    assert(ib   != NULL);
    assert(name != NULL);

    ib_mpool_t *mp = ib_engine_pool_main_get(ib);
    assert(mp != NULL);

    ib_status_t rc;

    ib_operator_t *local_op;
    if (op == NULL) {
        op = &local_op;
    }

    rc = ib_operator_create(
        op, mp, name, capabilities,
        fn_create,  cbdata_create,
        fn_destroy, cbdata_destroy,
        fn_execute, cbdata_execute
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_operator_stream_register(ib, *op);
    if (rc  != IB_OK) {
        return rc;
    }

    return IB_OK;
}

const char *ib_operator_get_name(
    const ib_operator_t *op
)
{
    assert(op != NULL);

    return op->name;
}

ib_flags_t ib_operator_get_capabilities(
    const ib_operator_t *op
)
{
    assert(op != NULL);

    return op->capabilities;
}

ib_status_t ib_operator_lookup(
    ib_engine_t          *ib,
    const char           *name,
    const ib_operator_t **op
)
{
    assert(ib != NULL);
    assert(op != NULL);

    return ib_hash_get(ib->operators, op, name);
}

ib_status_t ib_operator_stream_lookup(
    ib_engine_t          *ib,
    const char           *name,
    const ib_operator_t **op
)
{
    assert(ib != NULL);
    assert(op != NULL);

    return ib_hash_get(ib->stream_operators, op, name);
}

ib_status_t ib_operator_inst_create(
    const ib_operator_t *op,
    ib_context_t        *ctx,
    ib_flags_t           required_capabilities,
    const char          *parameters,
    void                *instance_data
)
{
    assert(op            != NULL);
    assert(instance_data != NULL);

    /* Verify that this operator is valid for this rule type */
    if (
        (op->capabilities & required_capabilities) !=
        required_capabilities
    ) {
        return IB_EINVAL;
    }

    *(void **)instance_data = NULL;

    if (op->fn_create == NULL) {
        return IB_OK;
    }

    return op->fn_create(
        ctx,
        parameters,
        instance_data,
        op->cbdata_create
    );
}

ib_status_t ib_operator_inst_destroy(
    const ib_operator_t *op,
    void                *instance_data
)
{
    assert(op != NULL);

    if (op->fn_destroy) {
        return op->fn_destroy(instance_data, op->cbdata_destroy);
    }
    else {
        return IB_OK;
    }
}

ib_status_t ib_operator_inst_execute(
    const ib_operator_t *op,
    void                *instance_data,
    ib_tx_t             *tx,
    const ib_field_t    *field,
    ib_field_t          *capture,
    ib_num_t            *result
)
{
    assert(op != NULL);

    if (op->fn_execute == NULL) {
        *result = 1;
        return IB_OK;
    }
    else {
        return op->fn_execute(
            tx,
            instance_data,
            field,
            capture,
            result,
            op->cbdata_execute
        );
    }
}
