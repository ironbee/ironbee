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

/**
 * @file
 * @brief IronBee --- abort module
 *
 * This is a module that defines the `abort` and `abortIf`
 * modifiers.  These are useful primarily for development and testing
 * purposes.
 *
 * @note Abort actions can operate on operators (operator abort action) or
 * actions (action abort action), resulting in an overload of the word
 * "action".  Thus, to avoid confusion, the term "abort modifier" is used
 * instead of "abort action".
 *
 * For every rule with an `abort` modifier, executes after every
 * operator or action executes.  It always fires, regardless of result of the
 * operator, or the returned status code of the operator.
 *
 * For every rule with an `abortIf` modifier, executes after any
 * relevant operator or action executes.  It fires the result of it's operand
 * is True.
 *
 * The abortIf operands are:
 * - `OpOk`: Fires if an operator's status is `IB_OK`.
 * - `OpFail`: Fires if an operator's status not `IB_OK`.
 * - `OpTrue`: Fires if an operator's result is `True`.
 * - `OpFail`: Fires if an operator's result if `False`.
 * - `ActOk`: Fires if an action's status is `IB_OK`.
 * - `ActFail`: Fires if an action's status not `IB_OK`.
 *
 * Any time an abort / abortIf modifier fires, a "ABORT:" message is logged.
 *
 * At the end of any transaction in which at least one abort / abortIf
 * modified fires, summary "ABORT:" messages are logged.
 *
 * The abort mode is configured via the AbortMode directive.  The possible
 * values are:
 *
 * - `Immediate`: Invokes abort() immediately if any of the rule's
 *    abort / abortIf modifiers fire.  This is the default mode.
 *
 * - `TxEnd`: Invokes abort() at the end of a transaction if any of the
 *     abort / abortIf modifiers fired for any rule executed for the
 *     transaction.
 *
 * - `Off`: abort() is never invoked.
 *
 * Examples:
 * - `rule s @streq "x"   id:1 chain abortIf:OpTrue`
 * - `rule t @streq "abc" id:2 abort:Chain executed!`
 * - `rule x @streq "x"   id:3 abortIf:OpOk`
 * - `rule x @eq     1    id:4 "abortIf:OpFail:eq operator Failed!"`
 * - `rule y @exists x    id:5 abortIf:OpTrue`
 * - `rule z @is_int x    id:6 abortIf:OpFalse`
 * - `rule n @eq     1    id:7 setvar:x+=3 "abortIf:ActFail:setvar failed"`
 * - `rule n @eq     1    id:8 setvar:s+=3 "abortIf:ActOk:setvar didn't fail"`
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/action.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/hash.h>
#include <ironbee/mm.h>
#include <ironbee/module.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>

#include <assert.h>
#include <inttypes.h>
#include <strings.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        abort
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/**
 * Abort mode
 */
enum abort_mode_t {
    ABORT_IMMEDIATE,                /**< Immediate abort() */
    ABORT_TX_END,                   /**< abort() at end of transaction */
    ABORT_OFF                       /**< Don't abort(), just log loudly */
};
typedef enum abort_mode_t abort_mode_t;

/**
 * Abort module configuration
 */
struct abort_config_t {
    abort_mode_t abort_mode;        /**< Abort mode. */
};
typedef struct abort_config_t abort_config_t;

/**
 * Abort module global configuration
 */
static abort_config_t abort_config = {
    .abort_mode = ABORT_IMMEDIATE,
};

/**
 * Abort types
 */
enum abort_type_t {
    ABORT_ALWAYS,                   /**< Abort any time the abort fires */
    ABORT_OP_TRUE,                  /**< Abort if operation true */
    ABORT_OP_FALSE,                 /**< Abort if operation false */
    ABORT_OP_OK,                    /**< Abort if operator failed */
    ABORT_OP_FAIL,                  /**< Abort if operator succeeded */
    ABORT_ACT_OK,                   /**< Abort if all actions succeeded */
    ABORT_ACT_FAIL,                 /**< Abort if any actions failed */
};
typedef enum abort_type_t abort_type_t;

/**
 * Abort per-TX data.
 */
struct abort_tx_data_t {
    ib_list_t       *abort_list;    /**< List of rules that aborted */
};
typedef struct abort_tx_data_t abort_tx_data_t;

/*
 * See note above about "abort action" vs "abort modifier".
 */

/**
 * Data stored for each abort modifier.
 */
struct abort_modifier_t {
    abort_type_t     abort_type;    /**< Type of abort modifier */
    bool             is_false;      /**< Abort modifier inverted? */
    const char      *abort_str;     /**< String version of abort_type */
    ib_var_expand_t *message;       /**< Message */
};
typedef struct abort_modifier_t abort_modifier_t;

/**
 * Rule + associated abort modifiers
 */
struct abort_rule_t {
    const ib_rule_t *rule;            /**< The rule itself */
    ib_list_t       *abort_modifiers; /**< List of aborts (@ref abort_modifier_t) */
};
typedef struct abort_rule_t abort_rule_t;

/**
 * Abort module data
 */
struct abort_module_data_t {
    ib_hash_t *op_rules;  /**< Rules with operator aborts (@ref abort_rule_t) */
    ib_hash_t *act_rules; /**< Rules with action aborts (@ref abort_rule_t) */
};
typedef struct abort_module_data_t abort_module_data_t;

