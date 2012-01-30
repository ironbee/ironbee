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

#ifndef _IB_CORE_PRIVATE_H_
#define _IB_CORE_PRIVATE_H_

/**
 * @file
 * @brief IronBee - Definitions private to the IronBee core module
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/release.h>

#include <ironbee/types.h>
#include <ironbee/list.h>
#include <ironbee/operator.h>
#include <ironbee/action.h>
#include <ironbee/engine.h>
#include <ironbee/rule_defs.h>

#include "ironbee_private.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Rule engine: Rule meta data
 */
typedef struct {
    const char            *id;            /**< Rule ID */
    ib_rule_phase_t        phase;         /**< Phase to execute rule */
} ib_rule_meta_t;

/**
 * Rule engine: condition data
 */
typedef struct {
    ib_operator_inst_t    *opinst;        /**< Condition operator instance */
} ib_rule_condition_t;

/**
 * Rule engine: action
 */
typedef struct {
    ib_action_inst_t      *action;        /**< Action */
} ib_rule_rule_action_t;

/**
 * Rule engine: Rule list
 */
typedef struct {
    ib_list_t             *rule_list;     /**< List of rules */
} ib_rulelist_t;

/**
 * Rule engine: Rule
 *
 * The typedef of ib_rule_t is done in ironbee/rule_engine.h
 */
struct ib_rule_t {
    ib_rule_meta_t         meta;          /**< Rule meta data */ 
    ib_rule_condition_t    condition;     /**< Rule condition */
    ib_list_t             *input_fields;  /**< List of input fields */
    ib_list_t             *true_actions;  /**< Actions if condition True */
    ib_list_t             *false_actions; /**< Actions if condition False */
    ib_rulelist_t         *parent_rlist;  /**< Parent rule list */
    ib_flags_t             flags;         /**< External, etc. */
};

/**
 * Rule engine: List of rules to execute during a phase
 */
typedef struct {
    ib_rule_phase_t        phase;         /**< Phase number */
    ib_rulelist_t          rules;         /**< Rules to exececute in phase */
} ib_rule_phase_data_t;

/**
 * Rule engine: Set of rules for all phases
 */
typedef struct {
    ib_rule_phase_data_t  phases[IB_RULE_PHASE_COUNT];
} ib_ruleset_t;

/**
 * Rule engine data; typedef in ironbee_private.h
 */
struct ib_rules_t {
    ib_ruleset_t       ruleset;      /**< Rules to exec */
    ib_rulelist_t      rule_list;    /**< All rules owned by this context */
};


/**
 * @internal
 * Initialize the rule engine.
 *
 * Called when the rule engine is loaded, registers event handlers.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 */
ib_status_t ib_rule_engine_init(ib_engine_t *ib,
                                ib_module_t *mod);

/**
 * @internal
 * Initialize a context the rule engine.
 *
 * Called when a context is initialized, performs rule engine initializations.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 * @param[in,out] ctx IronBee context
 */
ib_status_t ib_rule_engine_ctx_init(ib_engine_t *ib,
                                    ib_module_t *mod,
                                    ib_context_t *ctx);

/**
 * @internal
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
 * @internal
 * Initialize the core actions.
 *
 * Called when the rule engine is loaded, registers the core actions.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 */
ib_status_t ib_core_actions_init(ib_engine_t *ib,
                                 ib_module_t *mod);


#ifdef __cplusplus
}
#endif

#endif /* _IB_CORE_PRIVATE_H_ */
