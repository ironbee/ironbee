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

#ifndef _IB_RULE_ENGINE_H_
#define _IB_RULE_ENGINE_H_

/**
 * @file
 * @brief IronBee --- Rule engine definitions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/action.h>
#include <ironbee/build.h>
#include <ironbee/config.h>
#include <ironbee/operator.h>
#include <ironbee/rule_defs.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
* @defgroup IronBeeRule Rule Engine
* @ingroup IronBeeEngine
*
* The rule engine supports writing rules that trigger on certain inputs
* and execute actions as a result.
*
* @{
*/


/**
 * Rule meta data
 */
typedef struct {
    size_t                 index;           /**< Index */
    const char            *id;              /**< Rule ID */
    const char            *full_id;         /**< Rule's "full" ID */
    const char            *chain_id;        /**< Rule's chain ID */
    ib_var_expand_t       *msg;             /**< Rule message */
    ib_var_expand_t       *data;            /**< Rule logdata */
    ib_list_t             *tags;            /**< Rule tags */
    ib_rule_phase_num_t    phase;           /**< Phase number */
    uint8_t                severity;        /**< Rule severity */
    uint8_t                confidence;      /**< Rule confidence */
    uint16_t               revision;        /**< Rule revision # */
    const char            *config_file;     /**< File rule defined in */
    unsigned int           config_line;     /**< Line number of rule def */
} ib_rule_meta_t;

/**
 * Rule phase meta data
 */
typedef struct ib_rule_phase_meta_t ib_rule_phase_meta_t;

/**
 * Rule instance object.
 */
typedef struct ib_rule_operator_inst_t ib_rule_operator_inst_t;

/**
 * Basic rule object.
 */
struct ib_rule_t {
    ib_rule_meta_t         meta;            /**< Rule meta data */
    const ib_rule_phase_meta_t *phase_meta; /**< Phase meta data */
    ib_rule_operator_inst_t *opinst;          /**< Rule operator */
    ib_list_t             *target_fields;   /**< List of targets */
    ib_list_t             *true_actions;    /**< Actions if condition True */
    ib_list_t             *false_actions;   /**< Actions if condition False */
    ib_list_t             *aux_actions;     /**< Auxiliary actions */
    ib_list_t             *parent_rlist;    /**< Parent rule list */
    ib_context_t          *ctx;             /**< Parent context */
    ib_rule_t             *chained_rule;    /**< Next rule in the chain */
    ib_rule_t             *chained_from;    /**< Ptr to rule chained from */
    const char            *capture_collection; /**< Capture collection name */
    ib_flags_t             flags;           /**< External, etc. */
};

/**
 * Callback to produce an error page.
 *
 * @param[in] tx The transaction for which the error page should be
 *            generated.
 * @param[out] body The page body. If the memory segment holding the
 *             body must be allocated, it is recommended that it
 *             should be done so out of the tx's memory pool.
 *             Regardless, whatever memory is used, it must
 *             last until the error page is served by IronBee
 *             to the server plugin.
 * @param[out] length The length of the body.
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - IB_DECLINED If the default page should be used.
 * - Other on error. The default page will be used.
 */
typedef ib_status_t (*ib_rule_error_page_fn_t)(
    ib_tx_t        *tx,
    const uint8_t **body,
    size_t         *length,
    void           *cbdata);

/**
 * Rule engine parser data
 */
typedef struct {
    ib_rule_t             *previous;     /**< Previous rule parsed */
} ib_rule_parser_data_t;

/**
 * Rule trace data
 */
typedef struct {
    /* This can go away if we ever have a rule_by_index index. */
    /**
     * Rule being traced.
     **/
    const ib_rule_t *rule;

    /**
     * Evaluation time (microseconds).
     *
     * This is the amount of time spent evaluating this rule.  The time spent
     * in chained rules is counted in those rules.  If this rule is evaluated
     * multiple times in a single transaction, the time will be the total over
     * all runs.  See evaluation_n.
     **/
    ib_time_t        evaluation_time; /**< Microseconds to evaluate */

    /**
     * Number of times evaluated.
     **/
    size_t evaluation_n;
} ib_rule_trace_t;

/**
 * Rule execution data
 */
struct ib_rule_exec_t {
    ib_engine_t            *ib;          /**< The IronBee engine */
    ib_tx_t                *tx;          /**< The executing transaction */
    ib_rule_phase_num_t     phase;       /**< The phase being executed */
    bool                    is_stream;   /**< Is this a stream rule phase? */
    ib_rule_t              *rule;        /**< The currently executing rule */
    ib_rule_target_t       *target;      /**< The current rule target */
    ib_status_t             rule_status; /**< Rule execution status */
    ib_num_t                rule_result; /**< Rule execution result */

    /* Data on the current execution frame (current target) */
    ib_status_t             cur_status;  /**< Current execution status. */
    ib_num_t                cur_result;  /**< Current execution result. */
    const ib_field_t       *cur_value;   /**< Current value */