/**
 * Abort modifier filter function.
 *
 * The abort filters are called to filter an abort modifier to determine
 * whether to execute it or not.
 *
 * Currently, there are two filters; one which selects only operator aborts,
 * the other only action aborts.
 *
 * @param[in] modifier Abort modifier to filter
 *
 * @returns true if @a modifier matches, otherwise false
 */
typedef bool (* abort_filter_fn_t)(
    const abort_modifier_t *modifier
);

/**
 * Get the abort rule object associated with the @a rule (if it exists)
 *
 * @param[in] rules Hash of rules to look up rule
 * @param[in] rule Rule to look up
 * @param[out] pabort_rule Pointer to abort rule object
 *
 * @returns Status code
 */
static ib_status_t get_abort_rule(
    ib_hash_t        *rules,
    const ib_rule_t  *rule,
    abort_rule_t    **pabort_rule
)
{
    assert(rules != NULL);
    assert(rule != NULL);
    assert(pabort_rule != NULL);

    ib_status_t  rc;
    const char  *rule_id = ib_rule_id(rule);

    assert(rule_id != NULL);

    /* Look up the object in the "all" hash */
    rc = ib_hash_get(rules, pabort_rule, rule_id);

    /* Done */
    return rc;
}

/**
 * Get / create TX module data
 *
 * @param[in] tx Transaction
 * @param[in] module Module object
 * @param[in] create Create if required?
 * @param[out] ptx_data Pointer to module-specific TX data
 *
 * @returns Status code
 */
static ib_status_t get_create_tx_data(
    ib_tx_t            *tx,
    const ib_module_t  *module,
    bool                create,
    abort_tx_data_t   **ptx_data
)
{
    assert(tx != NULL);
    assert(module != NULL);
    assert(ptx_data != NULL);

    ib_status_t      rc;
    abort_tx_data_t *tx_data = NULL;
    ib_list_t       *abort_list;

    rc = ib_tx_get_module_data(tx, module, &tx_data);
    if ( (rc != IB_OK) && (rc != IB_ENOENT) ) {
        ib_log_error_tx(tx,
                        "%s: Failed to get TX module data: %s",
                        module->name,
                        ib_status_to_string(rc));
        return rc;
    }

    /* If it's not NULL, or we're not creating, just return it. */
    if ( (tx_data != NULL) || (!create) ) {
        *ptx_data = tx_data;
        return IB_OK;
    }

    /* Create the modifier data */
    tx_data = ib_mm_alloc(tx->mm, sizeof(*tx_data));
    if (tx_data == NULL) {
        ib_log_error_tx(tx,
                        "%s: Failed to get TX module data: %s",
                        module->name,
                        ib_status_to_string(rc));
        return IB_EALLOC;
    }

    /* Create the abort list */
    rc = ib_list_create(&abort_list, tx->mm);
    if (rc != IB_OK) {
        ib_log_error_tx(tx,
                        "%s: Failed to get TX module data: %s",
                        module->name,
                        ib_status_to_string(rc));
        return rc;
    }
    tx_data->abort_list = abort_list;

    /* Set it for the transaction */
    rc = ib_tx_set_module_data(tx, module, tx_data);
    if (rc != IB_OK) {
        ib_log_error_tx(tx,
                        "%s: Failed to set TX module data: %s",
                        module->name,
                        ib_status_to_string(rc));
        return rc;
    }

    /* Done */
    *ptx_data = tx_data;
    return IB_OK;
}

/**
 * Create function for the abort modifier.
 *
 * @param[in]  ib            IronBee engine.
 * @param[in]  mm            Memory manager.
 * @param[in]  parameters    Parameters
 * @param[out] instance_data Instance data to pass to execute.
 * @param[in]  cbdata        Callback data.
 *
 * @returns Status code
 */
static ib_status_t abort_create(
    ib_engine_t  *ib,
    ib_mm_t       mm,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ib != NULL);
    assert(instance_data != NULL);
    assert(cbdata != NULL);

    const char       *message;
    ib_var_expand_t  *expand;
    abort_modifier_t *modifier;
    ib_status_t       rc;

    /* The first argument is the type, second is message string. */
    message = (parameters == NULL) ? "" : parameters;

    /* Expand the message string as required */
    rc = ib_var_expand_acquire(&expand,
                               mm,
                               IB_S2SL(message),
                               ib_engine_var_config_get(ib),
                               NULL, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Allocate an abort instance object */
    modifier = ib_mm_alloc(mm, sizeof(*modifier));
    if (modifier == NULL) {
        return IB_EALLOC;
    }
    modifier->abort_str = "Always";
    modifier->abort_type = ABORT_ALWAYS;
    modifier->message = expand;

    *(void **)instance_data = modifier;
    return IB_OK;
}

/**
 * Create function for the abortIf modifier (action).
 *
 * @param[in]  ib            IronBee engine.
 * @param[in]  mm            Memory manager.
 * @param[in]  parameters    Parameters
 * @param[out] instance_data Instance data to pass to execute.
 * @param[in]  cbdata        Callback data.
 *
 * @returns Status code
 */
