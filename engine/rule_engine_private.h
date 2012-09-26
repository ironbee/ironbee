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
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/clock.h>
#include <ironbee/rule_engine.h>
#include <ironbee/types.h>

/**
 * Rule transformation results for logging.
 */
struct ib_rule_log_tfn_t {
    const ib_tfn_t         *tfn;         /**< Transformation */
    const ib_field_t       *in;          /**< Value before transformation */
    const ib_field_t       *out;         /**< Value after transformation */
    ib_status_t             status;      /**< Transformation return status */
};
typedef struct ib_rule_log_tfn_t ib_rule_log_tfn_t;

/**
 * Rule action for logging.
 */
struct ib_rule_log_act_t {
    const ib_action_inst_t *act_inst;    /**< Action instance */
    ib_status_t             status;      /**< Transformation return status */
};
typedef struct ib_rule_log_act_t ib_rule_log_act_t;

/**
 * Rule result counts for logging.
 */
struct ib_rule_log_count_t {
    int                     num_actions; /**< Total # of actions executed */
    int                     num_events;  /**< Total # of events */
    int                     num_errors;  /**< Total # of operator errors */
    int                     num_true;    /**< Total # of true results */
    int                     num_false;   /**< Total # of false results */
};
typedef struct ib_rule_log_count_t ib_rule_log_count_t;

/**
 * Rule result for logging.
 */
struct ib_rule_log_rslt_t {
    const ib_field_t       *value;       /**< Value passed to operator */
    ib_num_t                result;      /**< Result of operator */
    ib_status_t             status;      /**< Operator return status */
    ib_list_t              *act_list;    /**< List of executed actions */
    int                     num_actions; /**< # of actions */
    ib_list_t              *event_list;  /**< List of events created */
    int                     num_events;  /**< # of events */
};
typedef struct ib_rule_log_rslt_t ib_rule_log_rslt_t;

/**
 * Rule execution target for logging.
 */
struct ib_rule_log_tgt_t {
    const ib_rule_target_t *target;      /**< Target of rule */
    const ib_field_t       *original;    /**< Original value */
    const ib_field_t       *transformed; /**< Transformed value */
    ib_list_t              *tfn_list;    /**< List of transformations */
    int                     num_tfn;     /**< # of transformations */
    ib_list_t              *rslt_list;   /**< List of value/result objects */
    int                     num_results; /**< # of results */
    ib_rule_log_exec_t     *log_exec;    /**< Parent execution log object */
    ib_rule_log_count_t     counts;      /**< Result counting info */
};
typedef struct ib_rule_log_tgt_t ib_rule_log_tgt_t;

/**
 * Rule execution logging data
 */
struct ib_rule_log_exec_t {
    ib_timeval_t            start_time;  /**< Time of start of rule execution */
    ib_timeval_t            end_time;    /**< Time of end of rule execution */
    ib_flags_t              enable;      /**< Enable flags */
    ib_rule_log_tx_t       *tx_log;      /**< Rule transaction log */
    const ib_rule_t        *rule;        /**< Rule being executed */
    ib_list_t              *tgt_list;    /**< List of ib_rule_tgt_result_t */
    int                     num_tgt;     /**< # of targets */
    ib_rule_log_count_t     counts;      /**< Result counting info */
};

/**
 * Rule transaction logging data
 */
struct ib_rule_log_tx_t {
    ib_mpool_t             *mp;          /**< Memory pool for allocations */
    ib_timeval_t            start_time;  /**< Time of start of rule engine */
    ib_timeval_t            end_time;    /**< Time of end of rule engine */
    ib_flags_t              flags;       /**< Rule logging flags */
    ib_rule_log_mode_t      mode;        /**< Rule logging mode */
    ib_log_level_t          level;       /**< Level to log at */
    ib_rule_phase_num_t     last_phase;  /**< Phase of phase log */
};

/**
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
 * Initialize a context the rule engine.
 *
 * Called when a context is initialized; performs rule engine initialization.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 * @param[in,out] ctx IronBee context
 */
ib_status_t ib_rule_engine_ctx_init(ib_engine_t *ib,
                                    ib_module_t *mod,
                                    ib_context_t *ctx);

/**
 * Close a context for the rule engine.
 *
 * Called when a context is closed; performs rule engine rule fixups.
 *
 * @param[in,out] ib IronBee object
 * @param[in] mod Module object
 * @param[in,out] ctx IronBee context
 */
ib_status_t ib_rule_engine_ctx_close(ib_engine_t *ib,
                                     ib_module_t *mod,
                                     ib_context_t *ctx);


/**
 * Return rule execution logging flags
 *
 * @param[in] ctx The context that we're looking the level up for
 *
 * @return The configured rule log execution flags.
 */
ib_flags_t ib_rule_log_flags(ib_context_t *ctx);

/**
 * Return the configured rule logging level.
 *
 * This is used to determine if optional complex processing should be
 * performed to log possibly option information.
 *
 * @param[in] ctx The context that we're looking the level up for
 *
 * @return The log level configured.
 */
ib_log_level_t ib_rule_log_level(ib_context_t *ctx);

/**
 * Return the configured rule debug logging level.
 *
 * This is used to determine if optional complex processing should be
 * performed to log possibly option information.
 *
 * @param[in] ctx The context that we're looking the level up for
 *
 * @return The log level configured.
 */
ib_rule_dlog_level_t ib_rule_dlog_level(ib_context_t *ctx);

