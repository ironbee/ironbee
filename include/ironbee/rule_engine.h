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

#ifndef _IB_RULE_ENGINE_H_
#define _IB_RULE_ENGINE_H_

/**
 * @file
 * @brief IronBee - Rule engine definitions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>
#include <ironbee/rule_defs.h>
#include <ironbee/operator.h>
#include <ironbee/action.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Rule flag update operations.
 */
typedef enum {
    FLAG_OP_SET,                    /**< Set the flags */
    FLAG_OP_OR,                     /**< Or in the specified flags */
    FLAG_OP_CLEAR,                  /**< Clear the specified flags */
} ib_rule_flagop_t;

/**
 * Rule action add operator.
 */
typedef enum {
    RULE_ACTION_TRUE,               /**< Add a True action */
    RULE_ACTION_FALSE,              /**< Add a False action */
} ib_rule_action_t;

/**
 * Rule engine: Rule meta data
 */
typedef struct {
    const char            *id;            /**< Rule ID */
    const char            *msg;           /**< Rule message */
    const char            *data;          /**< Rule logdata */
    ib_list_t             *tags;          /**< Rule tags */
    ib_rule_type_t         type;          /**< Rule type (phase/stream) */
    ib_rule_phase_t        phase;         /**< Rule execution phase */
    ib_rule_stream_t       stream;        /**< Rule execution stream */
    uint8_t                severity;      /**< Rule severity */
    uint8_t                confidence;    /**< Rule confidence */
    ib_flags_t             flags;         /**< Rule meta-data flags */
} ib_rule_meta_t;

/**
 * Rule engine: Rule list
 */
typedef struct {
    ib_list_t             *rule_list;     /**< List of rules */
} ib_rulelist_t;

/**
 * Rule engine: Target fields
 */
typedef struct {
    const char            *field_name;    /**< The field name */
    ib_list_t             *tfn_list;      /**< List of transformations */
} ib_rule_target_t;

/**
 * Rule engine: Rule
 *
 * The typedef of ib_rule_t is done in ironbee/rule_engine.h
 */
struct ib_rule_t {
    ib_rule_meta_t         meta;          /**< Rule meta data */
    ib_operator_inst_t    *opinst;        /**< Rule operator */
    ib_list_t             *target_fields; /**< List of target fields */
    ib_list_t             *true_actions;  /**< Actions if condition True */
    ib_list_t             *false_actions; /**< Actions if condition False */
    ib_rulelist_t         *parent_rlist;  /**< Parent rule list */
    ib_rule_t             *chained_rule;  /**< Next rule in the chain */
    ib_flags_t             flags;         /**< External, etc. */
};

/**
 * List of rules to execute during a phase
 */
typedef struct {
    ib_rule_phase_t        phase;         /**< Phase number */
    ib_rulelist_t          rules;         /**< Rules to execute in phase */
} ib_rule_phase_data_t;

/**
 * List of rules to execute during on a stream
 */
typedef struct {
    ib_rule_stream_t       stream;        /**< Rule stream */
    ib_rulelist_t          rules;         /**< Rules to execute for stream */
} ib_rule_stream_data_t;

/**
 * Set of rules for all phases
 */
typedef struct {
    ib_rule_phase_data_t  phases[IB_RULE_PHASE_COUNT];
    ib_rule_stream_data_t streams[IB_RULE_STREAM_COUNT];
} ib_ruleset_t;

/**
 * Rule engine parser data
 */
typedef struct {
    ib_rule_t         *previous;     /**< Previous rule parsed */
} ib_rule_parser_data_t;

/**
 * Rule engine data; typedef in ironbee_private.h
 */
struct ib_rule_engine_t {
    ib_ruleset_t          ruleset;     /**< Rules to exec */
    ib_rulelist_t         rule_list;   /**< All rules owned by this context */
    ib_rule_parser_data_t parser_data; /**< Rule parser specific data */
};