static ib_status_t abort_if_create(
    ib_engine_t  *ib,
    ib_mm_t       mm,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ib != NULL);
    assert(instance_data != NULL);
    assert(cbdata != NULL);

    ib_var_expand_t  *expand;
    ib_mm_t           tmm = ib_engine_mm_temp_get(ib);
    abort_modifier_t *modifier;
    const char       *type_str;
    abort_type_t      abort_type;
    const char       *message;
    ib_status_t       rc;

    /* The first argument is the type, second is message string. */
    type_str = parameters;
    if (parameters != NULL) {
        const char *colon = strchr(parameters, ':');
        if (colon == NULL) {
            message = "";
        }
        else {
            message = colon + 1;
            if (colon == parameters) {
                type_str = NULL;
            }
            else {
                type_str = ib_mm_memdup_to_str(tmm, parameters,
                                               colon-parameters);
            }
        }
    }
    else {
        message = "";
    }

    /* Check for abort type */
    if (type_str == NULL) {
        ib_log_error(ib, "abortIf: No type specified");
        return IB_EINVAL;
    }
    else if (strncasecmp(type_str, "OpTrue", 6) == 0) {
        abort_type = ABORT_OP_TRUE;
        type_str = "Operator/True";
    }
    else if (strncasecmp(type_str, "OpFalse", 7) == 0) {
        abort_type = ABORT_OP_FALSE;
        type_str = "Operator/False";
    }
    else if (strncasecmp(type_str, "OpOK", 4) == 0) {
        abort_type = ABORT_OP_OK;
        type_str = "Operator/OK";
    }
    else if (strncasecmp(type_str, "OpFail", 6) == 0) {
        abort_type = ABORT_OP_FAIL;
        type_str = "Operator/Fail";
    }
    else if (strncasecmp(type_str, "ActOk", 5) == 0) {
        abort_type = ABORT_ACT_OK;
        type_str = "Action/OK";
    }
    else if (strncasecmp(type_str, "ActFail", 7) == 0) {
        abort_type = ABORT_ACT_FAIL;
        type_str = "Action/Fail";
    }
    else {
        ib_log_error(ib, "abortIf: Invalid type \"%s\"", type_str);
        return IB_EINVAL;
    }

    /* Expand the message string as required */
    rc = ib_var_expand_acquire(&expand,
                               mm,
                               IB_S2SL(message),
                               ib_engine_var_config_get(ib),
                               NULL, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Allocate an abort instance object */
    modifier = ib_mm_alloc(mm, sizeof(*modifier));
    if (modifier == NULL) {
        return IB_EALLOC;
    }
    modifier->abort_str = type_str;
    modifier->abort_type = abort_type;
    modifier->message = expand;

    *(void **)instance_data = modifier;
    return IB_OK;
}

/**
 * Check status for an abort/abortIf modifier
 *
 * @param[in] ib IronBee engine (for debug)
 * @param[in] rc Operator/action result code
 * @param[in] expect_ok Expect an OK status?
 * @param[in] invert Invert the result?
 * @param[in,out] match Set to true on match
 *
 * @returns Status code
 */
static ib_status_t check_status(
    const ib_engine_t *ib,
    ib_status_t        rc,
    bool               expect_ok,
    bool               invert,
    bool              *match
)
{
    assert(ib != NULL);
    assert(match != NULL);

    bool status_match;

    /* Based on abort type, determine if we have a match. */
    if (expect_ok) {
        status_match = (rc == IB_OK);
    }
    else {
        status_match = (rc != IB_OK);
    }

    /* Interpret the result */
    *match = (invert ? !status_match : status_match);

    return IB_OK;
}

/**
 * Check result for an abort / abortIf modifier
 *
 * @param[in] ib IronBee engine (for debug)
 * @param[in] result Operator/action result
 * @param[in] rc Operator/action result code
 * @param[in] expect_true Expect a true (non-zero) result?
 * @param[in] invert Invert the result?
 * @param[in,out] match Set to true on match
 *
 * @returns Status code
 */
static ib_status_t check_result(
    const ib_engine_t *ib,
    ib_num_t           result,
    ib_status_t        rc,
    bool               expect_true,
    bool               invert,
    bool              *match
)
{
    assert(ib != NULL);
    assert(match != NULL);

    bool result_match;

    /* Based on abort type, determine if we have a match. */
    if (expect_true) {
        result_match = (result != 0);
    }
    else {
        result_match = (result == 0);
    }

    /* Interpret the result */
    *match = (rc == IB_OK) && (invert ? !result_match : result_match);

    return IB_OK;
}

/**
 * Handle one or more abort / abortIf modifiers firing.
 *
 * Logs an "ABORT:" message, and a message for each abort / abortIf modifier
 * that fired.
 *
 * If the configured abort mode is ABORT_MODE_IMMEDIATE (the default), invokes
 * abort().
 *
 * Otherwise, the abort is added to the transaction's abort list.  This will
 * cause the handle_tx_finished() to to log the aborts associated with the
 * transaction.
 *
 * @param[in] module Module object
 * @param[in] rule_exec The rule execution object
 * @param[in] label Label string
 * @param[in] name Name of action / operator
 * @param[in] aborts List of fired abort modifiers
 * @param[in] result Operator/action result value
 * @param[in] inrc Operator/action status code
 *
 */
