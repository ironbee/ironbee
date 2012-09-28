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

#include <ironbee/engine.h>
#include <ironbee/types.h>

typedef struct {
    const char      *name;          /**< Flag name */
    const char      *tx_name;       /**< Name in the TX "FLAGS" collection */
    ib_flags_t       tx_flag;       /**< TX flag value */
    bool             read_only;     /**< Is setflag valid for this flag? */
    bool             default_value; /**< The flag's default value? */
} ib_tx_flag_map_t;

/**
 * Initialize the core fields.
 *
 * Called when the core is loaded, registers the core field generators.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 */
ib_status_t ib_core_fields_init(ib_engine_t *ib,
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
ib_status_t ib_core_fields_ctx_init(ib_engine_t *ib,
                                    ib_module_t *mod,
                                    ib_context_t *ctx,
                                    void *cbdata);

/**
 * Get the core flags collection
 *
 * @returns Pointer to array of ib_tx_flag_map_t
 */
const ib_tx_flag_map_t *ib_core_fields_tx_flags( void );

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


#endif /* _IB_CORE_PRIVATE_H_ */
