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
 *****************************************************************************/

/**
 * @file
 * @brief IronBee operator interface
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include <ironbee/operator.h>
#include <ironbee/debug.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>

#include "ironbee_private.h"

#include <string.h>

ib_status_t ib_register_operator(ib_engine_t *ib,
                                 const char *name,
                                 ib_operator_create_fn_t fn_create,
                                 ib_operator_destroy_fn_t fn_destroy,
                                 ib_operator_execute_fn_t fn_execute)
{
    IB_FTRACE_INIT(ib_register_operator);
    ib_hash_t *operator_hash = ib->operators;
    ib_mpool_t *pool = ib_engine_pool_main_get(ib);
    ib_status_t rc;
    char *name_copy;
    ib_operator_t *op;

    rc = ib_hash_get(operator_hash, name, &op);
    if (rc == IB_OK) {
        /* name already is registered */
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    name_copy = ib_mpool_strdup(pool, name);
    if (name_copy == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    op = (ib_operator_t *)ib_mpool_alloc(pool, sizeof(*op));
    if (op == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    op->name = name_copy;
    op->fn_create = fn_create;
    op->fn_destroy = fn_destroy;
    op->fn_execute = fn_execute;

    rc = ib_hash_set(operator_hash, name_copy, op);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_operator_inst_create(ib_engine_t *ib,
                                    const char *name,
                                    const char *parameters,
                                    ib_operator_inst_t **op_inst)
{
    IB_FTRACE_INIT(ib_operator_inst_create);
    ib_hash_t *operator_hash = ib->operators;
    ib_mpool_t *pool = ib_engine_pool_main_get(ib);
    ib_operator_t *op;
    ib_status_t rc;

    rc = ib_hash_get(operator_hash, name, &op);
    if (rc != IB_OK) {
        /* name is not registered */
        IB_FTRACE_RET_STATUS(rc);
    }

    *op_inst = (ib_operator_inst_t *)ib_mpool_alloc(pool,
                                                    sizeof(ib_operator_inst_t));
    if (*op_inst == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    (*op_inst)->op = op;

    if (op->fn_create != NULL) {
        rc = op->fn_create(pool, parameters, *op_inst);
    } else {
        rc = IB_OK;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_operator_inst_destroy(ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT(ib_operator_inst_create);
    ib_status_t rc;

    if (op_inst != NULL && op_inst->op != NULL
        && op_inst->op->fn_destroy != NULL) {
        rc = op_inst->op->fn_destroy(op_inst);
    } else {
        rc = IB_OK;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_operator_execute(const ib_operator_inst_t *op_inst,
                                ib_field_t *field,
                                ib_num_t *result)
{
    IB_FTRACE_INIT(ib_operator_execute);
    ib_status_t rc;

    if (op_inst != NULL && op_inst->op != NULL
        && op_inst->op->fn_execute != NULL) {
        rc = op_inst->op->fn_execute(op_inst->data, field, result);
    } else {
        rc = IB_OK;
    }

    IB_FTRACE_RET_STATUS(rc);
}