static void abort_now(
    const ib_module_t    *module,
    const ib_rule_exec_t *rule_exec,
    const char           *label,
    const char           *name,
    const ib_list_t      *aborts,
    ib_num_t              result,
    ib_status_t           inrc
)
{
    assert(module != NULL);
    assert(rule_exec != NULL);
    assert(rule_exec->tx != NULL);
    assert(label != NULL);
    assert(aborts != NULL);

    const abort_config_t *config = NULL;
    abort_tx_data_t      *tx_data;
    const ib_list_node_t *node;
    const char           *expanded = NULL;
    size_t                expanded_length;
    size_t                num = 0;
    ib_status_t           rc;
    abort_mode_t          abort_mode;

    /* Get my module configuration */
    rc = ib_context_module_config(rule_exec->tx->ctx, module, (void *)&config);
    if (rc != IB_OK) {
        ib_log_error_tx(rule_exec->tx,
                        "Failed to get %s module configuration: %s",
                        module->name, ib_status_to_string(rc));
        abort_mode = ABORT_IMMEDIATE;
    }
    else {
        abort_mode = config->abort_mode;
    }

    /* Log the results */
    ib_rule_log_error(rule_exec,
                      "ABORT: "
                      "%s [%s] status=%d \"%s\" result=%"PRIu64" "
                      "(%zd aborts)",
                      label, name,
                      inrc, ib_status_to_string(inrc), result,
                      IB_LIST_ELEMENTS(aborts));

    /* Log all of the related aborts */
    if (aborts != NULL) {
        IB_LIST_LOOP_CONST(aborts, node) {
            const abort_modifier_t *modifier = ib_list_node_data_const(node);

            /* Increment abort number */
            ++num;

            /* Expand the string */
            rc = ib_var_expand_execute(modifier->message,
                                       &expanded, &expanded_length,
                                       rule_exec->tx->mm,
                                       rule_exec->tx->var_store);
            if (rc != IB_OK) {
                ib_rule_log_error(rule_exec,
                                  "abort: Failed to expand string: %s",
                                  ib_status_to_string(rc));
                continue;
            }

            ib_rule_log_error(rule_exec, "#%zd: %s \"%.*s\"",
                              num,
                              modifier->abort_str,
                              (int)expanded_length, expanded);
        }
    }

    switch(abort_mode) {
    case ABORT_OFF:
        break;        /* Do nothing */

    case ABORT_IMMEDIATE:
        abort();      /* Never returns */
        break;

    case ABORT_TX_END:
        /* Get the TX module data */
        rc = get_create_tx_data(rule_exec->tx, module, true, &tx_data);
        if (rc != IB_OK) {
            return;
        }

        /* Add the rule to the list */
        rc = ib_list_push(tx_data->abort_list, rule_exec->rule);
        if (rc != IB_OK) {
            return;
        }
        break;
    }

}

/**
 * Post operator function
 *
 * @param[in] rule_exec Rule execution environment.
 * @param[in] opinst Operator instance.
 * @param[in] invert True iff this operator is inverted.
 * @param[in] value Input to operator.
 * @param[in] op_rc Result code of operator execution.
 * @param[in] result Result of operator.
 * @param[in] capture Capture collection of operator.
 * @param[in] cbdata Callback data (@ref ib_module_t).
 */
void abort_post_operator(
    const ib_rule_exec_t *rule_exec,
    const ib_operator_inst_t *opinst,
    bool                  invert,
    const ib_field_t     *value,
    ib_status_t           op_rc,
    ib_num_t              result,
    ib_field_t           *capture,
    void                 *cbdata
)
{
    assert(rule_exec != NULL);
    assert(opinst != NULL);
    assert(cbdata != NULL);

    ib_status_t                rc;
    const ib_module_t         *module = cbdata;
    const abort_module_data_t *module_data;
    const ib_list_node_t      *node;
    abort_rule_t              *abort_rule;
    bool                       do_abort = false;
    ib_list_t                 *aborts = NULL;

    assert(module->data != NULL);
    module_data = module->data;

    /* Find associated abort rule item (if there is one) */
    rc = get_abort_rule(module_data->op_rules, rule_exec->rule, &abort_rule);
    if (rc == IB_ENOENT) {
        return;
    }
    else if (rc != IB_OK) {
        ib_rule_log_error(rule_exec, "%s: Failed to get rule data: %s",
                          module->name, ib_status_to_string(rc));
        return;
    }

    /* Loop through the rule's abort operator modifiers */
    IB_LIST_LOOP_CONST(abort_rule->abort_modifiers, node) {
        const abort_modifier_t *modifier = ib_list_node_data_const(node);
        const ib_engine_t      *ib = rule_exec->ib;

        /* Based on abort type, determine if we have a match. */
        switch(modifier->abort_type) {
        case ABORT_OP_TRUE:
            check_result(ib, result, op_rc, true,
                         (invert ^ modifier->is_false), &do_abort);
            break;
        case ABORT_OP_FALSE:
            check_result(ib, result, op_rc, false,
                         (invert ^ modifier->is_false), &do_abort);
            break;

        case ABORT_OP_OK:
            check_status(ib, op_rc, true, modifier->is_false, &do_abort);
            break;
        case ABORT_OP_FAIL:
            check_status(ib, op_rc, false, modifier->is_false, &do_abort);
            break;

        case ABORT_ALWAYS:
            check_result(ib, 1, op_rc, true, modifier->is_false, &do_abort);
            break;

        default:
            assert(0 && "Action abort at operator post!");
            break;
        }

        /* Create the modifiers list if required */
        if (do_abort) {
            if (aborts == NULL) {
                ib_list_create(&aborts, rule_exec->tx->mm);
            }
            if (aborts != NULL) {
                ib_list_push(aborts, (void *)modifier);
            }
        }
    }

    /* If any of the modifiers set the abort flag, do it now */
    if (do_abort) {
        abort_now(module, rule_exec, "Operator",
                  ib_operator_name(ib_operator_inst_operator(opinst)),
                  aborts, result, op_rc);
    }
}

