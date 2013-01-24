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

#ifndef _IB_RULE_ENGINE_PRIVATE_H_
#define _IB_RULE_ENGINE_PRIVATE_H_

/**
 * @file
 * @brief IronBee --- Rule Engine Private Declarations
 *
 * These definitions and routines are called by core and nowhere else.
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/clock.h>
#include <ironbee/rule_engine.h>
#include <ironbee/types.h>

/**
 * Context-specific rule object.  This is the type of the objects
 * stored in the 'rule_list' field of ib_ruleset_phase_t.
 */
typedef struct {
    ib_rule_t             *rule;         /**< The rule itself */
    ib_flags_t             flags;        /**< Rule flags (IB_RULECTX_FLAG_xx) */
} ib_rule_ctx_data_t;

/**
 * Ruleset for a single phase.
 *  rule_list is a list of pointers to ib_rule_ctx_data_t objects.
 */
typedef struct {
    ib_rule_phase_num_t         phase_num;   /**< Phase number */
    const ib_rule_phase_meta_t *phase_meta;  /**< Rule phase meta-data */
    ib_list_t                  *rule_list;   /**< Rules to execute in phase */
} ib_ruleset_phase_t;

/**
 * Set of rules for all phases.
 * The elements of the phases list are ib_rule_ctx_data_t objects.
 */
typedef struct {
    ib_ruleset_phase_t     phases[IB_RULE_PHASE_COUNT];
} ib_ruleset_t;

/**
 * Data on enable directives.
 */
typedef struct {
    ib_rule_enable_type_t  enable_type;  /**< Enable All / by ID / by Tag */
    const char            *enable_str;   /**< String of ID or Tag */
    const char            *file;         /**< Configuration file of enable */
    unsigned int           lineno;       /**< Line number in config file */
} ib_rule_enable_t;

/**
 * Rules data for each context.
 */
struct ib_rule_context_t {
    ib_ruleset_t           ruleset;      /**< Rules to exec */
    ib_list_t             *rule_list;    /**< All rules owned by context */
    ib_hash_t             *rule_hash;    /**< Hash of rules (by rule-id) */
    ib_list_t             *enable_list;  /**< Enable All/IDs/tags */
    ib_list_t             *disable_list; /**< All/IDs/tags disabled */
    ib_rule_parser_data_t  parser_data;  /**< Rule parser specific data */
};

/**
 * Rule target fields
 */
struct ib_rule_target_t {
    const char            *field_name;    /**< The field name */
    const char            *target_str;    /**< The target string */
    ib_list_t             *tfn_list;      /**< List of transformations */
};

/**
 * Rule engine.
 */
struct ib_rule_engine_t {
    ib_list_t            *rule_list;        /**< List of all registered rules */
    ib_hash_t            *rule_hash;        /**< Hash of rules (by rule-id) */
    ib_hash_t            *external_drivers; /**< Drivers for external rules. */
    ib_list_t            *ownership_cbs;   /**< List of ownership callbacks */
    ib_list_t *injection_cbs[IB_RULE_PHASE_COUNT]; /**< Rule inj. callbacks*/
};

/**
 * Rule ownership callback object
 */
struct ib_rule_ownership_cb_t {
    const char             *name;        /**< Ownership callback name */
    ib_rule_ownership_fn_t  fn;          /**< Ownership function */
    void                   *data;        /**< Hook data */
};
typedef struct ib_rule_ownership_cb_t ib_rule_ownership_cb_t;

/**
 * Rule injection callback object
 */
struct ib_rule_injection_cb_t {
    const char             *name;        /**< Rule injector name */
    ib_rule_injection_fn_t  fn;          /**< Rule injection function */
    void                   *data;        /**< Rule injection data */
};
typedef struct ib_rule_injection_cb_t ib_rule_injection_cb_t;

/**
 * Initialize the rule engine.
 *
 * Called when the rule engine is loaded, registers event handlers.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 */
ib_status_t ib_rule_engine_init(
    ib_engine_t                *ib,
    ib_module_t                *mod);

/**
 * Create a rule execution object
 *
 * @param[in] tx Transaction.
 * @param[out] rule_exec Rule execution object (or NULL)
 *
 * @returns
 *   - IB_OK on success.
 */
ib_status_t ib_rule_exec_create(
    ib_tx_t                    *tx,
    ib_rule_exec_t            **rule_exec);

/**
 * Rule engine context open
 *
 * Called when a context is opened; performs rule engine context-specific
 * initializations.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 * @param[in,out] ctx IronBee context
 */
ib_status_t ib_rule_engine_ctx_open(
    ib_engine_t                *ib,
    ib_module_t                *mod,
    ib_context_t               *ctx);

/**
 * Close a context for the rule engine.
 *
 * Called when a context is closed; performs rule engine rule fixups.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 * @param[in,out] ctx IronBee context
 */
ib_status_t ib_rule_engine_ctx_close(
    ib_engine_t                *ib,
    ib_module_t                *mod,
    ib_context_t               *ctx);


#endif /* IB_RULE_ENGINE_PRIVATE_H_ */