    /* Logging objects */
    ib_rule_log_tx_t       *tx_log;      /**< Rule TX logging object */
    ib_rule_log_exec_t     *exec_log;    /**< Rule execution logging object */

    /* The below members are for rule engine internal use only, and should
     * never be accessed by actions, injection functions, etc. */

    /* Rule stack (for chains) */
    ib_list_t              *rule_stack;  /**< Stack of rules */

    /* List of all rules to run during the current phase. */
    ib_list_t              *phase_rules; /**< List of ib_rule_t */

    /**
     * Stack of @ref ib_field_t used for creating FIELD* targets
     */
    ib_list_t              *value_stack;

#ifdef IB_RULE_TRACE
    ib_rule_trace_t        *traces; /**< Rule trace information. */
#endif
};

/**
 * External rule driver function.
 *
 * Function is passed configuration parser, tag, location, and callback data.
 *
 * @param[in] cp Configuration parser
 * @param[in] rule Rule being processed
 * @param[in] tag Driver tag
 * @param[in] location Location of rule file
 * @param[in] cbdata Generic callback data
 */
typedef ib_status_t (*ib_rule_driver_fn_t)(
    ib_cfgparser_t             *cp,
    ib_rule_t                  *rule,
    const char                 *tag,
    const char                 *location,
    void                       *cbdata
);

/**
 * A driver is simply a function and its callback data.
 */
typedef struct {
    ib_rule_driver_fn_t     function;    /**< Driver function */
    void                   *cbdata;      /**< Driver callback data */
} ib_rule_driver_t;

/**
 * External rule ownership function, invoked during close of context.
 *
 * This function will be called during the rule selection process.  This can,
 * by returning IB_OK, inform the rule engine that the function is taking
 * ownership of @a rule, and that the rule engine should not schedule @a rule
 * to run.  Typically, a module will schedule @a rule, or one or more rules
 * in its stead, via the injection function (ib_rule_injection_fn_t and
 * ib_rule_register_injection_fn).
 *
 * This function may be called multiple times for a given rule: once for
 * every context the rule is enabled in.
 *
 * @param[in] ib IronBee engine
 * @param[in] rule Rule being registered
 * @param[in] ctx The context rule is enabled in.
 * @param[in] cbdata Registration function callback data
 *
 * @returns Status code:
 *   - IB_OK All OK, rule managed externally by module.
 *   - IB_DECLINE Decline to manage rule.
 *   - IB_Exx Other error.
 */
typedef ib_status_t (* ib_rule_ownership_fn_t)(
    const ib_engine_t  *ib,
    const ib_rule_t    *rule,
    const ib_context_t *ctx,
    void               *cbdata
);

/**
 * External rule injection function
 *
 * This function will be called at the start of each phase.  This gives a
 * module the opportunity to inject one or more rules into the start of the
 * phase.  It does this by appending rules, in the form of ib_rule_t pointers,
 * to @a rule_list.  @a rule_list may contain rules upon entry to this
 * function and should thus treat @a rule_list as append-only.
 *
 * @note Returning an error will cause the rule engine to abort the current
 * phase processing.
 *
 * @param[in] ib IronBee engine
 * @param[in] rule_exec Rule execution environment
 * @param[in,out] rule_list List of rules to execute (append-only)
 * @param[in] cbdata Injection function callback data
 *
 * @returns Status code:
 *   - IB_OK All OK
 *   - IB_Exx Other error
 */
typedef ib_status_t (* ib_rule_injection_fn_t)(
    const ib_engine_t    *ib,
    const ib_rule_exec_t *rule_exec,
    ib_list_t            *rule_list,
    void                 *cbdata
);

/**
 * @defgroup IronBeeRuleHooks Rule Engine Hooks
 * @ingroup IronBeeRule
 *
 * Rule engine hooks.
 *
 * These hooks provide for fine grained introspection into rule engine
 * activities.
 *
 * @{
 */

/**
 * Called before each rule.
 *
 * @param[in] rule_exec Rule execution environment.
 * @param[in] cbdata Callback data.
 */
typedef void (* ib_rule_pre_rule_fn_t)(
    const ib_rule_exec_t *rule_exec,
    void                 *cbdata
);

/**
 * Called after each rule.
 *
 * @param[in] rule_exec Rule execution environment.
 * @param[in] cbdata Callback data.
 */
typedef void (* ib_rule_post_rule_fn_t)(
    const ib_rule_exec_t *rule_exec,
    void                 *cbdata
);

/**
 * Called before each operator.
 *
 * @param[in] rule_exec Rule execution environment.
 * @param[in] opinst Operator instance to be executed.
 * @param[in] instance_data Instance data of @a op.
 * @param[in] invert True iff this operator is inverted.
 * @param[in] value Input to operator.
 * @param[in] cbdata Callback data.
 */