/**
 * Post action function
 *
 * @param[in] rule_exec Rule execution environment.
 * @param[in] action_inst Instance data for action just executed.
 * @param[in] act_rc Result code of action execution.
 * @param[in] result Result of operator.
 * @param[in] cbdata Callback data (@ref ib_module_t).
 */
void abort_post_action(
    const ib_rule_exec_t   *rule_exec,
    const ib_action_inst_t *action_inst,
    ib_num_t                result,
    ib_status_t             act_rc,
    void                   *cbdata
)
{
    assert(rule_exec != NULL);
    assert(action_inst != NULL);
    assert(cbdata != NULL);

    ib_status_t                rc;
    const ib_module_t         *module = cbdata;
    const abort_module_data_t *module_data;
    const ib_list_node_t      *node;
    abort_rule_t              *abort_rule;
    bool                       do_abort = false;
    ib_list_t                 *aborts = NULL;
    const char                *name =
        ib_action_name(ib_action_inst_action(action_inst));

    assert(module->data != NULL);
    module_data = module->data;

    /* Ignore abort/abortIf actions! */
    if ( (strcasecmp(name, "abort") == 0)   ||
         (strcasecmp(name, "abortIf") == 0) )
    {
        const abort_modifier_t *modifier = ib_action_inst_data(action_inst);

        switch (modifier->abort_type) {
        case ABORT_ACT_OK:
        case ABORT_ACT_FAIL:
            return;
        default:
            break;
        }
    }

    /* Find associated abort rule item (if there is one) */
    rc = get_abort_rule(module_data->act_rules, rule_exec->rule, &abort_rule);
    if (rc == IB_ENOENT) {
        return;
    }
    else if (rc != IB_OK) {
        ib_rule_log_error(rule_exec, "%s: Failed to get rule data: %s",
                          module->name, ib_status_to_string(rc));
        return;
    }

    /* Loop through the rule's abort operator modifiers */
    IB_LIST_LOOP_CONST(abort_rule->abort_modifiers, node) {
        const abort_modifier_t *modifier = ib_list_node_data_const(node);
        const ib_engine_t      *ib = rule_exec->ib;

        /* Based on abort type, determine if we have a match. */
        switch(modifier->abort_type) {
        case ABORT_ACT_OK:
            check_status(ib, act_rc, true, modifier->is_false, &do_abort);
            break;
        case ABORT_ACT_FAIL:
            check_status(ib, act_rc, false, modifier->is_false, &do_abort);
            break;

        case ABORT_ALWAYS:
            check_result(ib, 1, act_rc, true, modifier->is_false, &do_abort);
            break;

        default:
            assert(0 && "Operator abort at action post!");
            break;
        }

        /* Create the modifier list if required */
        if (do_abort) {
            if (aborts == NULL) {
                ib_list_create(&aborts, rule_exec->tx->mm);
            }
            if (aborts != NULL) {
                ib_list_push(aborts, (void *)modifier);
            }
        }
    }

    /* If any of the modifiers set the abort flag, do it now */
    if (do_abort) {
        abort_now(module, rule_exec, "Action", name,
                  aborts, result, act_rc);
    }

    return;
}

/**
 * Parse an AbortMode directive.
 *
 * @details Register a AbortMode directive to the engine.
 * @param[in] cp Configuration parser
 * @param[in] directive The directive name.
 * @param[in] p1 The first parameter passed to @a directive (mode).
 * @param[in] cbdata User data (module)
 */
static ib_status_t abort_mode_handler(
    ib_cfgparser_t  *cp,
    const char      *directive,
    const char      *p1,
    void            *cbdata
)
{
    assert(cp != NULL);
    assert(directive != NULL);
    assert(p1 != NULL);
    assert(cbdata != NULL);

    ib_status_t     rc;
    ib_module_t    *module = cbdata;
    ib_context_t   *context;
    const char     *mode;
    abort_config_t *config;
    abort_mode_t    abort_mode;

    /* Get my configuration context */
    rc = ib_cfgparser_context_current(cp, &context);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "TxData: Failed to get current context: %s",
                         ib_status_to_string(rc));
        return rc;
    }

    /* Get my module context configuration */
    rc = ib_context_module_config(context, module, (void *)&config);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to get %s module configuration: %s",
                         MODULE_NAME_STR, ib_status_to_string(rc));
        return rc;
    }

    /* Get the mode name string */
    mode = p1;
    if (strcasecmp(mode, "Immediate") == 0) {
        abort_mode = ABORT_IMMEDIATE;
    }
    else if (strcasecmp(mode, "TxEnd") == 0) {
        abort_mode = ABORT_TX_END;
    }
    else if (strcasecmp(mode, "Off") == 0) {
        abort_mode = ABORT_OFF;
    }
    else {
        ib_cfg_log_error(cp, "%s: Invalid AbortMode \"%s\"",
                         MODULE_NAME_STR, mode);
        return IB_EINVAL;
    }
    config->abort_mode = abort_mode;

    /* Done */
    return IB_OK;
}

