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
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/operator.h>

#include "engine_private.h"

#include <ironbee/debug.h>
#include <ironbee/mpool.h>

ib_status_t ib_operator_register(ib_engine_t *ib,
                                 const char *name,
                                 ib_flags_t flags,
                                 ib_operator_create_fn_t fn_create,
                                 void *cd_create,
                                 ib_operator_destroy_fn_t fn_destroy,
                                 void *cd_destroy,
                                 ib_operator_execute_fn_t fn_execute,
                                 void *cd_execute)
{
    IB_FTRACE_INIT();
    ib_hash_t *operator_hash = ib->operators;
    ib_mpool_t *pool = ib_engine_pool_main_get(ib);
    ib_status_t rc;
    char *name_copy;
    ib_operator_t *op;

    /* Verify that it doesn't start with '@' */
    if (*name == '@') {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_hash_get(operator_hash, &op, name);
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
    op->flags = flags;
    op->cd_create = cd_create;
    op->cd_destroy = cd_destroy;
    op->cd_execute = cd_execute;
    op->fn_create = fn_create;
    op->fn_destroy = fn_destroy;
    op->fn_execute = fn_execute;

    rc = ib_hash_set(operator_hash, name_copy, op);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_operator_inst_create(ib_engine_t *ib,
                                    ib_context_t *ctx,
                                    const ib_rule_t *rule,
                                    ib_flags_t required_op_flags,
                                    const char *name,
                                    const char *parameters,
                                    ib_flags_t flags,
                                    ib_operator_inst_t **op_inst)
{
    IB_FTRACE_INIT();
    ib_hash_t *operator_hash = ib->operators;
    ib_mpool_t *pool = ib_engine_pool_main_get(ib);
    ib_operator_t *op;
    ib_status_t rc;

    rc = ib_hash_get(operator_hash, &op, name);
    if (rc != IB_OK) {
        /* name is not registered */
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Verify that this operator is valid for this rule type */
    if ( (op->flags & required_op_flags) != required_op_flags) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    *op_inst = (ib_operator_inst_t *)
        ib_mpool_alloc(pool, sizeof(ib_operator_inst_t));
    if (*op_inst == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    (*op_inst)->op = op;
    (*op_inst)->flags = flags;
    (*op_inst)->params = ib_mpool_strdup(pool, parameters);
    (*op_inst)->fparam = NULL;

    if (op->fn_create != NULL) {
        rc = op->fn_create(ib, ctx, rule, pool, parameters, *op_inst);
    }
    else {
        rc = IB_OK;
    }

    if ((*op_inst)->fparam == NULL) {
        rc = ib_field_create(&((*op_inst)->fparam),
                             pool,
                             IB_FIELD_NAME("param"),
                             IB_FTYPE_NULSTR,
                             ib_ftype_nulstr_in(parameters));
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_operator_inst_destroy(ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if ((op_inst != NULL) && (op_inst->op != NULL)
        && (op_inst->op->fn_destroy != NULL)) {
        rc = op_inst->op->fn_destroy(op_inst);
    }
    else {
        rc = IB_OK;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_operator_execute(const ib_rule_exec_t *rule_exec,
                                const ib_operator_inst_t *op_inst,
                                ib_field_t *field,
                                ib_num_t *result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if ((op_inst != NULL) && (op_inst->op != NULL)
        && (op_inst->op->fn_execute != NULL))
    {
        rc = op_inst->op->fn_execute(
            rule_exec, op_inst->data, op_inst->flags, field, result);
    }
    else {
        *result = 1;
        rc = IB_OK;
    }

    IB_FTRACE_RET_STATUS(rc);
}
