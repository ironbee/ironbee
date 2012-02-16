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
 * @brief IronBee action interface
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/action.h>
#include <ironbee/debug.h>
#include <ironbee/mpool.h>

#include "ironbee_private.h"

#include <string.h>

ib_status_t ib_action_register(ib_engine_t *ib,
                               const char *name,
                               ib_flags_t flags,
                               ib_action_create_fn_t fn_create,
                               ib_action_destroy_fn_t fn_destroy,
                               ib_action_execute_fn_t fn_execute)
{
    IB_FTRACE_INIT();
    ib_hash_t *action_hash = ib->actions;
    ib_mpool_t *pool = ib_engine_pool_main_get(ib);
    ib_status_t rc;
    char *name_copy;
    ib_action_t *act;

    rc = ib_hash_get(action_hash, &act, name);
    if (rc == IB_OK) {
        /* name already is registered */
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    name_copy = ib_mpool_strdup(pool, name);
    if (name_copy == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    act = (ib_action_t *)ib_mpool_alloc(pool, sizeof(*act));
    if (act == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    act->name = name_copy;
    act->flags = flags;
    act->fn_create = fn_create;
    act->fn_destroy = fn_destroy;
    act->fn_execute = fn_execute;

    rc = ib_hash_set(action_hash, name_copy, act);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_action_inst_create(ib_engine_t *ib,
                                  ib_context_t *ctx,
                                  const char *name,
                                  const char *parameters,
                                  ib_flags_t flags,
                                  ib_action_inst_t **act_inst)
{
    IB_FTRACE_INIT();
    ib_hash_t *action_hash = ib->actions;
    ib_mpool_t *pool = ib_engine_pool_main_get(ib);
    ib_action_t *action;
    ib_status_t rc;

    rc = ib_hash_get(action_hash, &action, name);
    if (rc != IB_OK) {
        /* name is not registered */
        IB_FTRACE_RET_STATUS(rc);
    }

    *act_inst = (ib_action_inst_t *)ib_mpool_alloc(pool,
                                                   sizeof(ib_action_inst_t));
    if (*act_inst == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    (*act_inst)->action = action;
    (*act_inst)->flags = flags;

    if (action->fn_create != NULL) {
        rc = action->fn_create(ib, ctx, pool, parameters, *act_inst);
    } else {
        rc = IB_OK;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_action_inst_destroy(ib_action_inst_t *act_inst)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if (act_inst != NULL && act_inst->action != NULL
        && act_inst->action->fn_destroy != NULL) {
        rc = act_inst->action->fn_destroy(act_inst);
    } else {
        rc = IB_OK;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_action_execute(const ib_action_inst_t *act_inst,
                              ib_rule_t *rule,
                              ib_tx_t *tx)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if (act_inst != NULL && act_inst->action != NULL
        && act_inst->action->fn_execute != NULL) {
        rc = act_inst->action->fn_execute(act_inst->data, rule, tx);
    } else {
        rc = IB_OK;
    }

    IB_FTRACE_RET_STATUS(rc);
}