/**
 * Handle TX finished event.
 *
 * Checks to see if any aborts fired during @a tx.  If so, logs a summary of
 * the aborts that fired.  If the configured abort mode is ABORT_TX_END,
 * abort() is then invoked.
 *
 * @param[in] ib Engine.
 * @param[in] tx Transaction.
 * @param[in] event Event type.
 * @param[in] cbdata Callback data (module object)
 *
 * @returns
 *   - IB_OK on success.
 */
static ib_status_t handle_tx_finished(
    ib_engine_t           *ib,
    ib_tx_t               *tx,
    ib_state_event_type_t  event,
    void                  *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(event == tx_finished_event);
    assert(cbdata != NULL);

    ib_status_t           rc;
    ib_module_t          *module = cbdata;
    abort_config_t       *config;
    abort_tx_data_t      *tx_data;
    const ib_list_node_t *node;
    size_t                num = 0;

    /* Get my module configuration */
    rc = ib_context_module_config(tx->ctx, module, (void *)&config);
    if (rc != IB_OK) {
        ib_log_error_tx(tx,
                        "Failed to get %s module configuration: %s",
                        module->name, ib_status_to_string(rc));
        return rc;
    }

    /* Get the TX module data */
    rc = get_create_tx_data(tx, module, false, &tx_data);
    if (rc != IB_OK) {
        return rc;
    }
    else if (tx_data == NULL) {
        return IB_OK;
    }
    else if (IB_LIST_ELEMENTS(tx_data->abort_list) == 0) {
        return IB_OK;
    }

    /* Log it */
    ib_log_error_tx(tx, "ABORT: %zd aborts fired in transaction:",
                    IB_LIST_ELEMENTS(tx_data->abort_list));
    IB_LIST_LOOP_CONST(tx_data->abort_list, node) {
        const ib_rule_t *rule = ib_list_node_data_const(node);
        ++num;
        ib_log_error_tx(tx, "#%zd: Rule \"%s\"", num, rule->meta.full_id);
    }

    /* We're outta here */
    if (config->abort_mode == ABORT_TX_END) {
        abort();
    }
    return IB_OK;
}

/**
 * Search for the rule for matching actions.
 *
 * Search through @a rule for actions matching @a name.  True actions are
 * stored in the @a true_modifiers list, false actions in the @a
 * false_modifiers list.
 *
 * @param[in] ib IronBee engine
 * @param[in] rule Rule to search
 * @param[in] name Action name to search for
 * @param[in] true_modifiers Matching rule true actions
 * @param[in] false_modifiers Matching rule false actions
 *
 * @returns Status code
 */
