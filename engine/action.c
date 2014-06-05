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
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/action.h>

#include "engine_private.h"

#include <assert.h>

struct ib_action_t {
    /*! Name of the action. */
    char *name;

    /*! Instance creation function. */
    ib_action_create_fn_t create_fn;

    /*! Create callback data. */
    void *create_cbdata;

    /*! Instance destroy function. */
    ib_action_destroy_fn_t destroy_fn;

    /*! Destroy callback data. */
    void *destroy_cbdata;

    /*! Instance execution function. */
    ib_action_execute_fn_t execute_fn;

    /*! Execute callback data. */
    void *execute_cbdata;
};

struct ib_action_inst_t
{
    /*! Action. */
    const ib_action_t *action;

    /*! Parameters. */
    const char *parameters;

    /*! Instance data. */
    void *instance_data;
};

ib_status_t ib_action_create(
    ib_action_t            **action,
    ib_mm_t                  mm,
    const char              *name,
    ib_action_create_fn_t    create_fn,
    void                    *create_cbdata,
    ib_action_destroy_fn_t   destroy_fn,
    void                    *destroy_cbdata,
    ib_action_execute_fn_t   execute_fn,
    void                    *execute_cbdata
)
{
    assert(action != NULL);
    assert(name != NULL);

    ib_action_t *local_action;

    local_action = (ib_action_t *)ib_mm_alloc(mm, sizeof(*local_action));
    if (local_action == NULL) {
        return IB_EALLOC;
    }
    local_action->name = ib_mm_strdup(mm, name);
    if (local_action->name == NULL) {
        return IB_EALLOC;
    }
    local_action->create_fn      = create_fn;
    local_action->create_cbdata  = create_cbdata;
    local_action->destroy_fn     = destroy_fn;
    local_action->destroy_cbdata = destroy_cbdata;
    local_action->execute_fn     = execute_fn;
    local_action->execute_cbdata = execute_cbdata;

    *action = local_action;

    return IB_OK;
}

ib_status_t ib_action_register(
    ib_engine_t       *ib,
    const ib_action_t *action
)
{
    assert(ib != NULL);
    assert(action != NULL);

    ib_status_t rc;
    ib_hash_t *action_hash = ib->actions;

    rc = ib_hash_get(action_hash, NULL, action->name);
    if (rc == IB_OK) {
        /* Already exists. */
        return IB_EINVAL;
    }

    rc = ib_hash_set(action_hash, ib_action_name(action), (void *)action);

    return rc;
}

ib_status_t ib_action_create_and_register(
    ib_action_t            **action,
    ib_engine_t             *ib,
    const char              *name,
    ib_action_create_fn_t    create_fn,
    void                    *create_cbdata,
    ib_action_destroy_fn_t   destroy_fn,
    void                    *destroy_cbdata,
    ib_action_execute_fn_t   execute_fn,
    void                    *execute_cbdata
)
{
    assert(ib != NULL);
    assert(name != NULL);

    ib_action_t *local_action;
    ib_status_t rc;

    rc = ib_action_create(
        &local_action,
        ib_engine_mm_main_get(ib),
        name,
        create_fn, create_cbdata,
        destroy_fn, destroy_cbdata,
        execute_fn, execute_cbdata
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_action_register(ib, local_action);
    if (rc != IB_OK) {
        return rc;
    }

    if (action != NULL) {
        *action = local_action;
    }

    return IB_OK;
}

ib_status_t ib_action_lookup(
    ib_engine_t        *ib,
    const char         *name,
    size_t              name_length,
    const ib_action_t **action
)
{
    assert(ib != NULL);
    assert(name != NULL);

    return ib_hash_get_ex(ib->actions, action, name, name_length);
}

const char *ib_action_name(
    const ib_action_t *action
)
{
    assert(action != NULL);

    return action->name;
}

/*! Cleanup function to destroy action. */
static
void cleanup_action(
    void *cbdata
)
{
    const ib_action_inst_t *action_inst =
        (const ib_action_inst_t *)cbdata;
    assert(action_inst != NULL);
    const ib_action_t *action =
        ib_action_inst_action(action_inst);
    assert(action != NULL);

    /* Will only be called if there is a destroy function. */
    assert(action->destroy_fn);
    action->destroy_fn(
        action_inst->instance_data,
        action->destroy_cbdata
    );
}

ib_status_t ib_action_inst_create(
    ib_action_inst_t  **act_inst,
    ib_mm_t             mm,
    ib_context_t       *ctx,
    const ib_action_t  *action,
    const char         *parameters
)
{
    assert(action != NULL);
    assert(act_inst != NULL);
    assert(ctx != NULL);

    ib_action_inst_t *local_action_inst = NULL;
    ib_status_t rc;

    local_action_inst =
        (ib_action_inst_t *)ib_mm_alloc(mm, sizeof(*local_action_inst));
    if (local_action_inst == NULL) {
        return IB_EALLOC;
    }
    if (parameters != NULL) {
        local_action_inst->parameters = ib_mm_strdup(mm, parameters);
        if (local_action_inst->parameters == NULL) {
            return IB_EALLOC;
        }
    }
    else {
        local_action_inst->parameters = NULL;
    }
    local_action_inst->action = action;

    if (action->create_fn == NULL) {
        local_action_inst->instance_data = NULL;
    }
    else {
        rc = action->create_fn(
            mm,
            ctx,
            parameters,
            &(local_action_inst->instance_data),
            action->create_cbdata
        );
        if (rc != IB_OK) {
            return rc;
        }
    }

    if (action->destroy_fn != NULL) {
        /* Register the destroy function. */
        rc = ib_mm_register_cleanup(mm, cleanup_action, local_action_inst);
        if (rc != IB_OK) {
            return rc;
        }
    }

    *act_inst = local_action_inst;

    return IB_OK;
}

const ib_action_t *ib_action_inst_action(
    const ib_action_inst_t *act_inst
)
{
    assert(act_inst != NULL);

    return act_inst->action;
}

const char *ib_action_inst_parameters(
    const ib_action_inst_t *act_inst
)
{
    assert(act_inst != NULL);

    return act_inst->parameters;
}

void *ib_action_inst_data(
    const ib_action_inst_t *act_inst
)
{
    assert(act_inst != NULL);

    return act_inst->instance_data;
}

ib_status_t ib_action_inst_execute(
    const ib_action_inst_t *act_inst,
    const ib_rule_exec_t   *rule_exec
)
{
    assert(act_inst != NULL);

    const ib_action_t *action = ib_action_inst_action(act_inst);

    assert(action != NULL);

    if (action->execute_fn != NULL) {
        return action->execute_fn(
            rule_exec,
            ib_action_inst_data(act_inst),
            action->execute_cbdata
        );
    }
    else {
        return IB_OK;
    }
}