typedef void (* ib_rule_pre_operator_fn_t)(
    const ib_rule_exec_t     *rule_exec,
    const ib_operator_inst_t *opinst,
    bool                      invert,
    const ib_field_t         *value,
    void                     *cbdata
);

/**
 * Called after each operator.
 *
 * @param[in] rule_exec Rule execution environment.
 * @param[in] opinst Operator instance to be executed.
 * @param[in] invert True iff this operator is inverted.
 * @param[in] value Input to operator.
 * @param[in] op_rc Result code of operator execution.
 * @param[in] result Result of operator.
 * @param[in] capture Capture collection of operator.
 * @param[in] cbdata Callback data.
 */
typedef void (* ib_rule_post_operator_fn_t)(
    const ib_rule_exec_t     *rule_exec,
    const ib_operator_inst_t *opinst,
    bool                      invert,
    const ib_field_t         *value,
    ib_status_t               op_rc,
    ib_num_t                  result,
    ib_field_t               *capture,
    void                     *cbdata
);

/**
 * Called before each action.
 *
 * @param[in] rule_exec Rule execution environment.
 * @param[in] action Action to be executed.
 * @param[in] result Result of operator.
 * @param[in] cbdata Callback data.
 */
typedef void (* ib_rule_pre_action_fn_t)(
    const ib_rule_exec_t   *rule_exec,
    const ib_action_inst_t *action,
    ib_num_t                result,
    void                   *cbdata
);

/**
 * Called after each action.
 *
 * @param[in] rule_exec Rule execution environment.
 * @param[in] action Action just executed.
 * @param[in] result Result of operator.
 * @param[in] act_rc Result code of action.
 * @param[in] cbdata Callback data.
 */
typedef void (* ib_rule_post_action_fn_t)(
    const ib_rule_exec_t   *rule_exec,
    const ib_action_inst_t *action,
    ib_num_t                result,
    ib_status_t             act_rc,
    void                   *cbdata
);

/**
 * Register a pre rule function.
 *
 * @sa ib_rule_pre_rule_fn_t
 *
 * @param[in] ib IronBee engine.
 * @param[in] fn Function to register.
 * @param[in] cbdata Callback data for @a fn.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_rule_register_pre_rule_fn(
    ib_engine_t           *ib,
    ib_rule_pre_rule_fn_t  fn,
    void                  *cbdata
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Register a post rule function.
 *
 * @sa ib_rule_post_rule_fn_t
 *
 * @param[in] ib IronBee engine.
 * @param[in] fn Function to register.
 * @param[in] cbdata Callback data for @a fn.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_rule_register_post_rule_fn(
    ib_engine_t            *ib,
    ib_rule_post_rule_fn_t  fn,
    void                   *cbdata
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Register a pre operator function.
 *
 * @sa ib_rule_pre_operator_fn_t
 *
 * @param[in] ib IronBee engine.
 * @param[in] fn Function to register.
 * @param[in] cbdata Callback data for @a fn.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_rule_register_pre_operator_fn(
    ib_engine_t               *ib,
    ib_rule_pre_operator_fn_t  fn,
    void                      *cbdata
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Register a post operator function.
 *
 * @sa ib_rule_post_operator_fn_t
 *
 * @param[in] ib IronBee engine.
 * @param[in] fn Function to register.
 * @param[in] cbdata Callback data for @a fn.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_rule_register_post_operator_fn(
    ib_engine_t                *ib,
    ib_rule_post_operator_fn_t  fn,
    void                       *cbdata
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Register a pre action function.
 *
 * @sa ib_rule_pre_action_fn_t
 *
 * @param[in] ib IronBee engine.
 * @param[in] fn Function to register.
 * @param[in] cbdata Callback data for @a fn.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_rule_register_pre_action_fn(
    ib_engine_t             *ib,
    ib_rule_pre_action_fn_t  fn,
    void                    *cbdata
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Register a post action function.
 *
 * @sa ib_rule_post_action_fn_t
 *
 * @param[in] ib IronBee engine.
 * @param[in] fn Function to register.
 * @param[in] cbdata Callback data for @a fn.
 * @return
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_rule_register_post_action_fn(
    ib_engine_t              *ib,
    ib_rule_post_action_fn_t  fn,
    void                     *cbdata
)
NONNULL_ATTRIBUTE(1, 2);

/** @} */

/**
 * Set a rule engine value (for configuration)
 *
 * @param[in] cp Configuration parser
 * @param[in] name Name of parameter
 * @param[in] value Value to set to
 *
 * @returns IB_OK / IB_EINVAL
 */
ib_status_t DLL_PUBLIC ib_rule_engine_set(
    ib_cfgparser_t             *cp,
    const char                 *name,
    const char                 *value);

/**
 * Replace the default (or current) error page function.
 *
 * @param[in] ib IronBee engine.
 * @param[in] error_page_fn The error function to use in this engine.
 * @param[in] error_page_cbdata Callback data for @a error_page_fn.
 */
