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

#ifndef _IB_ACTION_H_
#define _IB_ACTION_H_

/**
 * @file
 * @brief IronBee --- Action interface
 *
 * @author Craig Forbes <cforbes@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

/**
 * @defgroup IronBeeActions Actions
 * @ingroup IronBee
 *
 * Actions perform work.
 *
 * @{
 */

#include <ironbee/engine.h>
#include <ironbee/rule_defs.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Action */
typedef struct ib_action_t ib_action_t;
/** Action Instance */
typedef struct ib_action_inst_t ib_action_inst_t;

/**
 * Action instance creation callback.
 *
 * This callback is responsible for doing any calculations needed to
 * instantiate the action, and writing a pointer to any action specific data
 * to @a instance_data.
 *
 * @param[in]  mm            Memory manager.
 * @param[in]  ctx           Context.
 * @param[in]  parameters    Parameters.
 * @param[out] instance_data Instance data to pass to execute.  Treat as
 *                           `void **`.
 * @param[in]  cbdata        Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
typedef ib_status_t (* ib_action_create_fn_t)(
    ib_mm_t       mm,
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
NONNULL_ATTRIBUTE(2, 3);

/**
 * Action instance destruction callback.
 *
 * This callback is responsible for interpreting @a instance_data and freeing
 * any resources the create function acquired.
 *
 * @param[in] instance_data Instance data produced by create.
 * @param[in] cbdata        Callback data.
 */
typedef void (* ib_action_destroy_fn_t)(
    void *instance_data,
    void *cbdata
);

/**
 * Action instance execution callback type
 *
 * This callback is responsible for executing an action given the
 * instance data create by the create callback.
 *
 * @param[in] rule_exec    The rule execution object
 * @param[in] instance_data Instance data needed for execution.
 * @param[in] cbdata       Callback data.
 *
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on memory allocation errors.
 * - IB_EOTHER something unexpected happened.
 */
typedef ib_status_t (* ib_action_execute_fn_t)(
    const ib_rule_exec_t *rule_exec,
    void                 *instance_data,
    void                 *cbdata
);

/**
 * Create an action.
 *
 * All callbacks may be NULL.
 *
 * @param[out] action         Created action.
 * @param[in]  mm             Memory manager.
 * @param[in]  name           The name of the action.
 * @param[in]  create_fn      Create function.
 * @param[in]  create_cbdata  Create callback data.
 * @param[in]  destroy_fn     Destroy function.
 * @param[in]  destroy_cbdata Destroy callback data.
 * @param[in]  execute_fn     Execute function.
 * @param[in]  execute_cbdata Execute callback data.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_action_create(
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
NONNULL_ATTRIBUTE(1, 3, 8);

/**
 * Register action with engine.
 *
 * If the name is not unique an error status will be returned.
 *
 * @param[in] ib     IronBee engine.
 * @param[in] action Action to register.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if an action with same name already exists.
 */
ib_status_t DLL_PUBLIC ib_action_register(
    ib_engine_t       *ib,
    const ib_action_t *action
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Create and register an action.
 *
 * @sa ib_action_create()
 * @sa ib_action_register()
 *
 * @param[out] action          Created action.  May be NULL.
 * @param[in]  ib              IronBee engine.
 * @param[in]  name            The name of the action.
 * @param[in]  create_fn       Create function.
 * @param[in]  create_cbdata   Create callback data.
 * @param[in]  destroy_fn      Destroy function.
 * @param[in]  destroy_cbdata  Destroy callback data.
 * @param[in]  execute_fn      Execute function.
 * @param[in]  execute_cbdata  Execute callback data.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if an action with same name exists.
 */
ib_status_t DLL_PUBLIC ib_action_create_and_register(
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
NONNULL_ATTRIBUTE(2, 3);

/**
 * Lookup an action by name.
 *
 * @param[in]  ib          IronBee engine.
 * @param[in]  name        Name of action.
 * @param[in]  name_length Length of @a name.
 * @param[out] action      Action if found.
 *
 * @return
 * - IB_OK on success.
 * - IB_ENOENT if no such action.
 */
ib_status_t DLL_PUBLIC ib_action_lookup(
    ib_engine_t        *ib,
    const char         *name,
    size_t              name_length,
    const ib_action_t **action
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Name accessor.
 *
 * @param[in] action Action.
 *
 * @return Name.
 */
const char DLL_PUBLIC *ib_action_name(
    const ib_action_t *action
)
NONNULL_ATTRIBUTE(1);

/**
 * Create an action instance.
 *
 * The destroy function will be register to be called when @a mm is cleaned
 * up.
 *
 * @param[out] act_inst   The action instance.
 * @param[in]  mm         Memory manager.
 * @param[in]  ctx        Context
 * @param[in]  action     Action to create an instance of.
 * @param[in]  parameters Parameters used to create the instance.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - Other if create callback fails.
 */
ib_status_t DLL_PUBLIC ib_action_inst_create(
    ib_action_inst_t  **act_inst,
    ib_mm_t             mm,
    ib_context_t       *ctx,
    const ib_action_t  *action,
    const char         *parameters
)
NONNULL_ATTRIBUTE(1, 3, 4);

/**
 * Get the action of an action instance.
 *
 * @param[in] action_inst Action instance to access.
 *
 * @return Action of action instance.
 */
const ib_action_t DLL_PUBLIC *ib_action_inst_action(
    const ib_action_inst_t *action_inst
)
NONNULL_ATTRIBUTE(1);

/**
 * Get the parameters of an action instance.
 *
 * @param[in] action_inst Action instance to access.
 *
 * @return Parameters of action instance.
 */
const char DLL_PUBLIC *ib_action_inst_parameters(
    const ib_action_inst_t *action_inst
)
NONNULL_ATTRIBUTE(1);

/**
 * Get the instance data of an action instance.
 *
 * @param[in] action_inst Action instance to access.
 *
 * @return Data of action instance.
 */
void DLL_PUBLIC *ib_action_inst_data(
    const ib_action_inst_t *action_inst
)
NONNULL_ATTRIBUTE(1);

/**
 * Execute action.
 *
 * @param[in] act_inst  Action instance.
 * @param[in] rule_exec The rule execution object
 *
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - Other on other failure.
 */
ib_status_t DLL_PUBLIC ib_action_inst_execute(
    const ib_action_inst_t *act_inst,
    const ib_rule_exec_t   *rule_exec
)
NONNULL_ATTRIBUTE(1);

#ifdef __cplusplus
}
#endif

/**
 * @} IronBeeActions
 */

#endif /* _IB_ACTION_H_ */
