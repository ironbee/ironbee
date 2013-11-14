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
 */

/**
 * @defgroup IronBeeActions Actions
 * @ingroup IronBee
 *
 * Actions are code that is executed in response to some trigger.
 * @{
 */

#include <ironbee/engine.h>
#include <ironbee/rule_defs.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Action Instance Structure */
typedef struct ib_action_inst_t ib_action_inst_t;

/**
 * Action instance creation callback type.
 *
 * When this function is invoked, the @a act_inst action instance has been
 * created and the actions, and params fields have been initialized.
 *
 * @param[in] ib IronBee engine.
 * @param[in] data Unparsed string with the parameters to
 *                 initialize the action instance.
 * @param[in,out] act_inst Pointer to the action instance to be initialized.
 * @param[in] cbdata Callback data passed to ib_action_register().
 *
 * @returns IB_OK if successful.
 */
typedef ib_status_t (* ib_action_create_fn_t)(
    ib_engine_t      *ib,
    const char       *data,
    ib_action_inst_t *act_inst,
    void             *cbdata
);

/**
 * Action instance destruction callback type.
 *
 * This frees any resources allocated to the instance but does not free
 * the instance itself.
 *
 * @param[in] act_inst Action Instance to be destroyed.
 * @param[in] cbdata Callback data passed to ib_action_register().
 *
 * @returns IB_OK if successful.
 */
typedef ib_status_t (* ib_action_destroy_fn_t)(
    ib_action_inst_t *act_inst,
    void             *cbdata
);

/**
 * Action instance execution callback type
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data Instance data needed for execution.
 * @param[in] cbdata Callback data passed to ib_action_register().
 *
 * @returns IB_OK if successful.
 */
typedef ib_status_t (* ib_action_execute_fn_t)(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    void                 *cbdata
);

/** Action Structure */
typedef struct ib_action_t ib_action_t;

struct ib_action_t {
    char                   *name;       /**< Name of the action. */
    ib_action_create_fn_t   fn_create;  /**< Instance creation function. */
    void                   *cbdata_create; /**< Callback data for above. */
    ib_action_destroy_fn_t  fn_destroy; /**< Instance destroy function. */
    void                   *cbdata_destroy; /**< Callback data for above. */
    ib_action_execute_fn_t  fn_execute; /**< Instance execution function. */
    void                   *cbdata_execute; /**< Callback data for execute. */
};

struct ib_action_inst_t {
    struct ib_action_t *action; /**< Pointer to the action type */
    void               *data;   /**< Data passed to the execute function */
    const char         *params; /**< Text of parameters */
    ib_field_t         *fparam; /**< Parameters as a field */
};

/**
 * Register an action.
 * This registers the name and the callbacks used to create, destroy and
 * execute an action instance.
 *
 * If the name is not unique an error status will be returned.
 *
 * @param[in] ib ironbee engine
 * @param[in] name The name of the action.
 * @param[in] fn_create A pointer to the instance creation function.
 *                      (May be NULL)
 * @param[in] cbdata_create Callback data for @a fn_create.
 * @param[in] fn_destroy A pointer to the instance destruction function.
 *                       (May be NULL)
 * @param[in] cbdata_destroy Callback data for @a fn_destroy.
 * @param[in] fn_execute A pointer to the action function.
 *                       (May be NULL)
 * @param[in] cbdata_execute Callback data for @a fn_execute.
 *
 * @returns IB_OK on success, IB_EINVAL if the name is not unique.
 */
ib_status_t ib_action_register(
    ib_engine_t            *ib,
    const char             *name,
    ib_action_create_fn_t   fn_create,
    void                   *cbdata_create,
    ib_action_destroy_fn_t  fn_destroy,
    void                   *cbdata_destroy,
    ib_action_execute_fn_t  fn_execute,
    void                   *cbdata_execute
);

/**
 * Create an action instance out of the @a ib main memory pool.
 * Looks up the action by name and executes the action creation callback.
 *
 * @param[in] ib ironbee engine.
 * @param[in] name The name of the action to create.
 * @param[in] parameters Parameters used to create the instance.
 * @param[out] act_inst The resulting instance.
 *
 * @returns IB_OK on success, IB_EINVAL if the named action does not exist.
 */
ib_status_t ib_action_inst_create(ib_engine_t *ib,
                                  const char *name,
                                  const char *parameters,
                                  ib_action_inst_t **act_inst);

/**
 * Destroy an action instance.
 * Destroys any resources held by the action instance.
 *
 * @param[in] act_inst The instance to destroy
 *
 * @returns IB_OK on success.
 */
ib_status_t ib_action_inst_destroy(ib_action_inst_t *act_inst);

/**
 * Call the execute function for an action instance.
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] act_inst Action instance to use.
 *
 * @returns IB_OK on success
 */
ib_status_t ib_action_execute(const ib_rule_exec_t *rule_exec,
                              const ib_action_inst_t *act_inst);

#ifdef __cplusplus
}
#endif

/**
 * @} IronBeeActions
 */

#endif /* _IB_ACTION_H_ */