void DLL_PUBLIC ib_rule_set_error_page_fn(
    ib_engine_t             *ib,
    ib_rule_error_page_fn_t  error_page_fn,
    void                    *error_page_cbdata
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Register external rule driver.
 *
 * @param ib       Engine.
 * @param tag      Driver tag; NUL terminated.
 * @param driver   Driver function.
 * @param cbdata   Driver callback data.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_register_external_driver(
    ib_engine_t                *ib,
    const char                 *tag,
    ib_rule_driver_fn_t         driver,
    void                       *cbdata);

/**
 * Lookup an external rule driver
 *
 * @param[in] ib IronBee engine
 * @param[in] tag Driver tag
 * @param[out] pdriver Pointer to driver
 *
 * @returns Status code
 *    - IB_OK All ok
 *    - Errors from @sa ib_hash_get()
 */
ib_status_t DLL_PUBLIC ib_rule_lookup_external_driver(
    const ib_engine_t          *ib,
    const char                 *tag,
    ib_rule_driver_t          **pdriver);

/**
 * Register a rule ownership function.
 *
 * @param[in] ib IronBee engine.
 * @param[in] name Logical name (for logging)
 * @param[in] ownership_fn Ownership hook function
 * @param[in] cbdata Registration hook callback data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_register_ownership_fn(
    ib_engine_t            *ib,
    const char             *name,
    ib_rule_ownership_fn_t  ownership_fn,
    void                   *cbdata);

/**
 * Register a rule injection function.
 *
 * @param[in] ib IronBee engine.
 * @param[in] name Logical name (for logging)
 * @param[in] phase Phase execution phase to hook into
 * @param[in] injection_fn Injection function
 * @param[in] cbdata Injection callback data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_register_injection_fn(
    ib_engine_t            *ib,
    const char             *name,
    ib_rule_phase_num_t     phase,
    ib_rule_injection_fn_t  injection_fn,
    void                   *cbdata);

/**
 * Create a rule.
 *
 * Allocates a rule for the rule engine, initializes it.
 *
 * @param[in] ib IronBee engine
 * @param[in] ctx Current IronBee context
 * @param[in] file Name of configuration file being parsed
 * @param[in] lineno Line number in configuration file
 * @param[in] is_stream true if this is an inspection rule else false
 * @param[out] prule Address which new rule is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_create(
    ib_engine_t                *ib,
    ib_context_t               *ctx,
    const char                 *file,
    unsigned int                lineno,
    bool                        is_stream,
    ib_rule_t                 **prule);

/**
 * Lookup rule by ID
 *
 * @param[in] ib IronBee Engine.
 * @param[in] ctx Context to look in (or NULL).
 * @param[in] id ID to match.
 * @param[out] rule Matching rule.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_lookup(
    ib_engine_t                *ib,
    ib_context_t               *ctx,
    const char                 *id,
    ib_rule_t                 **rule);

/**
 * Find rule matching a reference rule.
 *
 * @param[in] ib IronBee Engine.
 * @param[in] ctx Context to look in (or NULL).
 * @param[in] ref Reference rule.
 * @param[out] rule Matching rule.
 *
 * @returns Status code:
 *  - IB_ENOENT No matching rule found
 *  - IB_EBADVAL Matching rule has different phase
 *  - IB_OK Matching rule found, phase matches
 */
ib_status_t DLL_PUBLIC ib_rule_match(
    ib_engine_t                *ib,
    ib_context_t               *ctx,
    const ib_rule_t            *ref,
    ib_rule_t                 **rule);

/**
 * Add an enable All/ID/Tag to the enable list for the specified context
 *
 * @param[in] ib IronBee engine
 * @param[in] ctx IronBee context
 * @param[in] etype Enable type (ID/Tag)
 * @param[in] name String description of @a etype
 * @param[in] enable true:Enable, false:Disable
 * @param[in] file Configuration file name
 * @param[in] lineno Line number in @a file
 * @param[in] str String of the id/tag
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_enable(
    const ib_engine_t          *ib,
    ib_context_t               *ctx,
    ib_rule_enable_type_t       etype,
    const char                 *name,
    bool                        enable,
    const char                 *file,
    unsigned int                lineno,
    const char                 *str);

/**
 * Enable all rules for the specified context
 *
 * @param[in] ib IronBee engine
 * @param[in] ctx IronBee context
 * @param[in] file Configuration file name
 * @param[in] lineno Line number in @a file
 *
 * @returns Status code (IB_EINVAL for invalid ID, errors from ib_list_push())
 */
ib_status_t DLL_PUBLIC ib_rule_enable_all(
    const ib_engine_t          *ib,
    ib_context_t               *ctx,
    const char                 *file,
    unsigned int                lineno);

/**
 * Add an enable ID to the enable list for the specified context
 *
 * @param[in] ib IronBee engine
 * @param[in] ctx IronBee context
 * @param[in] file Configuration file name
 * @param[in] lineno Line number in @a file
 * @param[in] id String of the id
 *
 * @returns Status code (IB_EINVAL for invalid ID, errors from ib_list_push())
 */
ib_status_t DLL_PUBLIC ib_rule_enable_id(
    const ib_engine_t          *ib,
    ib_context_t               *ctx,
    const char                 *file,
    unsigned int                lineno,
    const char                 *id);

/**
 * Add an enable tag to the enable list for the specified context
 *
 * @param[in] ib IronBee engine
 * @param[in] ctx IronBee context
 * @param[in] file Configuration file name
 * @param[in] lineno Line number in @a file
 * @param[in] tag String of the tag
 *
 * @returns Status code (IB_EINVAL for invalid ID, errors from ib_list_push())
 */
ib_status_t DLL_PUBLIC ib_rule_enable_tag(
    const ib_engine_t          *ib,
    ib_context_t               *ctx,
    const char                 *file,
    unsigned int                lineno,
    const char                 *tag);

/**
 * Disable all rules for the specified context
 *
 * @param[in] ib IronBee engine
 * @param[in] ctx IronBee context
 * @param[in] file Configuration file name
 * @param[in] lineno Line number in @a file
 *
 * @returns Status code (IB_EINVAL for invalid ID, errors from ib_list_push())
 */
ib_status_t DLL_PUBLIC ib_rule_disable_all(
    const ib_engine_t          *ib,
    ib_context_t               *ctx,
    const char                 *file,
    unsigned int                lineno);

/**
 * Add an ID to the disable list for the specified context
 *
 * @param[in] ib IronBee engine
 * @param[in] ctx IronBee context
 * @param[in] file Configuration file name
 * @param[in] lineno Line number in @a file
 * @param[in] id String of the id
 *
 * @returns Status code (IB_EINVAL for invalid ID, errors from ib_list_push())
 */
ib_status_t DLL_PUBLIC ib_rule_disable_id(
    const ib_engine_t          *ib,
    ib_context_t               *ctx,
    const char                 *file,
    unsigned int                lineno,
    const char                 *id);

/**
 * Add a tag to the disable list for the specified context
 *
 * @param[in] ib IronBee engine
 * @param[in] ctx IronBee context
 * @param[in] file Configuration file name
 * @param[in] lineno Line number in @a file
 * @param[in] tag String of the tag
 *
 * @returns Status code (IB_EINVAL for invalid ID, errors from ib_list_push())
 */
ib_status_t DLL_PUBLIC ib_rule_disable_tag(
    const ib_engine_t          *ib,
    ib_context_t               *ctx,
    const char                 *file,
    unsigned int                lineno,
    const char                 *tag);

/**
 * Set the execution phase of a rule (for phase rules).
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] phase Rule execution phase
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_set_phase(
    ib_engine_t                *ib,
    ib_rule_t                  *rule,
    ib_rule_phase_num_t         phase);

/**
 * Set whether the rule to invert its result.
 *
 * @param[in,out] rule Rule to operate on.
 * @param[in] invert True if rule should invert.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_set_invert(ib_rule_t *rule, bool invert);

/**
 * Set the parameters in this rule so that they may be used for logging.
 *
 * @param[in] rule Rule to operate on.
 * @param[in] params The string constant to be copied.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on a copy error.
 */
ib_status_t DLL_PUBLIC ib_rule_set_op_params(ib_rule_t *rule, const char *params);

/**
 * A utility function that converts a string to the appropriate phase number.
 *
 * This is used when developers are trying to create and register a new rule
 * during configuration time.
 *
 * @param[in] phase Phase string
 * @param[in] is_stream true if the phase is a stream phase, false if not.
 * @return
 *   - PHASE_INVALID when an error occurs.
 *   - The appropriate phase number for the named phase if the given
 *     stream value is appropriate.
 */
ib_rule_phase_num_t DLL_PUBLIC ib_rule_lookup_phase(
    const char *phase,
    bool        is_stream);

/**
 * Get the name associated with a phase number
 *
 * @param[in] phase Phase number
 *
 * @returns Phase name string (or NULL if @a phase is invalid)
 */
const char DLL_PUBLIC *ib_rule_phase_name(
    ib_rule_phase_num_t         phase);

/**
 * Query as to whether a rule allow transformations
 *
 * @param[in,out] rule Rule to query
 *
 * @returns true or false
 */
bool DLL_PUBLIC ib_rule_allow_tfns(
    const ib_rule_t            *rule);

/**
 * Query as to whether a rule allow chains
 *
 * @param[in,out] rule Rule to query
 *
 * @returns true or false
 */
bool DLL_PUBLIC ib_rule_allow_chain(
    const ib_rule_t            *rule);

/**
 * Query as to whether is a stream inspection rule
 *
 * @param[in,out] rule Rule to query
 *
 * @returns true or false
 */
bool DLL_PUBLIC ib_rule_is_stream(
    const ib_rule_t            *rule);

/**
 * Get the operator flags required for this rule.
 *
 * @param[in] rule Rule to get flags for.
 *
 * @returns Required operator flags
 */
ib_flags_t DLL_PUBLIC ib_rule_required_op_flags(
    const ib_rule_t            *rule);

/**
 * Set a rule's operator.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] opinst Operator instance.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_set_operator(
    ib_engine_t   *ib,
    ib_rule_t     *rule,
    const ib_operator_inst_t *opinst);

/**
 * Set a rule's ID.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] id Rule ID
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_set_id(
    ib_engine_t                *ib,
    ib_rule_t                  *rule,
    const char                 *id);

/**
 * Set a rule's chain flag
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_set_chain(
    ib_engine_t                *ib,
    ib_rule_t                  *rule);

/**
 * Get a rule's ID string.
 *
 * If @a rule is a chain rule, then the @c chain_id is returned.
 *
 * If @a rule has neither an id nor a @c chain_id NULL is returned
 * to allow the calling program to report the error or assign an id to
 * @a rule.
 *
 * @param[in] rule Rule to operate on
 *
 * @returns The rule's @c id, @c chain_id if @c id is not set, or NULL
 *          if neither is set.
 */
const char DLL_PUBLIC *ib_rule_id(
    const ib_rule_t            *rule);

/**
 * Check for a match against a rule's ID
 *
 * @param[in] rule Rule to match
 * @param[in] id ID to attempt to match against
 * @param[in] parents Check parent rules (in case of chains)?
 * @param[in] children Check child rules (in case of chains)?
 *
 * @returns true if match is found, false if not
 */
bool DLL_PUBLIC ib_rule_id_match(
    const ib_rule_t            *rule,
    const char                 *id,
    bool                        parents,
    bool                        children);

/**
 * Check for a match against a rule's tags
 *
 * @param[in] rule Rule to match
 * @param[in] tag Tag to attempt to match against
 * @param[in] parents Check parent rules (in case of chains)?
 * @param[in] children Check child rules (in case of chains)?
 *
 * @returns true if match is found, false if not
 */
bool DLL_PUBLIC ib_rule_tag_match(
    const ib_rule_t            *rule,
    const char                 *tag,
    bool                        parents,
    bool                        children);

/**
 * Create a rule target.
 *
 * @param[in] ib IronBee engine.
 * @param[in] str Target string.
 * @param[in] tfns List of @ref ib_transformation_inst_t.
 * @param[in,out] target Pointer to new target.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 * - Other on other failures.
 */
ib_status_t DLL_PUBLIC ib_rule_create_target(
    ib_engine_t                *ib,
    const char                 *str,
    ib_list_t                  *tfns,
    ib_rule_target_t          **target);

/**
 * Add a target field to a rule.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] target Target object to add
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_add_target(
    ib_engine_t                *ib,
    ib_rule_t                  *rule,
    ib_rule_target_t           *target);

/**
 * Add a transformation to all target fields of a rule.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] name Name of the transformation to add.
 * @param[in] arg Argument to the transformation.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 * - IB_ENOENT If transformation is not found.
 * - Other if an error occurs.
 */
ib_status_t DLL_PUBLIC ib_rule_add_tfn(
    ib_engine_t                *ib,
    ib_rule_t                  *rule,
    const char                 *name,
    const char                 *arg);

/**
 * Add an transformation to a target field.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] target Target field
 * @param[in] name Transformation name
 * @param[in] arg Argument to the transformation.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 * - IB_ENOENT If transformation is not found.
 * - Other if an error occurs.
 */
ib_status_t DLL_PUBLIC ib_rule_target_add_tfn(
    ib_engine_t                *ib,
    ib_rule_target_t           *target,
    const char                 *name,
    const char                 *arg);

/**
 * Add a modifier to a rule.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] str Modifier string
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_add_modifier(
    ib_engine_t                *ib,
    ib_rule_t                  *rule,
    const char                 *str);

/**
 * Add an action modifier to a rule.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule Rule to operate on
 * @param[in] action Action instance to add
 * @param[in] which Which action list to add to
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_add_action(
    ib_engine_t                *ib,
    ib_rule_t                  *rule,
    ib_action_inst_t           *action,
    ib_rule_action_t            which);


/**
 * Check a rule (action) parameters
 *
 * @param[in] ib IronBee engine
 * @param[in] rule Rule to operate on
 * @param[in] params Parameters to check
 *
 * @returns Status code
 *   - IB_OK
 */
ib_status_t ib_rule_check_params(ib_engine_t *ib,
                                 ib_rule_t *rule,
                                 const char *params);

/**
 * Map a list of transformation names and arguments to @ref ib_transformation_inst_t.
 *
 * @param[in] ib The engine containing known transformations.
 * @param[in] mm Memory manager.
 * @param[in] tfn_fields A list of @ref ib_field_t of type
 *            IB_FTYPE_NULSTR. The name of the field is the name of the
 *            transformation. The value of the field is the argument
 *            to the transformation.
 * @param[out] tfn_insts A list that has @ref ib_transformation_inst_t elements
 *             pushed to it.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation error.
 * - IB_ENOENT If a transformation in @a tfn_fields cannot be found.
 * - Other on an unexpected error.
 */
ib_status_t DLL_PUBLIC ib_rule_tfn_fields_to_inst(ib_engine_t  *ib,
                                                  ib_mm_t      mm,
                                                  ib_list_t   *tfn_fields,
                                                  ib_list_t   *tfn_insts);

/**
 * Search for actions associated with a rule.
 *
 * @param[in] ib IronBee engine
 * @param[in] rule Rule to operate on
 * @param[in] which Which action list to search
 * @param[in] name Name of action to search for
 * @param[out] matches List of matching ib_action_inst_t pointers (or NULL)
 * @param[out] pcount Pointer to count of matches (or NULL)
 *
 * @returns Status code
 *   - IB_OK All ok
 *   - IB_EINVAL if both @a matches and @a pcount are NULL, or any of the
 *               other parameters are invalid
 */
ib_status_t DLL_PUBLIC ib_rule_search_action(const ib_engine_t *ib,
                                             const ib_rule_t *rule,
                                             ib_rule_action_t which,
                                             const char *name,
                                             ib_list_t *matches,
                                             size_t *pcount);

/**
 * Enable capture for a rule, and optionally set the capture collection
 *
 * @param[in] ib IronBee engine
 * @param[in,out] rule The rule to operate on
 * @param[in] capture_collection Name of the capture collection (or NULL)
 *
 * @returns Status code:
 *  - IB_EINVAL: Invalid input (@a ib or @a rule is NULL)
 *  - IB_ENOTIMPL: Capture not supported by @a rule's operator
 *  - IB_EALLOC: Allocation error
 */
ib_status_t DLL_PUBLIC ib_rule_set_capture(
    ib_engine_t *ib,
    ib_rule_t   *rule,
    const char  *capture_collection);

/**
 * Register a rule.
 *
 * Register a rule for the rule engine.
 *
 * @param[in] ib IronBee engine
 * @param[in,out] ctx Context in which to execute the rule
 * @param[in,out] rule Rule to register
 *
 * @returns
 * - IB_OK On success.
 * - IB_EEXIST If more than 1 external rule manager module claims ownership
 *   of a rule or if a rule is already defined by its ID and revision.
 * - IB_EALLOC On allocation errors.
 * - IB_EINVAL On if @a rule is not properly constructed.
 * - IB_EUNKNOWN If an external module tries to handle a rule
 *   and does not return IB_OK or IB_DECLINE.
 */
ib_status_t DLL_PUBLIC ib_rule_register(
    ib_engine_t                *ib,
    ib_context_t               *ctx,
    ib_rule_t                  *rule);

/**
 * Invalidate an entire rule chain
 *
 * @param[in] ib IronBee engine
 * @param[in,out] ctx Context in which to execute the rule
 * @param[in,out] rule Rule to invalidate
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_chain_invalidate(
    ib_engine_t                *ib,
    ib_context_t               *ctx,
    ib_rule_t                  *rule);

/**
 * Get the memory manager to use for rule allocations.
 *
 * @param[in] ib IronBee engine
 *
 * @returns Memory manager to use.
 */
ib_mm_t DLL_PUBLIC ib_rule_mm(ib_engine_t *ib);

/**
 * Perform logging of a rule's execution
 *
 * @param[in] rule_exec Rule execution object
 */
void DLL_PUBLIC ib_rule_log_execution(
    const ib_rule_exec_t       *rule_exec);

/**
 * Generic Logger for rule execution.
 *
 * This is intended to be used when a rule execution object is available.
 *
 * @warning There is currently a 1024 byte formatter limit when prefixing the
 *          log header data.
 *
 * @param[in] level Rule log level
 * @param[in] rule_exec Rule execution object (or NULL)
 * @param[in] file Filename (or NULL)
 * @param[in] func Function name (or NULL)
 * @param[in] line Line number (or 0)
 * @param[in] fmt Printf-like format string
 */
void DLL_PUBLIC ib_rule_log_exec(
    ib_rule_dlog_level_t        level,
    const ib_rule_exec_t       *rule_exec,
    const char                 *file,
    const char                 *func,
    int                         line,
    const char                 *fmt, ...)
    PRINTF_ATTRIBUTE(6, 7);

/**
 * Is @a rule the member of a chain and not the first rule in the chain?
 *
 * @param[in] rule
 *
 * @returns True of the rule has a preceding rule in a chain.
 */
bool DLL_PUBLIC ib_rule_is_chained(const ib_rule_t *rule) NONNULL_ATTRIBUTE(1);

/**
 * Is @a rule marked?
 *
 * @param[in] rule
 *
 * @returns True of the rule is marked.
 */
bool DLL_PUBLIC ib_rule_is_marked(const ib_rule_t *rule) NONNULL_ATTRIBUTE(1);

/**
 * Log a fatal rule execution error
 *
 * This will cause @sa ib_rule_log_execution() to @sa assert(), and thus
 * should be used only in development environments.
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] file Filename (or NULL)
 * @param[in] func Function name (or NULL)
 * @param[in] line Line number (or 0)
 * @param[in] fmt Printf-like format string
 */
void DLL_PUBLIC ib_rule_log_fatal_ex(
    const ib_rule_exec_t       *rule_exec,
    const char                 *file,
    const char                 *func,
    int                         line,
    const char                 *fmt, ...)
    PRINTF_ATTRIBUTE(5, 6);

/** Rule execution fatal error logging */
#define ib_rule_log_fatal(rule_exec, ...) \
    ib_rule_log_fatal_ex(rule_exec, __func__, __FILE__, __LINE__, __VA_ARGS__)

/** Rule execution error logging */
#define ib_rule_log_error(rule_exec, ...) \
    ib_rule_log_exec(IB_RULE_DLOG_ERROR, rule_exec, \
                     __FILE__, __func__, __LINE__, __VA_ARGS__)

/** Rule execution warning logging */
#define ib_rule_log_warn(rule_exec, ...) \
    ib_rule_log_exec(IB_RULE_DLOG_WARNING, rule_exec, \
                     __FILE__, __func__, __LINE__, __VA_ARGS__)

/** Rule execution notice logging */
#define ib_rule_log_notice(rule_exec, ...) \
    ib_rule_log_exec(IB_RULE_DLOG_NOTICE, rule_exec, \
                     __FILE__, __func__, __LINE__, __VA_ARGS__)

/** Rule execution info logging */
#define ib_rule_log_info(rule_exec, ...) \
    ib_rule_log_exec(IB_RULE_DLOG_INFO, rule_exec, \
                     __FILE__, __func__, __LINE__, __VA_ARGS__)

/** Rule execution debug logging */
#define ib_rule_log_debug(rule_exec, ...) \
    ib_rule_log_exec(IB_RULE_DLOG_DEBUG, rule_exec, \
                     __FILE__, __func__, __LINE__, __VA_ARGS__)

/** Rule execution trace logging */
#define ib_rule_log_trace(rule_exec, ...) \
    ib_rule_log_exec(IB_RULE_DLOG_TRACE, rule_exec, \
                     __FILE__, __func__, __LINE__, __VA_ARGS__)

/**
 * Generic Logger for with transaction
 *
 * This is intended to be used when no rule execution object is available.
 *
 * @warning There is currently a 1024 byte formatter limit when prefixing the
 *          log header data.
 *
 * @param[in] level Rule log level
 * @param[in] tx Transaction information
 * @param[in] file Filename (or NULL)
 * @param[in] func Function name (or NULL)
 * @param[in] line Line number (or 0)
 * @param[in] fmt Printf-like format string
 */
void ib_rule_log_tx(ib_rule_dlog_level_t level,
                    const ib_tx_t *tx,
                    const char *file,
                    const char *func,
                    int line,
                    const char *fmt, ...)
    PRINTF_ATTRIBUTE(6, 7);

/** Rule error logging (TX version) */
#define ib_rule_log_tx_error(tx, ...) \
    ib_rule_log_tx(IB_RULE_DLOG_ERROR, tx, \
                   __FILE__, __func__, __LINE__, __VA_ARGS__)

/** Rule warning logging (TX version) */
#define ib_rule_log_tx_warn(tx, ...) \
    ib_rule_log_tx(IB_RULE_DLOG_WARNING, tx, \
                   __FILE__, __func__, __LINE__, __VA_ARGS__)

/** Rule notice logging (TX version) */
#define ib_rule_log_tx_notice(tx, ...) \
    ib_rule_log_tx(IB_RULE_DLOG_NOTICE, tx, \
                   __FILE__, __func__, __LINE__, __VA_ARGS__)

/** Rule info logging (TX version) */
#define ib_rule_log_tx_info(tx, ...) \
    ib_rule_log_tx(IB_RULE_DLOG_INFO, tx, \
                   __FILE__, __func__, __LINE__, __VA_ARGS__)

/** Rule debug logging (TX version) */
#define ib_rule_log_tx_debug(tx, ...) \
    ib_rule_log_tx(IB_RULE_DLOG_DEBUG, tx, \
                   __FILE__, __func__, __LINE__, __VA_ARGS__)

/** Rule trace logging (TX version) */
#define ib_rule_log_tx_trace(tx, ...) \
    ib_rule_log_tx(IB_RULE_DLOG_TRACE, tx, \
                   __FILE__, __func__, __LINE__, __VA_ARGS__)

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_RULE_ENGINE_H_ */
