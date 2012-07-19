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

#ifndef _IB_RULE_ENGINE_PRIVATE_H_
#define _IB_RULE_ENGINE_PRIVATE_H_

/**
 * @file
 * @brief IronBee &mdash; Rule Engine Private Declarations
 *
 * These definitions and routines are called by core and nowhere else.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/types.h>

/**
 * Rule transformation results for logging.
 */
struct ib_rule_log_tfn_t {
    const ib_tfn_t         *tfn;         /**< Transformation */
    const ib_field_t       *in;          /**< Value before transformation */
    const ib_field_t       *out;         /**< Value after transformation */
};
typedef struct ib_rule_log_tfn_t ib_rule_log_tfn_t;

/**
 * Rule result for logging.
 */
struct ib_rule_log_rslt_t {
    const ib_field_t       *value;       /**< Value passed to operator */
    ib_num_t                result;      /**< Result of operator */
    const ib_list_t        *act_list;    /**< List of executed actions */
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
    ib_list_t              *rslt_list;   /**< List of value/result objects */
    ib_rule_log_exec_t     *log_exec;    /**< Parent execution log object */
};
typedef struct ib_rule_log_tgt_t ib_rule_log_tgt_t;

/**
 * Rule execution logging data
 */
struct ib_rule_log_exec_t {
    ib_rule_log_mode_t  mode;          /**< Logging mode */
    ib_flags_t          flags;         /**< Logging flags */
    ib_tx_t            *tx;            /**< Transformation */
    const ib_rule_t    *rule;          /**< Rule being executed */
    ib_num_t            result;        /**< Final result */
    ib_list_t          *tgt_list;      /**< List of ib_rule_tgt_result_t */
    ib_rule_log_tgt_t  *tgt_cur;       /**< Current target */
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
 * Return rule execution logging mode string
 *
 * @param[in] mode The mode to convert to string
 *
 * @return The string form of @a mode
 */
const char *ib_rule_log_mode_str(ib_rule_log_mode_t mode);

/**
 * Return rule execution logging mode
 *
 * @param[in] ib The IronBee engine that would be used in a call to ib_log_ex.
 *
 * @return The configured rule log mode
 */
ib_rule_log_mode_t ib_rule_log_mode(const ib_engine_t *ib);

/**
 * Return rule execution logging flags
 *
 * @param[in] ib The IronBee engine that would be used in a call to ib_log_ex.
 *
 * @return The configured rule log execution flags.
 */
ib_flags_t ib_rule_log_flags(const ib_engine_t *ib);

/**
 * Return the configured rule logging level.
 *
 * This is used to determine if optional complex processing should be
 * performed to log possibly option information.
 *
 * @param[in] ib The IronBee engine that would be used in a call to ib_log_ex.
 *
 * @return The log level configured.
 */
ib_rule_log_level_t ib_rule_log_level(const ib_engine_t *ib);

/**
 * Create a rule execution logging object
 *
 * @param[in] tx The IronBee transaction
 * @param[in] rule Rule being executed
 * @param[out] log_exec The new execution logging object
 *
 * @returns IB_OK on success,
 *          IB_EALLOC if the allocation failed,
 *          Error status returned by ib_list_create()
 */
ib_status_t ib_rule_log_exec_create(ib_tx_t *tx,
                                    const ib_rule_t *rule,
                                    ib_rule_log_exec_t **log_exec);

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
ib_status_t ib_rule_log_exec_add_tgt(ib_rule_log_exec_t *log_exec,
                                     const ib_rule_target_t *target,
                                     const ib_field_t *value);

/**
 * Add a result to a rule execution logging object
 *
 * @param[in,out] log_exec The new execution logging object
 * @param[in] value The value passed to the operator
 * @param[in] result Execution result
 * @param[in] actions List of executed actions
 *
 * @returns IB_OK on success,
 *          IB_EALLOC if an allocation failed
 *          Error status returned by ib_list_push()
 */
ib_status_t ib_rule_log_exec_add_result(ib_rule_log_exec_t *log_exec,
                                        const ib_field_t *value,
                                        ib_num_t result,
                                        const ib_list_t *actions);

/**
 * Set the current target's final value (after all transformations)
 *
 * @param[in,out] log_exec The execution logging object
 * @param[in] final Target after all transformations
 *
 * @returns IB_OK on success,
 */
ib_status_t ib_rule_log_exec_set_tgt_final(ib_rule_log_exec_t *log_exec,
                                           const ib_field_t *final);

/**
 * Add a stream target result to a rule execution log
 *
 * @param[in,out] log_exec The execution logging object
 * @param[in] field Value passed to the operator
 *
 * @returns IB_OK on success
 */
ib_status_t ib_rule_log_exec_add_stream_tgt(ib_rule_log_exec_t *log_exec,
                                            const ib_field_t *field);

/**
 * Add a transformation to a rule execution log
 *
 * @param[in,out] log_exec The execution logging object
 * @param[in] tfn The transformation to add
 * @param[in] in Value before transformation
 * @param[in] out Value after transformation
 *
 * @returns IB_OK on success
 */
ib_status_t ib_rule_log_exec_add_tfn(ib_rule_log_exec_t *log_exec,
                                     const ib_tfn_t *tfn,
                                     const ib_field_t *in,
                                     const ib_field_t *out);

/**
 * Log a field's value
 *
 * @param[in] tx Transaction
 * @param[in] rule Rule to log (or NULL)
 * @param[in] target Rule target (or NULL)
 * @param[in] tfn Transformation (or NULL)
 * @param[in] label Label string
 * @param[in] f Field
 */
void ib_rule_log_field(const ib_tx_t *tx,
                       const ib_rule_t *rule,
                       const ib_rule_target_t *target,
                       const ib_tfn_t *tfn,
                       const char *label,
                       const ib_field_t *f);



#endif /* IB_RULE_ENGINE_PRIVATE_H_ */
