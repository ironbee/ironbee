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

#ifndef _IB_CORE_PRIVATE_H_
#define _IB_CORE_PRIVATE_H_

/**
 * @file
 * @brief IronBee --- Engine Private Declarations
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "core_audit_private.h"

#include <ironbee/context_selection.h>
#include <ironbee/engine.h>
#include <ironbee/types.h>
#include <ironbee/var.h>

typedef struct {
    const char      *name;          /**< Flag name */
    const char      *tx_name;       /**< Name in the TX "FLAGS" collection */
    ib_flags_t       tx_flag;       /**< TX flag value */
    bool             read_only;     /**< Is setflag valid for this flag? */
    bool             default_value; /**< The flag's default value? */
    ib_var_target_t *target;        /**< Var target of tx_name. */
} ib_tx_flag_map_t;

/** Core-module-specific non-context-aware data accessed via module->data */
typedef struct {
    ib_list_t            *site_list;      /**< List: ib_site_t */
    ib_list_t            *selector_list;  /**< List: core_site_selector_t */
    ib_context_t         *cur_ctx;        /**< Current context */
    ib_site_t            *cur_site;       /**< Current site */
    ib_site_location_t   *cur_location;   /**< Current location */
} ib_core_module_data_t;

/** Core module transaction data */
typedef struct {
    ib_num_t              auditlog_parts; /**< Audit log parts */
} ib_core_module_tx_data_t;

/**
 * Get the core module symbol structure
 *
 * @returns Core module static symbol
 */
ib_module_t *ib_core_module_sym(void);

/**
 * Initialize the core fields.
 *
 * Called when the core is loaded, registers the core field generators.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 */
ib_status_t ib_core_vars_init(ib_engine_t *ib,
                              ib_module_t *mod);

/**
 * Initialize the core config context for fields.
 *
 * Called when the core is loaded, registers the core field generators.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 * @param[in] ctx Context.
 * @param[in] cbdata Callback data.
 *
 * @returns IB_OK on success.
 */
ib_status_t ib_core_vars_ctx_init(ib_engine_t *ib,
                                  ib_module_t *mod,
                                  ib_context_t *ctx,
                                  void *cbdata);

/**
 * Get the core flags collection
 *
 * @returns Pointer to array of ib_tx_flag_map_t
 */
const ib_tx_flag_map_t *ib_core_vars_tx_flags( void );

/**
 * Get the core audit log parts string/value configuration map
 *
 * @param[out] pmap Pointer to the configuration map
 *
 * @returns Status code (IB_OK)
 */
ib_status_t ib_core_auditlog_parts_map(
    const ib_strval_t   **pmap);


/**
 * Get the core mode and data
 *
 * @param[in] engine IronBee engine
 * @param[out] core_module Pointer to core module (or NULL)
 * @param[out] core_data Pointer to core data (or NULL)
 *
 * @returns IB_OK / return value from ib_engine_module_get()
 */
ib_status_t ib_core_module_data(
    ib_engine_t            *engine,
    ib_module_t           **core_module,
    ib_core_module_data_t **core_data);

/**
 * Initialize the core transformations.
 *
 * Called when the rule engine is loaded; registers the core transformations.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 */
ib_status_t ib_core_transformations_init(ib_engine_t *ib,
                                         ib_module_t *mod);

/**
 * Initialize the core operators.
 *
 * Called when the rule engine is loaded, registers the core operators.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 */
ib_status_t ib_core_operators_init(ib_engine_t *ib,
                                   ib_module_t *mod);

/**
 * Initialize the core actions.
 *
 * Called when the rule engine is loaded; registers the core actions.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 */
ib_status_t ib_core_actions_init(ib_engine_t *ib,
                                 ib_module_t *mod);


/**
 * Core site selection functions
 */


/**
 * Initialize the core context selection
 *
 * Called when the rule engine is loaded; registers the core context selection
 * functions.
 *
 * @param[in,out] ib IronBee object
 * @param[in] module Module object
 */
ib_status_t ib_core_ctxsel_init(ib_engine_t *ib,
                                ib_module_t *module);


/**
 * Initialize engine-scoped values of the flags var structure.
 *
 * @param[in] ib The engine to register the `FLAGS:*` values with.
 */
void ib_core_vars_tx_flags_init(ib_engine_t *ib);

/** Core collection managers functions */

/**
 * Register core collections managers
 *
 * @param[in] ib Engine
 * @param[in] module Collection manager's module object
 *
 * @returns Status code:
 *   - IB_OK All OK, parameters recognized
 *   - IB_Exxx Other error
 */
ib_status_t ib_core_collection_managers_register(
    ib_engine_t       *ib,
    const ib_module_t *module);

/**
 * Shut down core collections managers
 *
 * @param[in] ib Engine
 * @param[in] module Collection manager's module object
 *
 * @returns Status code:
 *   - IB_OK All OK
 */
ib_status_t ib_core_collection_managers_finish(
    ib_engine_t       *ib,
    const ib_module_t *module);


#endif /* _IB_CORE_PRIVATE_H_ */