/**
 * Dump the enabled rule log flags
 *
 * @param[in] ib The IronBee engine
 * @param[in] ctx The context that we're looking the level up for
 *
 * @return void
 */
void ib_rule_log_flags_dump(ib_engine_t *ib,
                            ib_context_t *ctx);

/**
 * Create a rule transaction logging object
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] tx_log The new rule transaction log object
 *
 * @returns IB_OK on success,
 *          IB_EALLOC if the allocation failed,
 *          Error status returned by ib_mpool_calloc()
 */
ib_status_t ib_rule_log_tx_create(const ib_rule_exec_t *rule_exec,
                                  ib_rule_log_tx_t **tx_log);

/**
 * Create a rule execution logging object
 *
 * @param[in] rule_exec Rule execution object
 * @param[out] exec_log The new execution logging object
 *
 * @returns IB_OK on success,
 *          IB_EALLOC if the allocation failed,
 *          Error status returned by ib_mpool_calloc()
 *          Error status returned by ib_list_create()
 */
ib_status_t ib_rule_log_exec_create(const ib_rule_exec_t *rule_exec,
                                    ib_rule_log_exec_t **exec_log);

/**
 * Log transaction events for the rule logger (start of phase)
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] event The transaction event to log (as required)
 *
 * @returns void
 */
void ib_rule_log_tx_event_start(const ib_rule_exec_t *rule_exec,
                                ib_state_event_type_t event);

/**
 * Log transaction events for the rule logger (end of phase)
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] event The transaction event to log (as required)
 *
 * @returns void
 */
void ib_rule_log_tx_event_end(const ib_rule_exec_t *rule_exec,
                              ib_state_event_type_t event);

/**
 * Log start of phase
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] phase_num Phase number
 * @param[in] phase_name Name of phase
 *
 * @returns void
 */
void ib_rule_log_phase(const ib_rule_exec_t *rule_exec,
                       ib_rule_phase_num_t phase_num,
                       const char *phase_name);

/**
 * Add a target result to a rule execution log
 *
 * @param[in,out] log_exec The execution logging object
 * @param[in] target Rule target
 * @param[in] value Target before transformations
 *
 * @returns IB_OK on success,
 *          IB_EALLOC if an allocation failed
 *          Error status returned by ib_list_push()
 */
ib_status_t ib_rule_log_exec_add_target(ib_rule_log_exec_t *log_exec,
                                        const ib_rule_target_t *target,
                                        const ib_field_t *value);

/**
 * Add a result to a rule execution logging object
 *
 * @param[in,out] log_exec The new execution logging object
 * @param[in] value The value passed to the operator
 * @param[in] result Execution result
 * @param[in] status Status returned by the operator
 *
 * @returns IB_OK on success,
 *          IB_EALLOC if an allocation failed
 *          Error status returned by ib_list_push()
 */
ib_status_t ib_rule_log_exec_add_result(ib_rule_log_exec_t *log_exec,
                                        const ib_field_t *value,
                                        ib_num_t result,
                                        ib_status_t status);

/**
 * Add an action to a rule execution logging object
 *
 * @param[in,out] exec_log The new execution logging object
 * @param[in] act_inst The action instance to log
 * @param[in] status Status returned by the action
 *
 * @returns IB_OK on success,
 *          IB_EALLOC if an allocation failed
 *          Error status returned by ib_list_push()
 */
ib_status_t ib_rule_log_exec_add_action(ib_rule_log_exec_t *exec_log,
                                        const ib_action_inst_t *act_inst,
                                        ib_status_t status);

/**
 * Add an event to a rule execution logging object
 *
 * @param[in,out] exec_log The rule execution log object
 * @param[in] event The event to log
 *
 * @returns IB_OK on success,
 *          IB_EALLOC if an allocation failed
 *          Error status returned by ib_list_push()
 */
ib_status_t ib_rule_log_exec_add_event(ib_rule_log_exec_t *exec_log,
                                       const ib_logevent_t *event);

/**
 * Set the current target's final value (after all transformations)
 *
 * @param[in,out] exec_log Rule execution log object
 * @param[in] final Target after all transformations
 *
 * @returns IB_OK on success,
 */
ib_status_t ib_rule_log_exec_set_tgt_final(ib_rule_log_exec_t *exec_log,
                                           const ib_field_t *final);

/**
 * Add a stream target result to a rule execution log
 *
 * @param[in,out] exec_log The execution logging object
 * @param[in] field Value passed to the operator
 *
 * @returns IB_OK on success
 */
ib_status_t ib_rule_log_exec_add_stream_tgt(ib_rule_log_exec_t *exec_log,
                                            const ib_field_t *field);

/**
 * Add a transformation to a rule execution log
 *
 * @param[in,out] exec_log The execution logging object
 * @param[in] tfn The transformation to add
 * @param[in] in Value before transformation
 * @param[in] out Value after transformation
 * @param[in] status Status returned by the transformation
 *
 * @returns IB_OK on success
 */
ib_status_t ib_rule_log_exec_add_tfn(ib_rule_log_exec_t *exec_log,
                                     const ib_tfn_t *tfn,
                                     const ib_field_t *in,
                                     const ib_field_t *out,
                                     ib_status_t status);

/**
 * Add an event to a rule execution log
 *
 * @param[in,out] exec_log The execution logging object
 * @param[in] event The event to add
 *
 * @returns IB_OK on success
 */
ib_status_t ib_rule_log_exec_add_event(ib_rule_log_exec_t *exec_log,
                                       const ib_logevent_t *event);


#endif /* IB_RULE_ENGINE_PRIVATE_H_ */
