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
 * @brief IronBee --- Action interface
 *
 * @author Craig Forbes <cforbes@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/action.h>

#include "engine_private.h"

#include <ironbee/mpool.h>

#include <assert.h>
#include <string.h>

ib_status_t ib_action_register(
    ib_engine_t            *ib,
    const char             *name,
    ib_action_create_fn_t   fn_create,
    void                   *cbdata_create,
    ib_action_destroy_fn_t  fn_destroy,
    void                   *cbdata_destroy,
    ib_action_execute_fn_t  fn_execute,
    void                   *cbdata_execute
)
{
    ib_hash_t *action_hash = ib->actions;
    ib_mpool_t *pool = ib_engine_pool_main_get(ib);
    ib_status_t rc;
    char *name_copy;
    ib_action_t *act;

    rc = ib_hash_get(action_hash, &act, name);
    if (rc == IB_OK) {
        /* name already is registered */
        return IB_EINVAL;
    }

    name_copy = ib_mpool_strdup(pool, name);
    if (name_copy == NULL) {
        return IB_EALLOC;
    }

    act = (ib_action_t *)ib_mpool_alloc(pool, sizeof(*act));
    if (act == NULL) {
        return IB_EALLOC;
    }
    act->name           = name_copy;
    act->fn_create      = fn_create;
    act->cbdata_create  = cbdata_create;
    act->fn_destroy     = fn_destroy;
    act->cbdata_destroy = cbdata_destroy;
    act->fn_execute     = fn_execute;
    act->cbdata_execute = cbdata_execute;

    rc = ib_hash_set(action_hash, name_copy, act);

    return rc;
}

ib_status_t ib_action_inst_create(
    ib_engine_t *ib,
    const char *name,
    const char *parameters,
    ib_action_inst_t **act_inst)
{
    assert(ib != NULL);
    assert(name != NULL);

    ib_hash_t *action_hash = ib->actions;
    ib_action_t *action;
    ib_status_t rc;
    ib_mpool_t *mpool = ib_engine_pool_main_get(ib);

    assert(mpool != NULL);

    rc = ib_hash_get(action_hash, &action, name);
    if (rc != IB_OK) {
        /* name is not registered */
        return rc;
    }

    *act_inst = (ib_action_inst_t *)ib_mpool_alloc(mpool,
                                                   sizeof(ib_action_inst_t));
    if (*act_inst == NULL) {
        return IB_EALLOC;
    }
    (*act_inst)->action = action;
    (*act_inst)->params = ib_mpool_strdup(mpool, parameters);
    (*act_inst)->fparam = NULL;

    if (action->fn_create != NULL) {
        rc = action->fn_create(
            ib,
            parameters,
            *act_inst,
            action->cbdata_create
        );
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        rc = IB_OK;
    }

    if ((*act_inst)->fparam == NULL) {
        rc = ib_field_create(&((*act_inst)->fparam),
                             mpool,
                             IB_FIELD_NAME("param"),
                             IB_FTYPE_NULSTR,
                             ib_ftype_nulstr_in(parameters));
    }

    return rc;
}

ib_status_t ib_action_inst_destroy(ib_action_inst_t *act_inst)
{
    ib_status_t rc;

    if (act_inst != NULL && act_inst->action != NULL
        && act_inst->action->fn_destroy != NULL) {
        rc = act_inst->action->fn_destroy(
            act_inst,
            act_inst->action->cbdata_destroy
        );
    }
    else {
        rc = IB_OK;
    }

    return rc;
}

ib_status_t ib_action_execute(const ib_rule_exec_t *rule_exec,
                              const ib_action_inst_t *act_inst)
{
    ib_status_t rc;

    if (act_inst != NULL && act_inst->action != NULL
        && act_inst->action->fn_execute != NULL) {
        rc = act_inst->action->fn_execute(
            rule_exec,
            act_inst->data,
            act_inst->action->cbdata_execute
        );
    }
    else {
        rc = IB_OK;
    }

    return rc;
}

const char DLL_PUBLIC *ib_action_name(const ib_action_t *action)
{
    return action->name;
}