/**
 * Create a rule.
 *
 * Allocates a rule for the rule engine, initializes it.
 *
 * @param[in] ib IronBee engine
 * @param[in] ctx Current IronBee context
 * @param[in] type Rule type (RULE_TYPE_PHASE / RULE_TYPE_STREAM)
 * @param[out] prule Address which new rule is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_create(ib_engine_t *ib,
                                      ib_context_t *ctx,
                                      ib_rule_type_t type,
                                      ib_rule_t **prule);

/**
 * Set the execution phase of a rule (for phase rules).
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] phase Rule execution phase
 *
 * @returns Status code
 */
ib_status_t ib_rule_set_phase(ib_engine_t *ib,
                              ib_rule_t *rule,
                              ib_rule_phase_t phase);

/**
 * Set the execution stream of a rule (for stream rules).
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] stream Rule execution stream
 *
 * @returns Status code
 */
ib_status_t ib_rule_set_stream(ib_engine_t *ib,
                               ib_rule_t *rule,
                               ib_rule_stream_t stream);

/**
 * Get the operator flags required for this rule.
 *
 * @param[in] rule Rule to get flags for.
 *
 * @returns Required operator flags
 */
ib_flags_t ib_rule_required_op_flags(const ib_rule_t *rule);

/**
 * Set a rule's operator.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] opinst Operator instance
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_set_operator(ib_engine_t *ib,
                                            ib_rule_t *rule,
                                            ib_operator_inst_t *opinst);

/**
 * Set a rule's ID.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] id Rule ID
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_set_id(ib_engine_t *ib,
                                      ib_rule_t *rule,
                                      const char *id);

/**
 * Get a rule's ID string.
 *
 * @param[in] rule Rule to operate on
 *
 * @returns Status code
 */
const char DLL_PUBLIC *ib_rule_id(const ib_rule_t *rule);

/**
 * Update a rule's flags.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] op Flag operation
 * @param[in] flags Flags to operate on
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_update_flags(ib_engine_t *ib,
                                            ib_rule_t *rule,
                                            ib_rule_flagop_t op,
                                            ib_flags_t flags);

/**
 * Get a rule's flags.
 *
 * @param[in] rule The rule
 *
 * @returns The rule's flags
 */
ib_flags_t DLL_PUBLIC ib_rule_flags(const ib_rule_t *rule);

/**
 * Create a rule target.
 *
 * @param[in] ib IronBee engine
 * @param[in] name Target name
 * @param[in] tfn_names List of transformations to add (or NULL)
 * @param[in,out] target Pointer to new target
 * @param[in] tfns_not_found Count of tfns names with no registered tfn
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_create_target(ib_engine_t *ib,
                                             const char *name,
                                             ib_list_t *tfn_names,
                                             ib_rule_target_t **target,
                                             ib_num_t *tfns_not_found);

/**
 * Add a target field to a rule.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] target Target object to add
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_add_target(ib_engine_t *ib,
                                          ib_rule_t *rule,
                                          ib_rule_target_t *target);

/**
 * Add a transformation to all target fields of a rule.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] name Name of the transformation to add.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_add_tfn(ib_engine_t *ib,
                                       ib_rule_t *rule,
                                       const char *name);

/**
 * Add an transformation to a target field.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] target Target field
 * @param[in] name Transformation name
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_target_add_tfn(ib_engine_t *ib,
                                              ib_rule_target_t *target,
                                              const char *name);

/**
 * Add a modifier to a rule.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] str Modifier string
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_add_modifier(ib_engine_t *ib,
                                            ib_rule_t *rule,
                                            const char *str);

/**
 * Add a modifier to a rule.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] action Action instance to add
 * @param[in] which Which action list to add to
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_add_action(ib_engine_t *ib,
                                          ib_rule_t *rule,
                                          ib_action_inst_t *action,
                                          ib_rule_action_t which);

/**
 * Register a rule.
 *
 * Register a rule for the rule engine.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] ctx Context in which to execute the rule
 * @param[in,out] rule Rule to register
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_register(ib_engine_t *ib,
                                        ib_context_t *ctx,
                                        ib_rule_t *rule);

/**
 * Get the memory pool to use for rule allocations.
 *
 * @param[in] ib IronBee engine
 *
 * @returns Pointer to the memory pool to use.
 */
ib_mpool_t DLL_PUBLIC *ib_rule_mpool(ib_engine_t *ib);

#ifdef __cplusplus
}
#endif

#endif /* _IB_RULE_ENGINE_H_ */
