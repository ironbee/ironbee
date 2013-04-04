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
    /*! Owning engine. */
    ib_engine_t *ib;

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

ib_status_t ib_operator_register(
    ib_engine_t              *ib,
    const char               *name,
    ib_flags_t                flags,
    ib_operator_create_fn_t   fn_create,
    void                     *cbdata_create,
    ib_operator_destroy_fn_t  fn_destroy,
    void                     *cbdata_destroy,
    ib_operator_execute_fn_t  fn_execute,
    void                     *cbdata_execute
)
{
    assert(ib   != NULL);
    assert(name != NULL);

    ib_mpool_t *pool = ib_engine_pool_main_get(ib);
    ib_status_t rc;
    char *name_copy;
    ib_operator_t *op;

    /* Verify that it doesn't start with '@' */
    if (*name == '@') {
        return IB_EINVAL;
    }

    rc = ib_hash_get(ib->operators, &op, name);
    if (rc == IB_OK) {
        /* name already is registered */
        return IB_EINVAL;
    }

    name_copy = ib_mpool_strdup(pool, name);
    if (name_copy == NULL) {
        return IB_EALLOC;
    }

    op = (ib_operator_t *)ib_mpool_alloc(pool, sizeof(*op));
    if (op == NULL) {
        return IB_EALLOC;
    }

    op->ib             = ib;
    op->name           = name_copy;
    op->capabilities   = flags;
    op->fn_create      = fn_create;
    op->cbdata_create  = cbdata_create;
    op->fn_destroy     = fn_destroy;
    op->cbdata_destroy = cbdata_destroy;
    op->fn_execute     = fn_execute;
    op->cbdata_execute = cbdata_execute;

    rc = ib_hash_set(ib->operators, name_copy, op);

    return rc;
}

ib_engine_t *ib_operator_get_engine(
    const ib_operator_t *op
)
{
    assert(op != NULL);

    return op->ib;
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
    ib_engine_t    *ib,
    const char     *name,
    ib_operator_t **op
)
{
    assert(ib != NULL);
    assert(op != NULL);

    return ib_hash_get(ib->operators, op, name);
}

ib_status_t ib_operator_inst_create(
    ib_operator_t  *op,
    ib_context_t   *ctx,
    ib_flags_t      required_capabilities,
    const char     *parameters,
    void          **instance_data
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

    *instance_data = NULL;

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
    ib_operator_t *op,
    void          *instance_data
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

ib_status_t ib_operator_execute(
    ib_operator_t *op,
    void          *instance_data,
    ib_tx_t       *tx,
    ib_field_t    *field,
    ib_field_t    *capture,
    ib_num_t      *result
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
