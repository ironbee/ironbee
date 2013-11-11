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
    ib_rule_t  *rule;  /**< The rule itself */
    ib_flags_t  flags; /**< Rule flags (IB_RULECTX_FLAG_xx) */
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
    ib_ruleset_phase_t phases[IB_RULE_PHASE_COUNT];
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
    ib_var_target_t *target;
    const char      *target_str; /**< The target string */
    ib_list_t       *tfn_list;   /**< List of transformations */
};

/**
 * Rule engine.
 */
struct ib_rule_engine_t {
    ib_list_t *rule_list;        /**< All registered rules. */
    ib_hash_t *rule_hash;        /**< All rules by rule-id. */
    ib_hash_t *external_drivers; /**< Drivers for external rules. */
    ib_list_t *ownership_cbs;    /**< List of ownership callbacks. */

    /**
     * Rule injection callbacks.
     */
    ib_list_t *injection_cbs[IB_RULE_PHASE_COUNT];

    /**
     * Error page function.
     */
    ib_rule_error_page_fn_t  error_page_fn;

    /**
     * Callback data for ib_rule_engine_t::error_page_cbdata.
     */
    void *error_page_cbdata;

    /* Var Sources */
    struct {
        ib_var_source_t *field;
        ib_var_source_t *field_target;
        ib_var_source_t *field_tfn;
        ib_var_source_t *field_name;
        ib_var_source_t *field_name_full;
    } source;
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
 * Rule engine operator instance object.
 */
struct ib_rule_operator_inst_t {
    const struct ib_operator_t *op;/**< Pointer to the operator type */
    bool                        invert;  /**< Invert operator? */
    void                       *instance_data; /**< Instance data */
    const char                 *params;  /**< Parameters passed to create */
    ib_field_t                 *fparam;  /**< Parameters as a field */
};

/**
 * Initialize the rule engine.
 *
 * Called when the rule engine is loaded, registers event handlers.
 *
 * @param[in,out] ib IronBee object
 */
ib_status_t ib_rule_engine_init(
    ib_engine_t                *ib);


#endif /* IB_RULE_ENGINE_PRIVATE_H_ */