static ib_status_t rule_search(
    const ib_engine_t *ib,
    const ib_rule_t   *rule,
    const char        *name,
    ib_list_t         *true_modifiers,
    ib_list_t         *false_modifiers
)
{
    ib_status_t rc;

    /* Search the True action list */
    ib_list_clear(true_modifiers);
    rc = ib_rule_search_action(ib, rule,
                               IB_RULE_ACTION_TRUE, name,
                               true_modifiers, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Search the False action list */
    ib_list_clear(false_modifiers);
    rc = ib_rule_search_action(ib, rule,
                               IB_RULE_ACTION_FALSE, name,
                               false_modifiers, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Create the abort rule object associated with the @a rule (if required).
 *
 * If an abort rule object already exists for @a rule, that abort rule object
 * is returned.  Otherwise, one is created and stored in the @a rules hash.
 *
 * The hash key used in @a rules is the rule ID.
 *
 * @param[in] ib IronBee engine
 * @param[in] mm Memory manager to use for allocations
 * @param[in] module Module object
 * @param[in] rules Hash of rules to look up rule
 * @param[in] rule Rule to look up
 * @param[out] pabort_rule Pointer to abort rule object
 *
 * @returns Status code
 */
static ib_status_t create_abort_rule(
    const ib_engine_t  *ib,
    ib_mm_t             mm,
    ib_module_t        *module,
    ib_hash_t          *rules,
    const ib_rule_t    *rule,
    abort_rule_t      **pabort_rule
)
{
    assert(rules != NULL);
    assert(rule != NULL);
    assert(pabort_rule != NULL);

    ib_status_t   rc;
    abort_rule_t *abort_rule = NULL;
    ib_list_t    *abort_modifiers;
    const char   *rule_id = ib_rule_id(rule);

    assert(rule_id != NULL);

    /* Look up the object in the "all" hash */
    rc = ib_hash_get(rules, &abort_rule, rule_id);
    if ( (rc != IB_OK) && (rc != IB_ENOENT) ) {
        ib_log_error(ib, "%s: Failed to get rule data for \"%s\": %s",
                     module->name, rule_id, ib_status_to_string(rc));
        return rc;
    }

    /* If it's not NULL, or we're not creating, just return it. */
    if (abort_rule != NULL) {
        *pabort_rule = abort_rule;
        return IB_OK;
    }

    /* Create the abort rule object */
    abort_rule = ib_mm_alloc(mm, sizeof(*abort_rule));
    if (abort_rule == NULL) {
        ib_log_error(ib, "%s: Failed to allocate abort rule object",
                     module->name);
        return IB_EALLOC;
    }

    /* Create the modifier list */
    rc = ib_list_create(&abort_modifiers, mm);
    if (rc != IB_OK) {
        ib_log_error(ib, "%s: Failed to create modifier list: %s",
                     module->name, ib_status_to_string(rc));
        return rc;
    }
    abort_rule->abort_modifiers = abort_modifiers;
    abort_rule->rule = rule;

    /* Save it into the hash */
    rc = ib_hash_set(rules, rule_id, abort_rule);
    if (rc != IB_OK) {
        ib_log_error(ib, "%s: Failed to set rule data for \"%s\": %s",
                     module->name, rule_id, ib_status_to_string(rc));
        return rc;
    }

    /* Done */
    *pabort_rule = abort_rule;
    return IB_OK;
}

/**
 * Add modifiers to the abort rule's modifier list
 *
 * If @a filter_fn is not NULL, it is invoked for each of the abort modifiers.
 * @a filter_fn is expected to return true if the abort modifier matches, false
 * if not.  If @a filter_fn returns true, the abort modifier is added to the
 * associated list; if not, it is ignored.  @sa abort_filter_fn_t
 *
 * @param[in] ib IronBee engine
 * @param[in] mm Memory manager to use for allocations
 * @param[in] module Module object
 * @param[in] rules_hash Rules hash to operate on
 * @param[in] rule Associated rule
 * @param[in] filter_fn Filter function or NULL @ref abort_filter_fn_t
 * @param[in] true_modifiers List of true modifiers to add
 * @param[in] false_modifiers List of false modifiers to add
 *
 * @returns Status code
 */
static ib_status_t add_abort_modifiers(
    const ib_engine_t *ib,
    ib_mm_t            mm,
    ib_module_t       *module,
    ib_hash_t         *rules_hash,
    const ib_rule_t   *rule,
    abort_filter_fn_t  filter_fn,
    const ib_list_t   *true_modifiers,
    const ib_list_t   *false_modifiers
)
{
    ib_status_t           rc;
    abort_rule_t         *abort_rule;
    const ib_list_node_t *node;

    /* Create the abort rule object if required */
    rc = create_abort_rule(ib, mm, module, rules_hash, rule, &abort_rule);
    if (rc != IB_OK) {
        return rc;
    }

    /* Add the matching true abort_modifier_t to the abort modifier list */
    IB_LIST_LOOP_CONST(true_modifiers, node) {
        const ib_action_inst_t *inst = ib_list_node_data_const(node);
        abort_modifier_t       *abort_modifier =
            ib_action_inst_data(inst);

        /* Filter out abort modifiers that the filter rejects */
        if ( (filter_fn == NULL) || (filter_fn(abort_modifier)) ) {
            abort_modifier->is_false = false;
            rc = ib_list_push(abort_rule->abort_modifiers, abort_modifier);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }

    /* Add the matching false abort_modifier_t to the abort modifier list */
    IB_LIST_LOOP_CONST(false_modifiers, node) {
        const ib_action_inst_t *inst = ib_list_node_data_const(node);
        abort_modifier_t       *abort_modifier =
            ib_action_inst_data(inst);

        /* Filter out abort modifiers that the filter rejects */
        if ( (filter_fn == NULL) || (filter_fn(abort_modifier)) ) {
            abort_modifier->is_false = true;
            rc = ib_list_push(abort_rule->abort_modifiers, abort_modifier);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }

    /* Done */
    return IB_OK;
}

/**
 * Filter operator aborts.
 *
 * @param[in] modifier Abort modifier to filter
 *
 * @returns True if @a modifier is an operator abort, otherwise false
 */
static bool abort_op_filter(
    const abort_modifier_t *modifier
)
{
    switch (modifier->abort_type) {
    case ABORT_ALWAYS:
    case ABORT_OP_TRUE:
    case ABORT_OP_FALSE:
    case ABORT_OP_OK:
    case ABORT_OP_FAIL:
        return true;
    case ABORT_ACT_OK:
    case ABORT_ACT_FAIL:
        return false;
    }
    return false;
}

/**
 * Filter action aborts.
 *
 * @param[in] modifier Abort modifier to filter
 *
 * @returns True if @a modifier is an action abort, otherwise false
 */
static bool abort_act_filter(
    const abort_modifier_t *modifier
)
{
    switch (modifier->abort_type) {
    case ABORT_OP_TRUE:
    case ABORT_OP_FALSE:
    case ABORT_OP_OK:
    case ABORT_OP_FAIL:
        return false;
    case ABORT_ALWAYS:
    case ABORT_ACT_OK:
    case ABORT_ACT_FAIL:
        return true;
    }
    return false;
}

/**
 * Handle rule ownership callbacks.
 *
 * Checks for abort or abortIf modifiers (actions) associated with the rule.
 * If so, add the rule to the appropriate rule hash.
 *
 * @param[in] ib Engine
 * @param[in] rule Rule being registered
 * @param[in] ctx Context rule is enabled in
 * @param[in] cbdata Callback data (@ref ib_module_t)
 *
 * @returns IB_DECLINE / Error status
 */
static ib_status_t abort_rule_ownership(
    const ib_engine_t  *ib,
    const ib_rule_t    *rule,
    const ib_context_t *ctx,
    void               *cbdata
)
{
    assert(ib != NULL);
    assert(rule != NULL);
    assert(cbdata != NULL);

    ib_status_t          rc;
    ib_module_t         *module = cbdata;
    abort_module_data_t *module_data = module->data;
    ib_mm_t              mm = ib_engine_mm_main_get(ib);
    ib_list_t           *true_modifiers;
    ib_list_t           *false_modifiers;
    ib_mm_t              tmm = ib_engine_mm_temp_get(ib);

    assert(module_data != NULL);

    /* Create the search lists */
    rc = ib_list_create(&true_modifiers, tmm);
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_list_create(&false_modifiers, tmm);
    if (rc != IB_OK) {
        return rc;
    }

    /*
     * Handle abort modifiers
     */

    /* Search for abort actions */
    rc = rule_search(ib, rule, "abort", true_modifiers, false_modifiers);
    if (rc != IB_OK) {
        return rc;
    }

    /* If there are any matches, add this rule to both hashes */
    if ( (IB_LIST_ELEMENTS(true_modifiers) != 0) ||
         (IB_LIST_ELEMENTS(false_modifiers) != 0) )
    {
        rc = add_abort_modifiers(ib, mm, module,
                                 module_data->op_rules, rule,
                                 NULL,
                                 true_modifiers, false_modifiers);
        if (rc != IB_OK) {
            return rc;
        }
        rc = add_abort_modifiers(ib, mm, module,
                                 module_data->act_rules, rule,
                                 NULL,
                                 true_modifiers, false_modifiers);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /*
     * Handle abortIf modifiers
     */

    /* Search for abortIf actions */
    rc = rule_search(ib, rule, "abortIf", true_modifiers, false_modifiers);
    if (rc != IB_OK) {
        return rc;
    }

    /* If there are any matches, add this rule to both hashes */
    if ( (IB_LIST_ELEMENTS(true_modifiers) != 0) ||
         (IB_LIST_ELEMENTS(false_modifiers) != 0) )
    {
        rc = add_abort_modifiers(ib, mm, module,
                                 module_data->op_rules, rule,
                                 abort_op_filter,
                                 true_modifiers, false_modifiers);
        if (rc != IB_OK) {
            return rc;
        }
        rc = add_abort_modifiers(ib, mm, module,
                                 module_data->act_rules, rule,
                                 abort_act_filter,
                                 true_modifiers, false_modifiers);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_DECLINED;
}

/**
 * Initialize the abort module
 *
 * @param[in] ib IronBee engine
 * @param[in] module Module
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 **/
static ib_status_t abort_init(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *cbdata)
{
    ib_status_t          rc;
    ib_mm_t              mm = ib_engine_mm_main_get(ib);
    abort_module_data_t *module_data;

    /* Create the abort module data */
    module_data = ib_mm_alloc(mm, sizeof(*module_data));
    if (module_data == NULL) {
        return IB_EALLOC;
    }

    /* Create the rule hashes */
    rc = ib_hash_create_nocase(&(module_data->op_rules), mm);
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_hash_create_nocase(&(module_data->act_rules), mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Store the module data */
    module->data = module_data;

    /* Register the abort action */
    rc = ib_action_create_and_register(
        NULL, ib,
        "abort",
        abort_create, module,
        NULL, NULL, /* no destroy function */
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the abortIf action */
    rc = ib_action_create_and_register(
        NULL, ib,
        "abortIf",
        abort_if_create, module,
        NULL, NULL, /* no destroy function */
        NULL, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Register the AbortMode directive */
    rc = ib_config_register_directive(ib,
                                      "AbortMode",
                                      IB_DIRTYPE_PARAM1,
                                      (ib_void_fn_t)abort_mode_handler,
                                      NULL,
                                      module,
                                      NULL,
                                      NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register AbortMode directive: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Register the rule ownership function */
    rc = ib_rule_register_ownership_fn(ib, "abort",
                                       abort_rule_ownership, module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register Abort rule ownership function: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Register the post operator function */
    rc = ib_rule_register_post_operator_fn(ib, abort_post_operator, module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register post operator function: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Register the post action function */
    rc = ib_rule_register_post_action_fn(ib, abort_post_action, module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register post action function: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Register the TX finished event */
    rc = ib_hook_tx_register(ib, tx_finished_event,
                             handle_tx_finished, module);
    if (rc != IB_OK) {
        ib_log_error(ib, "%s: Failed to register tx finished handler: %s",
                     module->name,
                     ib_status_to_string(rc));
        return rc;
    }

    return IB_OK;
}

/**
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,               /* Default metadata */
    MODULE_NAME_STR,                         /* Module name */
    IB_MODULE_CONFIG(&abort_config),         /* Global config data */
    NULL,                                    /* Module config map */
    NULL,                                    /* Module directive map */
    abort_init,                              /* Initialize function */
    NULL,                                    /* Callback data */
    NULL,                                    /* Finish function */
    NULL,                                    /* Callback data */
);
