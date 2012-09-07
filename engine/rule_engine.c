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
 * @brief IronBee
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/rule_engine.h>
#include "rule_engine_private.h"
#include "engine_private.h"

#include <ironbee/action.h>
#include <ironbee/bytestr.h>
#include <ironbee/config.h>
#include <ironbee/debug.h>
#include <ironbee/escape.h>
#include <ironbee/field.h>
#include <ironbee/mpool.h>
#include <ironbee/operator.h>
#include <ironbee/transformation.h>
#include <ironbee/util.h>

#include <assert.h>
#include <inttypes.h>

/**
 * Phase Flags
 */
#define PHASE_FLAG_NONE          (0x0)     /**< No phase flags */
#define PHASE_FLAG_IS_VALID      (1 << 0)  /**< Phase is valid */
#define PHASE_FLAG_IS_STREAM     (1 << 1)  /**< Phase is steam inspection */
#define PHASE_FLAG_ALLOW_CHAIN   (1 << 2)  /**< Rule allows chaining */
#define PHASE_FLAG_ALLOW_TFNS    (1 << 3)  /**< Rule allows transformations */
#define PHASE_FLAG_FORCE         (1 << 4)  /**< Force execution for phase */
#define PHASE_FLAG_REQUEST       (1 << 5)  /**< One of the request phases */
#define PHASE_FLAG_RESPONSE      (1 << 6)  /**< One of the response phases */
#define PHASE_FLAG_POSTPROCESS   (1 << 7)  /**< Post process phase */

/**
 * Max # of data types (IB_DTYPE_*) per rule phase
 */
#define MAX_PHASE_DATA_TYPES 4

/**
 * Data on each rule phase, one per phase.
 */
struct ib_rule_phase_meta_t {
    bool                   is_stream;
    ib_rule_phase_t        phase_num;
    ib_state_hook_type_t   hook_type;
    ib_flags_t             flags;
    const char            *name;
    ib_flags_t             required_op_flags;
    ib_state_event_type_t  event;
};

/* Rule definition data */
static const ib_rule_phase_meta_t rule_phase_meta[] =
{
    {
        false,
        PHASE_NONE,
        (ib_state_hook_type_t) -1,
        ( PHASE_FLAG_ALLOW_CHAIN |
          PHASE_FLAG_ALLOW_TFNS ),
        "Generic 'Phase' Rule",
        IB_OP_FLAG_PHASE,
        (ib_state_event_type_t) -1
    },
    {
        false,
        PHASE_REQUEST_HEADER,
        IB_STATE_HOOK_TX,
        ( PHASE_FLAG_IS_VALID |
          PHASE_FLAG_ALLOW_CHAIN |
          PHASE_FLAG_ALLOW_TFNS |
          PHASE_FLAG_REQUEST ),
        "Request Header",
        IB_OP_FLAG_PHASE,
        handle_request_header_event
    },
    {
        false,
        PHASE_REQUEST_BODY,
        IB_STATE_HOOK_TX,
        ( PHASE_FLAG_IS_VALID |
          PHASE_FLAG_ALLOW_CHAIN |
          PHASE_FLAG_ALLOW_TFNS |
          PHASE_FLAG_REQUEST ),
        "Request Body",
        IB_OP_FLAG_PHASE,
        handle_request_event
    },
    {
        false,
        PHASE_RESPONSE_HEADER,
        IB_STATE_HOOK_TX,
        ( PHASE_FLAG_IS_VALID |
          PHASE_FLAG_ALLOW_CHAIN |
          PHASE_FLAG_ALLOW_TFNS |
          PHASE_FLAG_RESPONSE ),
        "Response Header",
        IB_OP_FLAG_PHASE,
        handle_response_header_event
    },
    {
        false,
        PHASE_RESPONSE_BODY,
        IB_STATE_HOOK_TX,
        ( PHASE_FLAG_IS_VALID |
          PHASE_FLAG_ALLOW_CHAIN |
          PHASE_FLAG_ALLOW_TFNS |
          PHASE_FLAG_RESPONSE ),
        "Response Body",
        IB_OP_FLAG_PHASE,
        handle_response_event
    },
    {
        false,
        PHASE_POSTPROCESS,
        IB_STATE_HOOK_TX,
        ( PHASE_FLAG_IS_VALID |
          PHASE_FLAG_ALLOW_CHAIN |
          PHASE_FLAG_ALLOW_TFNS |
          PHASE_FLAG_FORCE |
          PHASE_FLAG_POSTPROCESS ),
        "Post Process",
        IB_OP_FLAG_PHASE,
        handle_postprocess_event
    },

    /* Stream rule phases */
    {
        true,
        PHASE_NONE,
        (ib_state_hook_type_t) -1,
        (PHASE_FLAG_IS_STREAM),
        "Generic 'Stream Inspection' Rule",
        IB_OP_FLAG_STREAM,
        (ib_state_event_type_t) -1
    },
    {
        true,
        PHASE_STR_REQUEST_HEADER,
        IB_STATE_HOOK_TX,
        ( PHASE_FLAG_IS_VALID |
          PHASE_FLAG_IS_STREAM |
          PHASE_FLAG_REQUEST ),
        "Request Header Stream",
        IB_OP_FLAG_STREAM,
        handle_context_tx_event
    },
    {
        true,
        PHASE_STR_REQUEST_BODY,
        IB_STATE_HOOK_TXDATA,
        ( PHASE_FLAG_IS_VALID |
          PHASE_FLAG_IS_STREAM |
          PHASE_FLAG_REQUEST ),
        "Request Body Stream",
        IB_OP_FLAG_STREAM,
        request_body_data_event
    },
    {
        true,
        PHASE_STR_RESPONSE_HEADER,
        IB_STATE_HOOK_HEADER,
        ( PHASE_FLAG_IS_VALID |
          PHASE_FLAG_IS_STREAM |
          PHASE_FLAG_RESPONSE ),
        "Response Header Stream",
        IB_OP_FLAG_STREAM,
        response_header_data_event
    },
    {
        true,
        PHASE_STR_RESPONSE_BODY,
        IB_STATE_HOOK_TXDATA,
        ( PHASE_FLAG_IS_VALID |
          PHASE_FLAG_IS_STREAM |
          PHASE_FLAG_RESPONSE ),
        "Response Body Stream",
        IB_OP_FLAG_STREAM,
        response_body_data_event
    },
    {
        false,
        PHASE_INVALID,
        (ib_state_hook_type_t) -1,
        PHASE_FLAG_NONE,
        "Invalid",
        IB_OP_FLAG_NONE,
        (ib_state_event_type_t) -1
    }
};

/**
 * Simple stack of values for creating FIELD, FIELD_NAME and
 * FIELD_NAME_FULL fields
 */
typedef struct {
    ib_list_t *stack;           /**< The actual list */
} value_stack_t;

/**
 * The rule engine uses recursion to walk through lists and chains.  These
 * define the limits of the recursion depth.
 */
#define MAX_LIST_RECURSION   (5)       /**< Max list recursion limit */
#define MAX_CHAIN_RECURSION  (10)      /**< Max chain recursion limit */

/**
 * Test the validity of a phase number
 *
 * @param phase_num Phase number to check
 *
 * @returns true if @a phase_num is valid, false if not.
 */
static inline bool is_phase_num_valid(ib_rule_phase_t phase_num)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_UINT(
        ( (phase_num >= PHASE_NONE) && (phase_num < IB_RULE_PHASE_COUNT) )
        ? true : false);
}

/**
 * Find a rule's matching phase meta data, matching the only the phase number.
 *
 * @param[in] phase_num Phase number (PHASE_xxx)
 * @param[out] phase_meta Matching rule phase meta-data
 *
 * @returns Status code
 */
static ib_status_t find_phase_meta(ib_rule_phase_t phase_num,
                                   const ib_rule_phase_meta_t **phase_meta)
{
    IB_FTRACE_INIT();
    const ib_rule_phase_meta_t *meta;

    assert (phase_meta != NULL);
    assert (is_phase_num_valid(phase_num));

    /* Loop through all parent rules */
    for (meta = rule_phase_meta;  meta->phase_num != PHASE_INVALID;  ++meta)
    {
        if (meta->phase_num == phase_num) {
            *phase_meta = meta;
            IB_FTRACE_RET_STATUS(IB_OK);
        }
    }
    IB_FTRACE_RET_STATUS(IB_ENOENT);
}

/**
 * Find a rule's matching phase meta data, matching the phase number and
 * the phase's stream type.
 *
 * @param[in] is_stream true if this is a "stream inspection" rule
 * @param[in] phase_num Phase number (PHASE_xxx)
 * @param[out] phase_meta Matching rule phase meta-data
 *
 * @note If @a is_stream is IB_TRI_UNSET, this function will return the
 * first phase meta-data object that matches the phase number.
 *
 * @returns Status code
 */
static ib_status_t find_phase_stream_meta(
    bool is_stream,
    ib_rule_phase_t phase_num,
    const ib_rule_phase_meta_t **phase_meta)
{
    IB_FTRACE_INIT();
    const ib_rule_phase_meta_t *meta;

    assert (phase_meta != NULL);
    assert (is_phase_num_valid(phase_num));

    /* Loop through all parent rules */
    for (meta = rule_phase_meta;  meta->phase_num != PHASE_INVALID;  ++meta)
    {
        if ( (meta->phase_num == phase_num) &&
             (is_stream == meta->is_stream) )
        {
            *phase_meta = meta;
            IB_FTRACE_RET_STATUS(IB_OK);
        }
    }
    IB_FTRACE_RET_STATUS(IB_ENOENT);
}

/**
 * Execute transformations on a target.
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction
 * @param[in] rule The rule being executed
 * @param[in] target Target field
 * @param[in] value Initial value of the target field
 * @param[out] result Pointer to field in which to store the result
 * @param[in,out] log_exec Rule execution logging object
 *
 * @returns Status code
 */
static ib_status_t execute_tfns(ib_engine_t *ib,
                                ib_tx_t *tx,
                                const ib_rule_t *rule,
                                ib_rule_target_t *target,
                                ib_field_t *value,
                                ib_field_t **result,
                                ib_rule_log_exec_t *log_exec)
{
    IB_FTRACE_INIT();
    ib_status_t     rc;
    int             n = 0;
    ib_list_node_t *node = NULL;
    ib_field_t     *in_field;
    ib_field_t     *out = NULL;

    assert(ib != NULL);
    assert(tx != NULL);
    assert(target != NULL);
    assert(result != NULL);

    /* No transformations?  Do nothing. */
    if (value == NULL) {
        *result = NULL;
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (IB_LIST_ELEMENTS(target->tfn_list) == 0) {
        *result = value;
        ib_rule_log_debug(tx, rule, target, NULL, "No transformations");
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_rule_log_debug(tx, rule, target, NULL,
                      "Executing %zd transformations",
                      IB_LIST_ELEMENTS(target->tfn_list));

    /*
     * Loop through all of the field operators.
     */
    in_field = value;
    IB_LIST_LOOP(target->tfn_list, node) {
        ib_tfn_t  *tfn = (ib_tfn_t *)node->data;
        ib_flags_t flags = 0;

        /* Run it */
        ++n;
        ib_rule_log_debug(tx, rule, target, tfn,
                          "Executing field transformation #%d", n);
        ib_rule_log_field(tx, rule, target, tfn, "before tfn", in_field);
        rc = ib_tfn_transform(ib, tx->mp, tfn, in_field, &out, &flags);
        if (rc != IB_OK) {
            ib_rule_log_error(tx, rule, target, tfn,
                              "Error executing target transformation: %s",
                              ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
        ib_rule_log_field(tx, rule, target, tfn, "after tfn", out);
        ib_rule_log_exec_add_tfn(log_exec, tfn, in_field, out);

        /* Verify that out isn't NULL */
        if (out == NULL) {
            ib_rule_log_error(tx, rule, target, tfn,
                              "Target transformation #%d returned NULL", n);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* The output of the operator is now input for the next field op. */
        in_field = out;
    }

    /* The output of the final operator is the result */
    *result = out;

    /* Done. */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute a single rule action
 *
 * @param[in] ib Engine
 * @param[in] rule Rule to execute
 * @param[in,out] tx Transaction
 * @param[in] result Rule execution result
 * @param[in] action The action to execute
 *
 * @returns Status code
 */
static ib_status_t execute_action(ib_engine_t *ib,
                                  const ib_rule_t *rule,
                                  ib_tx_t *tx,
                                  ib_num_t result,
                                  const ib_action_inst_t *action)
{
    IB_FTRACE_INIT();
    ib_status_t   rc;
    const char   *name = (result != 0) ? "True" : "False";

    ib_rule_log_debug(tx, rule, NULL, NULL,
                      "Executing %s rule action %s",
                      name, action->action->name);

    /* Run it, check the results */
    rc = ib_action_execute(action, rule, tx);
    if ( rc != IB_OK ) {
        ib_rule_log_error(tx, rule, NULL, NULL,
                          "Action \"%s\" returned an error: %s",
                          action->action->name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute a rule's actions
 *
 * @param[in] ib Engine
 * @param[in] rule Rule to execute
 * @param[in,out] tx Transaction
 * @param[in] result Rule execution result
 * @param[in] actions List of actions to execute
 *
 * @returns Status code
 */
static ib_status_t execute_action_list(ib_engine_t *ib,
                                       const ib_rule_t *rule,
                                       ib_tx_t *tx,
                                       ib_num_t result,
                                       const ib_list_t *actions)
{
    IB_FTRACE_INIT();
    const ib_list_node_t *node = NULL;
    ib_status_t           rc = IB_OK;
    const char           *name = (result != 0) ? "True" : "False";

    ib_rule_log_trace(tx, rule, NULL, NULL, "Executing rule %s actions", name);

    /*
     * Loop through all of the fields
     *
     * @todo The current behavior is to keep running even after an action
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    IB_LIST_LOOP_CONST(actions, node) {
        ib_status_t       arc;     /* Action's return code */
        const ib_action_inst_t *action = (const ib_action_inst_t *)node->data;

        /* Execute the action */
        arc = execute_action(ib, rule, tx, result, action);

        /* Record an error status code unless a block rc is to be reported. */
        if (arc != IB_OK) {
            ib_rule_log_error(tx, rule, NULL, NULL,
                              "Action %s/\"%s\" returned an error: %s",
                              name,
                              action->action->name,
                              ib_status_to_string(arc));
            rc = arc;
        }
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Perform a block operation by signaling an error to the server.
 *
 * The server is signaled with
 * ib_server_error_response(ib_server_t *, ib_context_t *, int) using
 * @a tx and @a ib to provide the @c ib_context_t* and @c ib_server_t*,
 * respectively.
 *
 * @param ib IronBee engine containing the server callbacks.
 * @param tx Transaction containing the active context.
 *
 * @returns The result of calling ib_server_error_response().
 */
static ib_status_t report_block_to_server(ib_engine_t *ib, ib_tx_t *tx)
{
    IB_FTRACE_INIT();

    /* Store the final return code here. */
    ib_status_t rc;

    assert(ib);
    assert(ib->server);
    assert(tx);
    assert(tx->ctx);

    ib_log_debug_tx(tx, "Setting HTTP error response: status=%d",
                    tx->block_status);
    rc = ib_server_error_response(ib->server, tx, tx->block_status);
    if (rc == IB_DECLINED) {
        ib_log_notice_tx(tx,
                         "Server not willing to set HTTP error response.");
    }
    else if (rc != IB_OK) {
        ib_log_error_tx(tx,
                        "Server failed to set HTTP error response: %s",
                        ib_status_to_string(rc));
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Initialize a value stack object
 *
 * @param[in] tx Transaction
 * @param[in,out] vs The value stack to initialize
 */
static void value_stack_init(ib_tx_t *tx,
                             value_stack_t *vs)
{
    IB_FTRACE_INIT();
    assert(tx != NULL);
    assert(vs != NULL);
    ib_status_t rc;

    rc = ib_list_create(&(vs->stack), tx->mp);
    if (rc != IB_OK) {
        vs->stack = NULL;
        ib_rule_log_warn(tx, NULL, NULL, NULL,
                         "Failed to create value stack: %s",
                         ib_status_to_string(rc));
    }
    IB_FTRACE_RET_VOID();
}

/**
 * Push a value onto a value stack object
 *
 * @param[in,out] vs The value stack to push onto
 * @param[in] value The value to push
 *
 * @returns true if the value was successfully pushed
 */
static bool value_stack_push(value_stack_t *vs,
                             const ib_field_t *value)
{
    IB_FTRACE_INIT();
    assert(vs != NULL);
    ib_status_t rc;

    if (vs->stack == NULL) {
        IB_FTRACE_RET_BOOL(false);
    }
    rc = ib_list_push(vs->stack, (ib_field_t *)value);
    if (rc != IB_OK) {
        ib_rule_log_warn(NULL, NULL, NULL, NULL,
                         "Failed to push value onto value stack: %s",
                         ib_status_to_string(rc));
        IB_FTRACE_RET_BOOL(false);
    }
    IB_FTRACE_RET_BOOL(true);
}

/**
 * Pop the value off a value stack object
 *
 * @param[in,out] vs The value stack to pop from
 * @param[in] pushed true if the value was successfully pushed
 *
 */
static void value_stack_pop(value_stack_t *vs,
                            bool pushed)
{
    IB_FTRACE_INIT();
    assert(vs != NULL);
    ib_status_t rc;

    if ( (! pushed) || (vs->stack == NULL) ) {
        IB_FTRACE_RET_VOID();
    }
    rc = ib_list_pop(vs->stack, NULL);
    if (rc != IB_OK) {
        ib_rule_log_warn(NULL, NULL, NULL, NULL,
                         "Failed to pop value from value stack: %s",
                         ib_status_to_string(rc));
    }
    IB_FTRACE_RET_VOID();
}

/**
 * Clear the target fields (FIELD, FIELD_NAME, FIELD_NAME_FULL)
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction to clear the target fields from
 *
 */
static void clear_target_fields(ib_engine_t *ib,
                                ib_tx_t *tx)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(tx != NULL);
    assert(tx->dpi != NULL);

    /* Create FIELD */
    ib_data_remove(tx->dpi, "FIELD", NULL);
    ib_data_remove(tx->dpi, "FIELD_TARGET", NULL);
    ib_data_remove(tx->dpi, "FIELD_TFN", NULL);
    ib_data_remove(tx->dpi, "FIELD_NAME", NULL);
    ib_data_remove(tx->dpi, "FIELD_NAME_FULL", NULL);

    IB_FTRACE_RET_VOID();
}

/**
 * Set the target fields (FIELD, FIELD_TFN, FIELD_NAME, FIELD_NAME_FULL)
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction to add the target fields to
 * @param[in] rule Rule being executed
 * @param[in] target Target data
 * @param[in] value_stack Stack of values
 * @param[in] transformed Transformed value
 *
 * @returns Status code
 */
static ib_status_t set_target_fields(ib_engine_t *ib,
                                     ib_tx_t *tx,
                                     const ib_rule_t *rule,
                                     const ib_rule_target_t *target,
                                     const value_stack_t *value_stack,
                                     const ib_field_t *transformed)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(tx->dpi != NULL);
    assert(rule != NULL);
    assert(value_stack != NULL);

    ib_field_t *f;
    ib_bytestr_t *bs;
    ib_status_t rc = IB_OK;
    ib_status_t trc;
    const ib_field_t *value;
    const ib_list_node_t *node;
    size_t namelen;
    size_t nameoff;
    int names;
    int n;
    char *name;

    if (value_stack->stack == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);       /* Do nothing for now */
    }

    /* The current value is the top of the stack */
    node = ib_list_last_const(value_stack->stack);
    if ( (node == NULL) || (node->data == NULL) ) {
        IB_FTRACE_RET_STATUS(IB_OK);       /* Do nothing for now */
    }
    value = (const ib_field_t *)node->data;

    /* Create FIELD */
    (void)ib_data_remove(tx->dpi, "FIELD", NULL);
    trc = ib_data_add_named(tx->dpi,
                            (ib_field_t *)value,
                            IB_FIELD_NAME("FIELD"));
    if (trc != IB_OK) {
        ib_rule_log_error(tx, rule, target, NULL,
                          "Failed to create FIELD: %s",
                          ib_status_to_string(trc));
        rc = trc;
    }

    /* Create FIELD_TFN */
    if (transformed != NULL) {
        (void)ib_data_remove(tx->dpi, "FIELD_TFN", NULL);
        trc = ib_data_add_named(tx->dpi,
                                (ib_field_t *)value,
                                IB_FIELD_NAME("FIELD_TFN"));
        if (trc != IB_OK) {
            ib_rule_log_error(tx, rule, target, NULL,
                              "Failed to create FIELD_TFN: %s",
                              ib_status_to_string(trc));
            rc = trc;
        }
    }

    /* Create FIELD_TARGET */
    if (target != NULL) {
        trc = ib_data_get(tx->dpi, "FIELD_TARGET", &f);
        if (trc == IB_ENOENT) {
            trc = ib_data_add_nulstr_ex(tx->dpi,
                                        IB_FIELD_NAME("FIELD_TARGET"),
                                        target->target_str,
                                        NULL);
        }
        else if (trc == IB_OK) {
            trc = ib_field_setv(f, ib_ftype_nulstr_in(target->target_str));
        }
        if (trc != IB_OK) {
            ib_rule_log_error(tx, rule, target, NULL,
                              "Failed to create FIELD_TARGET: %s",
                              ib_status_to_string(trc));
            rc = trc;
        }
    }

    /* Create FIELD_NAME */
    trc = ib_data_get(tx->dpi, "FIELD_NAME", &f);
    if (trc == IB_ENOENT) {
        trc = ib_data_add_bytestr_ex(tx->dpi,
                                     IB_FIELD_NAME("FIELD_NAME"),
                                     (uint8_t *)value->name,
                                     value->nlen,
                                     NULL);
    }
    else if (trc == IB_OK) {
        trc = ib_bytestr_dup_mem(&bs, tx->mp,
                                 (uint8_t *)value->name,
                                 value->nlen);
        if (trc == IB_OK) {
            trc = ib_field_setv(f, bs);
        }
    }
    if (trc != IB_OK) {
        ib_rule_log_error(tx, rule, target, NULL,
                          "Failed to create FIELD_NAME: %s",
                          ib_status_to_string(trc));
        rc = trc;
    }

    /* Create FIELD_NAME_FULL */

    /* Step 1: Calculate the buffer size & allocate */
    namelen = 0;
    names = 0;
    IB_LIST_LOOP_CONST(value_stack->stack, node) {
        if (node->data != NULL) {
            ++names;
            value = (const ib_field_t *)node->data;
            if (value->nlen > 0) {
                namelen += (value->nlen + 1);
            }
        }
    }
    /* Remove space for the final ":" */
    if (namelen > 0) {
        --namelen;
    }
    name = ib_mpool_alloc(tx->mp, namelen);
    if (name == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Step 2: Populate the name buffer. */
    nameoff = 0;
    n = 0;
    IB_LIST_LOOP_CONST(value_stack->stack, node) {
        if (node->data != NULL) {
            value = (const ib_field_t *)node->data;
            if (value->nlen > 0) {
                memcpy(name+nameoff, value->name, value->nlen);
                nameoff += value->nlen;
                ++n;
                if (n < names) {
                    *(name+nameoff) = ':';
                    ++nameoff;
                }
            }
        }
    }

    /* Step 3: Update the FIELD_NAME_FULL field. */
    trc = ib_data_get(tx->dpi, "FIELD_NAME_FULL", &f);
    if (trc == IB_ENOENT) {
        trc = ib_data_add_bytestr_ex(tx->dpi,
                                     IB_FIELD_NAME("FIELD_NAME_FULL"),
                                     (uint8_t *)name, namelen,
                                     NULL);
    }
    else if (trc == IB_OK) {
        trc = ib_bytestr_dup_mem(&bs, tx->mp, (uint8_t *)name, namelen);
        if (trc == IB_OK) {
            trc = ib_field_setv(f, bs);
        }
    }
    if (trc != IB_OK) {
        ib_rule_log_error(tx, rule, target, NULL,
                          "Failed to create FIELD_NAME_FULL: %s",
                          ib_status_to_string(trc));
        rc = trc;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Execute a rule on a list of values
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction
 * @param[in] rule Rule being executed
 * @param[in] target Target data
 * @param[in] opinst Operator instance
 * @param[in] value_stack Stack of values
 * @param[in] value Field value to operate on
 * @param[in] recursion Recursion limit -- won't recurse if recursion is zero
 * @param[in,out] rule_result Pointer to number in which to store the result
 * @param[in,out] log_exec Rule execution logging object
 *
 * @returns Status code
 */
static ib_status_t execute_operator(ib_engine_t *ib,
                                    ib_tx_t *tx,
                                    const ib_rule_t *rule,
                                    const ib_rule_target_t *target,
                                    const ib_operator_inst_t *opinst,
                                    value_stack_t *value_stack,
                                    const ib_field_t *value,
                                    ib_num_t recursion,
                                    ib_num_t *rule_result,
                                    ib_rule_log_exec_t *log_exec)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(rule != NULL);
    assert(opinst != NULL);

    ib_status_t rc;

    /* This if-block is only to log operator values when tracing. */
    if ( ib_rule_log_level(ib) >= IB_RULE_LOG_LEVEL_TRACE ) {
        if ( value == NULL ) {
            ib_rule_log_trace(tx,
                              rule,
                              target,
                              NULL,
                              "Exec of op %s on field %s = NULL",
                              opinst->op->name,
                              target->field_name);
        }
        else if ( value->type == IB_FTYPE_NUM ) {
            ib_num_t num;
            rc = ib_field_value(value, ib_ftype_num_out(&num));
            if ( rc != IB_OK ) {
                IB_FTRACE_RET_STATUS(rc);
            }
            ib_rule_log_trace(tx,
                              rule,
                              target,
                              NULL,
                              "Exec of op %s on field %s = %" PRId64,
                              opinst->op->name,
                              target->field_name,
                              num);
        }
        else if ( value->type == IB_FTYPE_UNUM ) {
            ib_unum_t unum;
            rc = ib_field_value(value, ib_ftype_unum_out(&unum));
            if ( rc != IB_OK ) {
                IB_FTRACE_RET_STATUS(rc);
            }
            ib_rule_log_trace(tx,
                              rule,
                              target,
                              NULL,
                              "Exec of op %s on field %s = %" PRIu64,
                              opinst->op->name,
                              target->field_name,
                              unum);
        }
        else if ( value->type == IB_FTYPE_NULSTR ) {
            const char* nulstr;
            rc = ib_field_value(value, ib_ftype_nulstr_out(&nulstr));

            if ( rc != IB_OK ) {
                IB_FTRACE_RET_STATUS(rc);
            }

            const char* escaped_value =
                ib_util_hex_escape(nulstr, strlen(nulstr));
            if ( escaped_value == NULL ) {
                IB_FTRACE_RET_STATUS(rc);
            }

            ib_rule_log_trace(tx,
                              rule,
                              target,
                              NULL,
                              "Exec of op %s on field %s = %s",
                              opinst->op->name,
                              target->field_name,
                              escaped_value);
            free((void *)escaped_value);
        }
        else if ( value->type == IB_FTYPE_BYTESTR ) {
            const char* escaped_value;
            const ib_bytestr_t *bytestr;

            rc = ib_field_value(value, ib_ftype_bytestr_out(&bytestr));
            if ( rc != IB_OK ) {
                IB_FTRACE_RET_STATUS(rc);
            }

            escaped_value = ib_util_hex_escape(
                (const char *)ib_bytestr_const_ptr(bytestr),
                ib_bytestr_size(bytestr));
            if ( escaped_value == NULL ) {
                IB_FTRACE_RET_STATUS(rc);
            }
            ib_rule_log_trace(tx,
                              rule,
                              target,
                              NULL,
                              "Exec of op %s on field %s = %s",
                              opinst->op->name,
                              target->field_name,
                              escaped_value);
            free((void *)escaped_value);
        }
        else {
            ib_rule_log_trace(tx,
                              rule,
                              target,
                              NULL,
                              "Exec of op %s on field %s = %s",
                              opinst->op->name,
                              target->field_name,
                              "[cannot decode field type]");
        }
    }

    /* Limit recursion */
    --recursion;
    if (recursion <= 0) {
        ib_rule_log_error(tx, rule, target, NULL,
                          "Rule engine: List recursion limit reached");
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    /* Handle a list by looping through it and recursively calling this func. */
    if ( (value != NULL) && (value->type == IB_FTYPE_LIST) ) {
        const ib_list_t      *vlist;
        const ib_list_node_t *node = NULL; /* Node in vlist. */
        int                   n    = 0;    /* Nth value in the vlist. */

        /* Fetch list out of value into vlist. */
        rc = ib_field_value(value, ib_ftype_list_out(&vlist));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Iterate over vlist. */
        IB_LIST_LOOP_CONST(vlist, node) {
            const ib_field_t *nvalue =
                (const ib_field_t *)ib_list_node_data_const(node);
            bool pushed;

            ++n;
            pushed = value_stack_push(value_stack, nvalue);

            /* Recursive call. */
            rc = execute_operator(ib, tx, rule, target, opinst, value_stack,
                                  nvalue, recursion,
                                  rule_result, log_exec);
            if (rc != IB_OK) {
                ib_rule_log_warn(tx, rule, target, NULL,
                                 "Error executing list element #%d: %s",
                                 n, ib_status_to_string(rc));
            }
            value_stack_pop(value_stack, pushed);
        }

        ib_rule_log_debug(tx, NULL, target, NULL,
                          "Operator (list %zd) => %" PRId64,
                          vlist->nelts, *rule_result);
    }

    /* No recursion required, handle it here */
    else {
        ib_list_t  *actions;
        ib_num_t    result = 0;
        ib_num_t    trc;

        /* Fill in the FIELD* fields */
        rc = set_target_fields(ib, tx, rule, target, value_stack, value);
        if (rc != IB_OK) {
            ib_rule_log_error(tx, rule, NULL, NULL,
                              "Error creating one or more FIELD* fields: %s",
                              ib_status_to_string(rc));
        }

        /* Execute the operator */
        /* @todo remove the cast-away of the constness of value */
        rc = ib_operator_execute(ib, tx, rule, opinst,
                                 (ib_field_t *)value, &result);
        if (rc != IB_OK) {
            ib_rule_log_warn(tx, rule, target, NULL,
                             "Operator returned an error: %s",
                             ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Store the result */
        if (result != 0) {
            *rule_result = result;
        }

        /*
         * Execute the actions.
         *
         * @todo The current behavior is to keep running even after action(s)
         * returns an error.  This needs further discussion to determine what
         * the correct behavior should be.
         */
        if ( (opinst->flags & IB_OPINST_FLAG_INVERT) != 0) {
            result = (result == 0);
        }
        if (result != 0) {
            actions = rule->true_actions;
        }
        else {
            actions = rule->false_actions;
        }

        ib_rule_log_exec_add_result(log_exec, value, result, actions);
        trc = execute_action_list(ib, rule, tx, result, actions);

        /* Done. */
        clear_target_fields(ib, tx);

        if (trc != IB_OK) {
            ib_rule_log_error(tx, rule, NULL, NULL,
                              "Failed to execute action for rule");
            rc = trc;
        }
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Execute a single rule's operator on all target fields.
 *
 * @param[in] ib Engine
 * @param[in] rule Rule to execute
 * @param[in,out] tx Transaction
 * @param[in,out] rule_result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t execute_phase_rule_targets(ib_engine_t *ib,
                                              ib_rule_t *rule,
                                              ib_tx_t *tx,
                                              ib_num_t *rule_result)
{
    IB_FTRACE_INIT();
    assert(ib);
    assert(rule);
    assert(tx);
    assert(rule_result);

    ib_list_node_t     *node = NULL;
    ib_operator_inst_t *opinst = rule->opinst;
    ib_status_t         rc = IB_OK;
    value_stack_t       value_stack;
    ib_rule_log_exec_t *log_exec = NULL;

    /* Special case: External rules */
    if (ib_flags_all(rule->flags, IB_RULE_FLAG_EXTERNAL)) {

        /* Execute the operator */
        ib_rule_log_debug(tx, rule, NULL, NULL, "Executing external rule");
        rc = ib_operator_execute(ib, tx, rule, opinst, NULL, rule_result);
        if (rc != IB_OK) {
            ib_rule_log_error(tx, rule, NULL, NULL,
                              "External operator returned an error: %s",
                              ib_status_to_string(rc));
        }
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Log what we're going to do */
    ib_rule_log_debug(tx, rule, NULL, NULL, "Executing rule");

    /* Initialize the value stack */
    value_stack_init(tx, &value_stack);

    /* Create a new execution logging object */
    rc = ib_rule_log_exec_create(tx, rule, &log_exec);
    if (rc != IB_OK) {
        ib_rule_log_error(tx, rule, NULL, NULL,
                          "Rule engine: Failed to create log object: %s",
                          ib_status_to_string(rc));
        rc = IB_OK;
    }


    /* If this is a no-target rule (i.e. action), do nothing */
    if (ib_flags_all(rule->flags, IB_RULE_FLAG_NO_TGT)) {
        assert(ib_list_elements(rule->target_fields) == 1);
    }
    else {
        assert(ib_list_elements(rule->target_fields) != 0);
    }

    ib_rule_log_debug(tx, rule, NULL, NULL, "Operating on %zd fields.",
                      ib_list_elements(rule->target_fields));

    /*
     * Loop through all of the fields.
     *
     * @todo The current behavior is to keep running even after an operator
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    IB_LIST_LOOP(rule->target_fields, node) {
        ib_rule_target_t   *target = (ib_rule_target_t *)node->data;
        assert(target != NULL);
        const char         *fname = target->field_name;
        assert(fname != NULL);
        ib_field_t         *value = NULL;      /* Value from the DPI */
        ib_field_t         *tfnvalue = NULL;   /* Value after tfns */
        ib_status_t         getrc;             /* Status from ib_data_get() */
        ib_num_t            target_result = 0; /* Result of this target */
        bool                pushed = true;

        /* Get the field value */
        getrc = ib_data_get(tx->dpi, fname, &value);
        if (getrc == IB_ENOENT) {
            bool allow  =
                ib_flags_all(opinst->op->flags, IB_OP_FLAG_ALLOW_NULL);
            ib_rule_log_exec_add_tgt(log_exec, target, NULL);

            if (! allow) {
                ib_rule_log_debug(tx,
                                  rule,
                                  target,
                                  NULL,
                                  "Operator %s will not execute because "
                                  "there is no field %s.",
                                  opinst->op->name,
                                  fname);
                continue;
            }

            ib_rule_log_debug(tx,
                              rule,
                              target,
                              NULL,
                              "Operator %s receiving null argument because "
                              "there is no field %s.",
                              opinst->op->name,
                              fname);
        }
        else if (getrc != IB_OK) {
            ib_rule_log_error(tx, rule, target, NULL,
                              "Error getting target field: %s",
                              ib_status_to_string(rc));
            continue;
        }

        /* Add a target execution result to the log object */
        ib_rule_log_exec_add_tgt(log_exec, target, value);

        /* Execute the target transformations */
        if (value != NULL) {
            rc = execute_tfns(ib, tx, rule, target,
                              value, &tfnvalue, log_exec);
            if (rc != IB_OK) {
                ib_rule_log_error(tx, rule, target, NULL,
                                  "Error executing transformations : %s",
                                  ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }
        }

        /* Store the rule's final value */
        ib_rule_log_exec_set_tgt_final(log_exec, tfnvalue);

        /* Put the value on the value stack */
        pushed = value_stack_push(&value_stack, value);

        /* Execute the rule operator on the value. */
        if ( (tfnvalue != NULL) && (tfnvalue->type == IB_FTYPE_LIST) ) {
            ib_list_t *value_list;
            ib_list_node_t *value_node;
            rc = ib_field_value(tfnvalue, (void *)&value_list);

            if (rc != IB_OK) {
                ib_rule_log_error(tx, rule, target, NULL,
                                  "Error getting target field value: %s",
                                  ib_status_to_string(rc));
                continue;
            }

            /* Log when there are no arguments. */
            if ( ib_list_elements(value_list) == 0 &&
                 ib_rule_log_level(ib) >= IB_RULE_LOG_LEVEL_TRACE ) {
                ib_rule_log_trace(tx,
                                  rule,
                                  target,
                                  NULL,
                                  "Rule not running because there are no "
                                  "values for operator %s "
                                  "to operate on in field %s.",
                                  opinst->op->name,
                                  fname);
            }

            /* Run operations on each list element. */
            IB_LIST_LOOP(value_list, value_node) {
                ib_num_t result = 0;
                ib_field_t *node_value = (ib_field_t *)value_node->data;
                bool lpushed;

                lpushed = value_stack_push(&value_stack, node_value);


                rc = execute_operator(ib,
                                      tx,
                                      rule,
                                      target,
                                      opinst,
                                      &value_stack,
                                      node_value,
                                      MAX_LIST_RECURSION,
                                      &result,
                                      log_exec);
                if (rc != IB_OK) {
                    ib_rule_log_error(tx, rule, target, NULL,
                                      "Operator returned an error: %s",
                                      ib_status_to_string(rc));
                    IB_FTRACE_RET_STATUS(rc);
                }
                ib_rule_log_debug(tx, rule, target, NULL,
                                  "Operator result => %" PRId64,
                                  result);

                /* Store the result */
                if (result != 0) {
                    target_result = result;
                }
                value_stack_pop(&value_stack, lpushed);
            }
        }
        else {
            ib_rule_log_debug(tx, rule, target, NULL,
                              "calling exop on single target");
            rc = execute_operator(ib,
                                  tx,
                                  rule,
                                  target,
                                  opinst,
                                  &value_stack,
                                  tfnvalue,
                                  MAX_LIST_RECURSION,
                                  &target_result,
                                  log_exec);
            if (rc != IB_OK) {
                ib_rule_log_error(tx, rule, target, NULL,
                                  "Operator returned an error: %s",
                                  ib_status_to_string(rc));

                /* Clean up the value stack before we return */
                value_stack_pop(&value_stack, pushed);
                IB_FTRACE_RET_STATUS(rc);
            }

            /* Log it */
            ib_rule_log_debug(tx, rule, target, NULL,
                              "Operator result => %" PRId64,
                              target_result);
        }

        /* Store the result */
        if (target_result != 0) {
            *rule_result = target_result;
        }

        /* Pop this element off the value stack */
        value_stack_pop(&value_stack, pushed);
    }

    /* Invert? */
    /* done: */
    if ( (opinst->flags & IB_OPINST_FLAG_INVERT) != 0) {
        *rule_result = (*rule_result == 0);
    }

    ib_rule_log_exec(log_exec);

    ib_rule_log_debug(tx, rule, NULL, NULL,
                      "Rule operator => %" PRId64, *rule_result);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Execute a single phase rule, it's actions, and it's chained rules.
 *
 * @param[in] ib Engine
 * @param[in] rule Rule to execute
 * @param[in,out] tx Transaction
 * @param[in] recursion Recursion limit
 * @param[in,out] rule_result Result of rule execution
 *
 * @returns Status code
 */
static ib_status_t execute_phase_rule(ib_engine_t *ib,
                                      ib_rule_t *rule,
                                      ib_tx_t *tx,
                                      ib_num_t recursion,
                                      ib_num_t *rule_result)
{
    IB_FTRACE_INIT();
    ib_status_t         rc = IB_OK;
    ib_status_t         trc;          /* Temporary status code */

    assert(ib != NULL);
    assert(rule != NULL);
    assert(! rule->phase_meta->is_stream);
    assert(tx != NULL);
    assert(rule_result != NULL);

    /* Limit recursion */
    --recursion;
    if (recursion <= 0) {
        ib_rule_log_error(tx, rule, NULL, NULL,
                          "Rule engine: Phase chain recursion limit reached");
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    /* Initialize the rule result */
    *rule_result = 0;

    /*
     * Execute the rule operator on the target fields.
     *
     * @todo The current behavior is to keep running even after an operator
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    trc = execute_phase_rule_targets(ib, rule, tx, rule_result);
    if (trc != IB_OK) {
        ib_rule_log_error(tx, rule, NULL, NULL,
                          "Error executing rule: %s",
                          ib_status_to_string(trc));
        rc = trc;
    }

    /*
     * Execute chained rule
     *
     * @todo The current behavior is to keep running even after a chained rule
     * returns an error.  This needs further discussion to determine what
     * the correct behavior should be.
     *
     * @note Chaining is currently done via recursion.
     */
    if ( (*rule_result != 0) && (rule->chained_rule != NULL) ) {
        ib_rule_log_debug(tx, rule, NULL, NULL,
                          "Chaining to rule \"%s\"",
                          ib_rule_id(rule->chained_rule));
        trc = execute_phase_rule(ib,
                                 rule->chained_rule,
                                 tx,
                                 recursion,
                                 rule_result);

        if (trc != IB_OK) {
            ib_rule_log_error(tx, rule, NULL, NULL,
                              "Error executing chained rule \"%s\": %s",
                              ib_rule_id(rule->chained_rule),
                              ib_status_to_string(trc));
            rc = trc;
        }
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Check if the current rule is runnable
 *
 * @param[in] ctx_rule Context rule data
 * @param[in] rule Rule to check
 *
 * @returns true if the rule is runnable, otherwise false
 */
static bool rule_is_runnable(const ib_rule_ctx_data_t *ctx_rule,
                             const ib_rule_t *rule)
{
    IB_FTRACE_INIT();

    /* Skip invalid / disabled rules */
    if (! ib_flags_all(ctx_rule->flags, IB_RULECTX_FLAG_ENABLED)) {
        IB_FTRACE_RET_UINT(false);
    }
    if (! ib_flags_all(rule->flags, IB_RULE_FLAG_VALID)) {
        IB_FTRACE_RET_UINT(false);
    }

    IB_FTRACE_RET_UINT(true);
}

/**
 * Check if allow affects the current rule
 *
 * @param[in] tx Transaction
 * @param[in] meta Rule's phase meta-data
 * @param[in] rule Rule to check (or NULL)
 * @param[in] check_phase Check if ALLOW_PHASE is set
 *
 * @returns true if the rule is affected, otherwise false
 */
static bool rule_allow(const ib_tx_t *tx,
                       const ib_rule_phase_meta_t *meta,
                       const ib_rule_t *rule,
                       bool check_phase)
{
    IB_FTRACE_INIT();

    /* Check the ALLOW_ALL flag */
    if ( (meta->phase_num != PHASE_POSTPROCESS) &&
         (ib_tx_flags_isset(tx, IB_TX_ALLOW_ALL) == 1) )
    {
        ib_rule_log_debug(tx, rule, NULL, NULL,
                          "Skipping phase %d/\"%s\" in context \"%s\": "
                          "ALLOW set",
                          meta->phase_num, meta->name,
                          ib_context_full_get(tx->ctx));
        IB_FTRACE_RET_BOOL(true);
    }

    /* If this is a request phase rule, Check the ALLOW_REQUEST flag */
    if ( (ib_flags_all(meta->flags, PHASE_FLAG_REQUEST)) &&
         (ib_tx_flags_isset(tx, IB_TX_ALLOW_REQUEST) == 1) )
    {
        ib_rule_log_debug(tx, rule, NULL, NULL,
                          "Skipping phase %d/\"%s\" in context \"%s\": "
                          "ALLOW_REQUEST set",
                          meta->phase_num, meta->name,
                          ib_context_full_get(tx->ctx));
        IB_FTRACE_RET_BOOL(true);
    }

    /* If check_phase is true, check the ALLOW_PHASE flag */
    if ( check_phase &&
         (tx->allow_phase == meta->phase_num) &&
         (ib_tx_flags_isset(tx, IB_TX_ALLOW_PHASE) == 1) )
    {
        ib_rule_log_debug(tx, rule, NULL, NULL,
                          "Skipping remaining rules phase %d/\"%s\" "
                          "in context \"%s\": ALLOW_PHASE set",
                          meta->phase_num, meta->name,
                          ib_context_full_get(tx->ctx));
        IB_FTRACE_RET_BOOL(true);
    }

    IB_FTRACE_RET_BOOL(false);
}

/**
 * Run a set of phase rules.
 *
 * @param[in] ib Engine.
 * @param[in,out] tx Transaction.
 * @param[in] event Event type.
 * @param[in] cbdata Callback data (actually phase_rule_cbdata_t).
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_DECLINED if the httpd plugin declines to block the traffic.
 */
static ib_status_t run_phase_rules(ib_engine_t *ib,
                                   ib_tx_t *tx,
                                   ib_state_event_type_t event,
                                   void *cbdata)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(tx != NULL);
    assert(tx->ctx != NULL);
    assert(cbdata != NULL);

    const ib_rule_phase_meta_t *meta = (const ib_rule_phase_meta_t *) cbdata;
    ib_context_t               *ctx = tx->ctx;
    const ib_ruleset_phase_t   *ruleset_phase;
    ib_list_t                  *rules;
    ib_list_node_t             *node = NULL;
    ib_status_t                rc;

    ruleset_phase = &(ctx->rules->ruleset.phases[meta->phase_num]);
    assert(ruleset_phase != NULL);
    rules = ruleset_phase->rule_list;
    assert(rules != NULL);

    /* Allow (skip) this phase? */
    if (rule_allow(tx, meta, NULL, false)) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Clear the phase allow flag */
    ib_flags_clear(tx->flags, IB_TX_ALLOW_PHASE);
    tx->allow_phase = PHASE_NONE;

    /* If we're blocking, skip processing */
    if (ib_tx_flags_isset(tx, IB_TX_BLOCK_PHASE | IB_TX_BLOCK_IMMEDIATE) &&
        (ib_flags_any(meta->flags, PHASE_FLAG_FORCE) == false) )
    {
        ib_rule_log_debug(tx, NULL, NULL, NULL,
                          "Not executing rules for phase %d/\"%s\" "
                          "in context \"%s\" because transaction previously "
                          "has been blocked with status %d",
                          meta->phase_num, meta->name,
                          ib_context_full_get(ctx), tx->block_status);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Sanity check */
    if (ruleset_phase->phase_num != meta->phase_num) {
        ib_rule_log_error(tx, NULL, NULL, NULL,
                          "Rule engine: Phase %d/\"%s\" is %d",
                          meta->phase_num, meta->name,
                          ruleset_phase->phase_num);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Walk through the rules & execute them */
    if (IB_LIST_ELEMENTS(rules) == 0) {
        ib_rule_log_debug(tx, NULL, NULL, NULL,
                          "No rules for phase %d/\"%s\" in context \"%s\"",
                          meta->phase_num, meta->name,
                          ib_context_full_get(ctx));
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    ib_rule_log_debug(tx, NULL, NULL, NULL,
                      "Executing %zd rules for phase %d/\"%s\" "
                      "in context \"%s\"",
                      IB_LIST_ELEMENTS(rules),
                      meta->phase_num, meta->name, ib_context_full_get(ctx));

    /*
     * Loop through all of the rules for this phase, execute them.
     *
     * @todo The current behavior is to keep running even after rule execution
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    IB_LIST_LOOP(rules, node) {
        ib_rule_ctx_data_t *ctx_rule = (ib_rule_ctx_data_t *)node->data;
        ib_rule_t          *rule;
        ib_num_t            rule_result = 0;
        ib_status_t         rule_rc;

        /* Skip invalid / disabled rules */
        rule = ctx_rule->rule;
        if (! rule_is_runnable(ctx_rule, rule)) {
            ib_rule_log_debug(tx, rule, NULL, NULL,
                              "Not executing invalid/disabled phase rule");
            continue;
        }

        /* Allow (skip) this phase? */
        if (rule_allow(tx, meta, rule, true)) {
            break;
        }

        /* Execute the rule, it's actions and chains */
        rule_rc = execute_phase_rule(ib,
                                     rule,
                                     tx,
                                     MAX_CHAIN_RECURSION,
                                     &rule_result);

        if (rule_rc != IB_OK) {
            ib_rule_log_error(tx, rule, NULL, NULL,
                              "Error executing rule: %s",
                              ib_status_to_string(rule_rc));
        }

        /* Handle declined return code. Did this block? */
        if ( ib_tx_flags_isset( tx, IB_TX_BLOCK_IMMEDIATE ) ) {
            ib_rule_log_debug(tx, rule, NULL, NULL,
                              "Rule resulted in immediate block: %s",
                              ib_status_to_string(rule_rc));
            ib_rule_log_debug(tx, rule, NULL, NULL,
                              "Rule processing is aborted by "
                              "immediate block.");
            rule_rc = report_block_to_server(ib, tx);
            if ( rule_rc != IB_OK ) {
                ib_rule_log_error(tx, rule, NULL, NULL, "Failed to block.");
            }
            else {
                IB_FTRACE_RET_STATUS(IB_OK);
            }
        }
    }

    if ( ib_tx_flags_isset(tx, IB_TX_BLOCK_PHASE ) ) {
        ib_log_debug_tx(tx, "Rule resulted in phase block");
        rc = report_block_to_server(ib, tx);
        if ( rc != IB_OK ) {
            ib_log_error_tx(tx,
                            "Failed to block phase: %s",
                            ib_status_to_string(rc));
        }
    }

    /*
     * @todo Eat errors for now.  Unless something Really Bad(TM) has
     * occurred, return IB_OK to the engine.  A bigger discussion of if / how
     * such errors should be propagated needs to occur.
     */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute a single stream operator, and it's actions
 *
 * @param[in] ib Engine
 * @param[in] rule Rule to execute
 * @param[in] tx Transaction
 * @param[in] value Value to pass to the operator
 * @param[in] value_stack Value stack for logging
 * @param[in] result Result from the operator
 * @param[in,out] log_exec Rule execution log object
 *
 * @returns Status code
 */
static ib_status_t execute_stream_operator(ib_engine_t *ib,
                                           ib_tx_t *tx,
                                           const ib_rule_t *rule,
                                           ib_field_t *value,
                                           value_stack_t *value_stack,
                                           ib_num_t *result,
                                           ib_rule_log_exec_t *log_exec)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(rule != NULL);
    assert(value != NULL);
    assert(result != NULL);

    ib_status_t      rc;
    const ib_list_t *actions;
    bool             pushed = value_stack_push(value_stack, value);

    /* Add a target execution result to the log object */
    ib_rule_log_exec_add_stream_tgt(log_exec, value);

    /* Fill in the FIELD* fields */
    rc = set_target_fields(ib, tx, rule, NULL, value_stack, value);
    if (rc != IB_OK) {
        ib_rule_log_error(tx, rule, NULL, NULL,
                          "Error creating one or more FIELD* fields: %s",
                          ib_status_to_string(rc));
    }

    /* Execute the rule operator */
    rc = ib_operator_execute(ib, tx, rule, rule->opinst, value, result);
    if (rc != IB_OK) {
        ib_rule_log_error(tx, rule, NULL, NULL,
                          "Operator returned an error: %s",
                          ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_rule_log_debug(tx, rule, NULL, NULL, "Operator => %" PRId64, *result);

    value_stack_pop(value_stack, pushed);

    /* Invert? */
    if ( (rule->opinst->flags & IB_OPINST_FLAG_INVERT) != 0) {
        *result = (*result == 0);
    }

    /*
     * Execute the actions.
     *
     * @todo The current behavior is to keep running even after action(s)
     * returns an error.  This needs further discussion to determine what
     * the correct behavior should be.
     */
    actions = (*result != 0) ? rule->true_actions : rule->false_actions;

    ib_rule_log_exec_add_result(log_exec, value, *result, actions);

    rc = execute_action_list(ib, rule, tx, *result, actions);

    if (rc != IB_OK) {
        ib_rule_log_error(tx, rule, NULL, NULL,
                          "Error executing action for rule: %s",
                          ib_status_to_string(rc));
    }

    if ( ib_tx_flags_isset( tx, IB_TX_BLOCK_IMMEDIATE ) ) {
        ib_rule_log_debug(tx, rule, NULL, NULL,
                          "Rule resulted in immediate block: %s",
                          ib_status_to_string(rc));
        ib_rule_log_debug(tx, rule, NULL, NULL,
                          "Rule processing is aborted by "
                          "immediate block.");
        report_block_to_server(ib, tx);
    }

    ib_rule_log_exec(log_exec);
    clear_target_fields(ib, tx);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Execute a single stream txdata rule, and it's actions
 *
 * @param[in] ib Engine
 * @param[in] rule Rule to execute
 * @param[in] tx Transaction
 * @param[in,out] txdata Transaction data
 * @param[in,out] log_exec Rule execution log object
 *
 * @returns Status code
 */
static ib_status_t execute_stream_txdata_rule(ib_engine_t *ib,
                                              ib_tx_t *tx,
                                              const ib_rule_t *rule,
                                              ib_txdata_t *txdata,
                                              ib_rule_log_exec_t *log_exec)
{
    IB_FTRACE_INIT();
    ib_status_t    rc = IB_OK;
    ib_field_t    *value = NULL;
    value_stack_t  value_stack;
    ib_num_t       result = 0;

    assert(ib != NULL);
    assert(rule != NULL);
    assert(txdata != NULL);
    assert(rule->phase_meta->is_stream);


    /*
     * Execute the rule operator.
     *
     * @todo The current behavior is to keep running even after action(s)
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */

    /* Create a field to hold the data */
    rc = ib_field_create_bytestr_alias(&value,
                                       tx->mp,
                                       "txdata", 3,
                                       txdata->data, txdata->dlen);
    if (rc != IB_OK) {
        ib_rule_log_error(tx, rule, NULL, NULL,
                          "Error creating field for stream "
                          "txdata rule data: %s",
                          ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Initialize the value stack */
    value_stack_init(tx, &value_stack);

    rc = execute_stream_operator(ib, tx, rule, value, &value_stack,
                                 &result, log_exec);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Execute a single stream header rule, and it's actions
 *
 * @param[in] ib Engine
 * @param[in] rule Rule to execute
 * @param[in] tx Transaction
 * @param[in] header Parsed header
 * @param[in,out] log_exec Rule execution log object
 *
 * @returns Status code
 */
static ib_status_t execute_stream_header_rule(ib_engine_t *ib,
                                              ib_tx_t *tx,
                                              const ib_rule_t *rule,
                                              ib_parsed_header_t *header,
                                              ib_rule_log_exec_t *log_exec)
{
    IB_FTRACE_INIT();
    ib_status_t          rc = IB_OK;
    ib_field_t          *value;
    ib_parsed_name_value_pair_list_t *nvpair;
    value_stack_t        value_stack;
    ib_num_t             rule_result = 0;

    assert(ib != NULL);
    assert(rule != NULL);
    assert(header != NULL);
    assert(rule->phase_meta->is_stream);

    /* Initialize the value stack */
    value_stack_init(tx, &value_stack);

    /*
     * Execute the rule operator.
     *
     * @todo The current behavior is to keep running even after action(s)
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    for (nvpair = header;  nvpair != NULL;  nvpair = nvpair->next) {
        ib_num_t result = 0;

        /* Create a field to hold the data */
        ib_rule_log_debug(tx, rule, NULL, NULL,
                          "nvpair: \"%.*s\"=\"%.*s\"\n",
                          (int)ib_bytestr_length(nvpair->name),
                          (const char *)ib_bytestr_const_ptr(nvpair->name),
                          (int)ib_bytestr_length(nvpair->value),
                          (const char *)ib_bytestr_const_ptr(nvpair->value));
        rc = ib_field_create(&value,
                             tx->mp,
                             (const char *)ib_bytestr_const_ptr(nvpair->name),
                             ib_bytestr_length(nvpair->name),
                             IB_FTYPE_BYTESTR,
                             ib_ftype_bytestr_in(nvpair->value));
        if (rc != IB_OK) {
            ib_rule_log_error(tx, rule, NULL, NULL,
                              "Error creating field for "
                              "header stream rule data: %s",
                              ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        rc = execute_stream_operator(ib, tx, rule, value, &value_stack,
                                     &result, log_exec);

        /* Store the result */
        if (result != 0) {
            rule_result = result;
        }
    }
    ib_log_debug3_tx(tx, "Operator \"%s\" => %" PRId64,
                     rule->opinst->op->name, rule_result);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Run a set of stream rules.
 *
 * @param[in] ib Engine.
 * @param[in] tx Transaction.
 * @param[in] event Event type.
 * @param[in] txdata Transaction data (or NULL)
 * @param[in] header Parsed header (or NULL)
 * @param[in] meta Phase meta data
 *
 * @returns
 *   - Status code IB_OK
 */
static ib_status_t run_stream_rules(ib_engine_t *ib,
                                    ib_tx_t *tx,
                                    ib_state_event_type_t event,
                                    ib_txdata_t *txdata,
                                    ib_parsed_header_t *header,
                                    const ib_rule_phase_meta_t *meta)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert( (txdata != NULL) || (header != NULL) );
    assert(meta != NULL);
    assert( (meta->hook_type != IB_STATE_HOOK_TXDATA) || (txdata != NULL) );
    assert( (meta->hook_type != IB_STATE_HOOK_HEADER) || (header != NULL) );

    ib_context_t             *ctx = tx->ctx;
    const ib_ruleset_phase_t *ruleset_phase =
        &(ctx->rules->ruleset.phases[meta->phase_num]);
    ib_list_t                *rules = ruleset_phase->rule_list;
    const ib_list_node_t     *node = NULL;

    /* Allow (skip) this phase? */
    if (rule_allow(tx, meta, NULL, false)) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Clear the phase allow flag if we're in a new phase */
    if ( (ib_tx_flags_isset(tx, IB_TX_ALLOW_PHASE) == 1) &&
         (tx->allow_phase != meta->phase_num) ) {
        ib_flags_clear(tx->flags, IB_TX_ALLOW_PHASE);
        tx->allow_phase = PHASE_NONE;
    }

    /* Sanity check */
    if (ruleset_phase->phase_num != meta->phase_num) {
        ib_rule_log_error(tx, NULL, NULL, NULL,
                          "Rule engine: Stream %d/\"%s\" is %d",
                          meta->phase_num, meta->name,
                          ruleset_phase->phase_num);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Are there any rules?  If not, do a quick exit */
    if (IB_LIST_ELEMENTS(rules) == 0) {
        ib_rule_log_debug(tx, NULL, NULL, NULL,
                          "No rules for stream %d/\"%s\" in context \"%s\"",
                          meta->phase_num, meta->name,
                          ib_context_full_get(ctx));
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    ib_rule_log_debug(tx, NULL, NULL, NULL,
                      "Executing %zd rules for stream %d/\"%s\" "
                      "in context \"%s\"",
                      IB_LIST_ELEMENTS(rules),
                      meta->phase_num, meta->name, ib_context_full_get(ctx));

    /*
     * Loop through all of the rules for this phase, execute them.
     *
     * @todo The current behavior is to keep running even after rule execution
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    IB_LIST_LOOP_CONST(rules, node) {
        const ib_rule_ctx_data_t *ctx_rule =
            (const ib_rule_ctx_data_t *)node->data;
        const ib_rule_t    *rule;
        ib_status_t         rc = IB_OK;
        ib_status_t         trc;
        ib_rule_log_exec_t *log_exec = NULL;

        /* Skip invalid / disabled rules */
        rule = ctx_rule->rule;
        if (! rule_is_runnable(ctx_rule, rule)) {
            ib_rule_log_debug(tx, rule, NULL, NULL,
                              "Not executing invalid/disabled stream rule");
            continue;
        }

        /* Allow (skip) this phase? */
        if (rule_allow(tx, meta, rule, true)) {
            break;
        }

        /* Create the execution logging object */
        trc = ib_rule_log_exec_create(tx, rule, &log_exec);
        if (trc != IB_OK) {
            ib_rule_log_error(tx, rule, NULL, NULL,
                              "Rule engine: Failed to create log object: %s",
                              ib_status_to_string(trc));
        }

        /*
         * Execute the rule
         *
         * @todo The current behavior is to keep running even after an
         * operator returns an error.  This needs further discussion to
         * determine what the correct behavior should be.
         */
        if (txdata != NULL) {
            rc = execute_stream_txdata_rule(ib, tx, rule, txdata, log_exec);
        }
        else if (header != NULL) {
            rc = execute_stream_header_rule(ib, tx, rule, header, log_exec);
        }
        if (rc != IB_OK) {
            ib_rule_log_error(tx, rule, NULL, NULL,
                              "Error executing rule: %s",
                              ib_status_to_string(rc));
        }

        ib_rule_log_exec(log_exec);
    }

    if ( ib_tx_flags_isset(tx, IB_TX_BLOCK_PHASE) ) {
        report_block_to_server(ib, tx);
    }

    /*
     * @todo Eat errors for now.  Unless something Really Bad(TM) has
     * occurred, return IB_OK to the engine.  A bigger discussion of if / how
     * such errors should be propagated needs to occur.
     */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Run stream header rules
 *
 * @param[in] ib Engine.
 * @param[in] tx Transaction.
 * @param[in] event Event type.
 * @param[in] header Parsed header
 * @param[in] cbdata Callback data (actually phase_rule_cbdata_t)
 *
 * @returns Status code
 */
static ib_status_t run_stream_header_rules(ib_engine_t *ib,
                                           ib_tx_t *tx,
                                           ib_state_event_type_t event,
                                           ib_parsed_header_t *header,
                                           void *cbdata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(header != NULL);
    assert(cbdata != NULL);

    const ib_rule_phase_meta_t *meta = (const ib_rule_phase_meta_t *) cbdata;
    ib_status_t rc;
    rc = run_stream_rules(ib, tx, event, NULL, header, meta);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Run stream TXDATA rules
 *
 * @param[in] ib Engine.
 * @param[in] tx Transaction.
 * @param[in] event Event type.
 * @param[in] txdata Transaction data.
 * @param[in] cbdata Callback data (actually phase_rule_cbdata_t)
 *
 * @returns Status code
 */
static ib_status_t run_stream_txdata_rules(ib_engine_t *ib,
                                           ib_tx_t *tx,
                                           ib_state_event_type_t event,
                                           ib_txdata_t *txdata,
                                           void *cbdata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(cbdata != NULL);

    if (txdata == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    const ib_rule_phase_meta_t *meta = (const ib_rule_phase_meta_t *) cbdata;
    ib_status_t rc;
    rc = run_stream_rules(ib, tx, event, txdata, NULL, meta);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Run stream TX rules (aka request header)
 *
 * @param[in] ib Engine.
 * @param[in] tx Transaction.
 * @param[in] event Event type.
 * @param[in] cbdata Callback data (actually phase_rule_cbdata_t)
 *
 * @returns Status code
 */
static ib_status_t run_stream_tx_rules(ib_engine_t *ib,
                                       ib_tx_t *tx,
                                       ib_state_event_type_t event,
                                       void *cbdata)
{
    IB_FTRACE_INIT();
    ib_parsed_header_wrapper_t *hdrs;
    ib_status_t rc;

    assert(ib != NULL);
    assert(tx != NULL);
    assert(cbdata != NULL);

    /* Wrap up the request line */
    rc = ib_parsed_name_value_pair_list_wrapper_create(&hdrs, tx);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Error creating name/value pair list: %s",
                        ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    if ( (tx->request_line->method != NULL) &&
         (ib_bytestr_const_ptr(tx->request_line->method) != NULL) )
    {
        rc = ib_parsed_name_value_pair_list_add(
            hdrs,
            "method", 6,
            (const char *)ib_bytestr_const_ptr(tx->request_line->method),
            ib_bytestr_length(tx->request_line->method));
        if (rc != IB_OK) {
            ib_rule_log_error(tx, NULL, NULL, NULL,
                              "Error adding method to name/value pair list: %s",
                              ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    if ( (tx->request_line->uri != NULL) &&
         (ib_bytestr_const_ptr(tx->request_line->uri) != NULL) )
    {
        rc = ib_parsed_name_value_pair_list_add(
            hdrs,
            "uri", 3,
            (const char *)ib_bytestr_const_ptr(tx->request_line->uri),
            ib_bytestr_length(tx->request_line->uri));
        if (rc != IB_OK) {
            ib_rule_log_error(tx, NULL, NULL, NULL,
                              "Error adding uri to name/value pair list: %s",
                              ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    if ( (tx->request_line->protocol != NULL) &&
         (ib_bytestr_const_ptr(tx->request_line->protocol) != NULL) )
    {
        rc = ib_parsed_name_value_pair_list_add(
            hdrs,
            "protocol", 8,
            (const char *)ib_bytestr_const_ptr(tx->request_line->protocol),
            ib_bytestr_length(tx->request_line->protocol));
        if (rc != IB_OK) {
            ib_rule_log_error(tx, NULL, NULL, NULL,
                              "Error adding protocol to name/value "
                              "pair list: %s",
                              ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Now, process the request line */
    if ( (hdrs != NULL) && (hdrs->head != NULL) ) {
        ib_rule_log_debug(tx, NULL, NULL, NULL,
                          "Running header line through stream header");
        rc = run_stream_header_rules(ib, tx, event,
                                     hdrs->head, cbdata);
        if (rc != IB_OK) {
            ib_rule_log_error(tx, NULL, NULL, NULL,
                              "Error processing tx request line: %s",
                              ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Process the request header */
    if ( (tx->request_header != NULL) && (tx->request_header->head != NULL) ) {
        ib_rule_log_debug(tx, NULL, NULL, NULL,
                          "Running header through stream header");
        rc = run_stream_header_rules(ib, tx, event,
                                     tx->request_header->head,
                                     cbdata);
        if (rc != IB_OK) {
            ib_rule_log_error(tx, NULL, NULL, NULL,
                              "Error processing tx request line: %s",
                              ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Initialize rule set objects.
 *
 * @param[in] ib Engine
 * @param[in] mp Memory pool to use for allocations
 * @param[in,out] ctx_rules Context's rules
 *
 * @returns Status code
 */
static ib_status_t init_ruleset(ib_engine_t *ib,
                                ib_mpool_t *mp,
                                ib_rule_context_t *ctx_rules)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_num_t    phase_num;

    /* Initialize the phase rules */
    for (phase_num = (ib_num_t)PHASE_NONE;
         phase_num < (ib_num_t)IB_RULE_PHASE_COUNT;
         ++phase_num)
    {
        ib_ruleset_phase_t *ruleset_phase =
            &(ctx_rules->ruleset.phases[phase_num]);
        ruleset_phase->phase_num = (ib_rule_phase_t)phase_num;
        rc = find_phase_meta(phase_num, &(ruleset_phase->phase_meta));
        if (rc != IB_OK) {
            ib_log_error(ib,
                         "Rule set initialization: "
                         "failed to find phase meta data: %s",
                         ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        rc = ib_list_create(&(ruleset_phase->rule_list), mp);
        if (rc != IB_OK) {
            ib_log_error(ib,
                         "Rule set initialization: "
                         "failed to create phase ruleset list: %s",
                         ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Create a hash to hold rules indexed by ID */
    rc = ib_hash_create_nocase(&(ctx_rules->rule_hash), mp);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Rule set initialization: failed to create hash: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Register rules callbacks
 *
 * @param[in] ib Engine
 * @param[in] mp Memory pool to use for allocations
 * @param[in,out] rule_engine Rule engine
 *
 * @returns Status code
 */
static ib_status_t register_callbacks(ib_engine_t *ib,
                                      ib_mpool_t *mp,
                                      ib_rule_engine_t *rule_engine)
{
    IB_FTRACE_INIT();
    const ib_rule_phase_meta_t *meta;
    const char                 *hook_type = NULL;
    ib_status_t                 rc = IB_OK;

    /* Register specific handlers for specific events, and a
     * generic handler for the rest */
    for (meta = rule_phase_meta; meta->phase_num != PHASE_INVALID; ++meta) {
        if (meta->event == (ib_state_event_type_t) -1) {
            continue;
        }

        /* Non-phase rules all use the same callback */
        if (! meta->is_stream) {
            rc = ib_hook_tx_register(
                ib,
                meta->event,
                run_phase_rules,
                (void *)meta);
            hook_type = "tx";
        }
        else {
            switch (meta->hook_type) {

            case IB_STATE_HOOK_TX:
                rc = ib_hook_tx_register(
                    ib,
                    meta->event,
                    run_stream_tx_rules,
                    (void *)meta);
                hook_type = "stream-tx";
                break;

            case IB_STATE_HOOK_TXDATA:
                rc = ib_hook_txdata_register(
                    ib,
                    meta->event,
                    run_stream_txdata_rules,
                    (void *)meta);
                hook_type = "txdata";
                break;

            case IB_STATE_HOOK_HEADER:
                rc = ib_hook_parsed_header_data_register(
                    ib,
                    meta->event,
                    run_stream_header_rules,
                    (void *)meta);
                hook_type = "header";
                break;

            default:
                ib_log_error(ib,
                             "Unknown hook registration type %d for "
                             "phase %d/\"%s\"",
                             meta->phase_num, meta->event, meta->name);
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }
        }

        /* OK */
        if (rc != IB_OK) {
            ib_log_error(ib,
                         "Hook \"%s\" registration for phase "
                         "%d/%d/\"%s\" returned %s",
                         hook_type, meta->phase_num, meta->event, meta->name,
                         ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Initialize a rule engine object.
 *
 * @param[in] ib Engine
 * @param[in,out] mp Memory pool to use for allocations
 * @param[out] p_rule_engine Pointer to new rule engine object
 *
 * @returns Status code
 */
static ib_status_t create_rule_engine(const ib_engine_t *ib,
                                      ib_mpool_t *mp,
                                      ib_rule_engine_t **p_rule_engine)
{
    IB_FTRACE_INIT();
    ib_rule_engine_t *rule_engine;
    ib_status_t       rc;

    /* Create the rule object */
    rule_engine =
        (ib_rule_engine_t *)ib_mpool_calloc(mp, 1, sizeof(*rule_engine));
    if (rule_engine == NULL) {
        ib_log_error(ib,
                     "Rule engine failed to allocate rule engine object");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Create the rule list */
    rc = ib_list_create(&(rule_engine->rule_list), mp);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Rule engine failed to create rule list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_hash_create_nocase(&(rule_engine->rule_hash), mp);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Rule engine failed to create rule hash: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    *p_rule_engine = rule_engine;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Initialize a context's rule object.
 *
 * @param[in] ib Engine
 * @param[in,out] mp Memory pool to use for allocations
 * @param[out] p_ctx_rules Pointer to new rule context object
 *
 * @returns Status code
 */
static ib_status_t create_rule_context(const ib_engine_t *ib,
                                       ib_mpool_t *mp,
                                       ib_rule_context_t **p_ctx_rules)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(mp != NULL);
    assert(p_ctx_rules != NULL);

    ib_rule_context_t *ctx_rules;
    ib_status_t        rc;

    /* Create the rule object */
    ctx_rules =
        (ib_rule_context_t *)ib_mpool_calloc(mp, 1, sizeof(*ctx_rules));
    if (ctx_rules == NULL) {
        ib_log_error(ib,
                     "Rule engine failed to allocate context rule object");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Create the rule list */
    rc = ib_list_create(&(ctx_rules->rule_list), mp);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Rule engine failed to initialize rule list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Create the rule enable/disable lists */
    rc = ib_list_create(&(ctx_rules->enable_list), mp);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Rule engine failed to initialize rule enable list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    rc = ib_list_create(&(ctx_rules->disable_list), mp);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Rule engine failed to initialize rule disable list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    *p_ctx_rules = ctx_rules;
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_engine_init(ib_engine_t *ib,
                                ib_module_t *mod)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Create the rule engine object */
    rc = create_rule_engine(ib, ib->mp, &(ib->rule_engine));
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Rule engine failed to create rule engine: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the rule callbacks */
    rc = register_callbacks(ib, ib->mp, ib->rule_engine);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Rule engine failed to register phase callbacks: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_engine_ctx_init(ib_engine_t *ib,
                                    ib_module_t *mod,
                                    ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* If the rules are already initialized, do nothing */
    if (ctx->rules != NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Create the rule engine object */
    rc = create_rule_context(ib, ctx->mp, &(ctx->rules));
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Rule engine failed to initialize context rules: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Initialize the rule sets */
    rc = init_ruleset(ib, ctx->mp, ctx->rules);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Rule engine failed to initialize phase ruleset: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Enable/disable an individual rule
 *
 * @param[in] enable true:Enable, false:Disable
 * @param[in,out] ctx_rule Context rule to enable/disable
 */
static void set_rule_enable(bool enable,
                            ib_rule_ctx_data_t *ctx_rule)
{
    IB_FTRACE_INIT();
    assert(ctx_rule != NULL);

    if (enable) {
        ib_flags_set(ctx_rule->flags, IB_RULECTX_FLAG_ENABLED);
    }
    else {
        ib_flags_clear(ctx_rule->flags, IB_RULECTX_FLAG_ENABLED);
    }
    IB_FTRACE_RET_VOID();
}

/**
 * Enable rules that match tag / id
 *
 * @param[in] ib IronBee engine
 * @param[in] ctx Current IronBee context
 * @param[in] match Enable match data
 * @param[in] enable true:Enable, false:Disable
 * @param[in,out] ctx_rule_list List of rules to search for matches to @a enable
 *
 * @returns Status code
 */
static ib_status_t enable_rules(ib_engine_t *ib,
                                ib_context_t *ctx,
                                const ib_rule_enable_t *match,
                                bool enable,
                                ib_list_t *ctx_rule_list)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(ctx != NULL);
    assert(match != NULL);
    assert(ctx_rule_list != NULL);

    ib_list_node_t *node;
    unsigned int    matches = 0;
    const char     *name = enable ? "Enable" : "Disable";
    const char     *lcname = enable ? "enable" : "disable";

    switch (match->enable_type) {

    case RULE_ENABLE_ALL :
        IB_LIST_LOOP(ctx_rule_list, node) {
            ib_rule_ctx_data_t *ctx_rule;
            ctx_rule = (ib_rule_ctx_data_t *)ib_list_node_data(node);

            ++matches;
            set_rule_enable(enable, ctx_rule);
            ib_cfg_log_debug2_ex(ib, match->file, match->lineno,
                                 "%sd rule matched \"%s\" by ALL",
                                 name, ib_rule_id(ctx_rule->rule));
        }
        if (matches == 0) {
            ib_cfg_log_notice_ex(ib, match->file, match->lineno,
                                 "No rules by ALL to %s",
                                 match->enable_str);
        }
        else {
            ib_cfg_log_debug_ex(ib, match->file, match->lineno,
                                "%sd %u rules by ALL",
                                name, matches);
        }
        IB_FTRACE_RET_STATUS(IB_OK);
        break;

    case RULE_ENABLE_ID :
        /* Note: We return from the loop before because the rule
         * IDs are unique */
        ib_cfg_log_debug3_ex(ib, match->file, match->lineno,
                             "Looking for rule with ID \"%s\" to %s",
                             match->enable_str, lcname);
        IB_LIST_LOOP(ctx_rule_list, node) {
            ib_rule_ctx_data_t *ctx_rule;
            bool matched;
            ctx_rule = (ib_rule_ctx_data_t *)ib_list_node_data(node);

            /* Match the rule ID, including children */
            matched = ib_rule_id_match(ctx_rule->rule,
                                       match->enable_str,
                                       false,
                                       true);
            if (matched) {
                set_rule_enable(enable, ctx_rule);
                ib_cfg_log_debug2_ex(ib, match->file, match->lineno,
                                     "%sd ID matched rule \"%s\"",
                                     name, ib_rule_id(ctx_rule->rule));
                IB_FTRACE_RET_STATUS(IB_OK);
            }
        }
        ib_cfg_log_notice_ex(ib, match->file, match->lineno,
                             "No rule with ID of \"%s\" to %s",
                             match->enable_str, lcname);
        IB_FTRACE_RET_STATUS(IB_ENOENT);
        break;

    case RULE_ENABLE_TAG :
        ib_cfg_log_debug3_ex(ib, match->file, match->lineno,
                             "Looking for rules with tag \"%s\" to %s",
                             match->enable_str, lcname);
        IB_LIST_LOOP(ctx_rule_list, node) {
            ib_rule_ctx_data_t   *ctx_rule;
            ib_rule_t            *rule;
            bool                  matched;

            ctx_rule = (ib_rule_ctx_data_t *)ib_list_node_data(node);
            rule = ctx_rule->rule;

            matched = ib_rule_tag_match(ctx_rule->rule,
                                        match->enable_str,
                                        false,
                                        true);
            if (matched) {
                ++matches;
                set_rule_enable(enable, ctx_rule);
                ib_cfg_log_debug2_ex(ib, match->file, match->lineno,
                                     "%s tag \"%s\" matched "
                                     "rule \"%s\" from ctx=\"%s\"",
                                     name,
                                     match->enable_str,
                                     ib_rule_id(rule),
                                     ib_context_full_get(rule->ctx));
            }
        }
        if (matches == 0) {
            ib_cfg_log_notice_ex(ib, match->file, match->lineno,
                                 "No rules with tag of \"%s\" to %s",
                                 match->enable_str, lcname);
        }
        else {
            ib_cfg_log_debug_ex(ib, match->file, match->lineno,
                                "%s %u rules with tag of \"%s\"",
                                name, matches, match->enable_str);
        }
        IB_FTRACE_RET_STATUS(IB_OK);
        break;

    default:
        assert(0 && "Invalid rule enable type");

    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_engine_ctx_close(ib_engine_t *ib,
                                     ib_module_t *mod,
                                     ib_context_t *ctx)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(mod != NULL);
    assert(ctx != NULL);

    ib_list_t      *all_rules;
    ib_list_node_t *node;
    ib_flags_t      skip_flags;
    ib_status_t     rc;
    ib_context_t   *main_ctx = ib_context_main(ib);

    /* Don't enable rules for the main context */
    if (ctx == main_ctx) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Create the list of all rules */
    rc = ib_list_create(&all_rules, ctx->mp);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Rule engine failed to initialize rule list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Step 1: Unmark all rules in the context's rule list */
    IB_LIST_LOOP(ctx->rules->rule_list, node) {
        ib_rule_t    *rule = (ib_rule_t *)ib_list_node_data(node);
        ib_flags_clear(rule->flags, IB_RULE_FLAG_MARK);
    }

    /* Step 2: Loop through all of the rules in the main context, add them
     * to the list of all rules */
    ib_log_debug2(ib, "Adding rules from \"%s\" to ctx \"%s\" temp list",
                  ib_context_full_get(main_ctx),
                  ib_context_full_get(ctx));
    skip_flags = IB_RULE_FLAG_CHCHILD;
    IB_LIST_LOOP(main_ctx->rules->rule_list, node) {
        ib_rule_t          *ref = (ib_rule_t *)ib_list_node_data(node);
        ib_rule_t          *rule;
        ib_rule_ctx_data_t *ctx_rule;

        ib_log_debug3(ib, "Looking at rule \"%s\" from \"%s\"",
                      ref->meta.id, ib_context_full_get(ref->ctx));

        /* If it's a chained rule, skip it */
        if (ib_flags_any(ref->flags, skip_flags)) {
            continue;
        }

        /* Find the appropriate version of the rule to use */
        rc = ib_rule_lookup(ib, ctx, ref->meta.id, &rule);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to lookup rule \"%s\": %s",
                         ref->meta.id, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Create a rule ctx object for it, store it in the list */
        ctx_rule = ib_mpool_alloc(ctx->mp, sizeof(*ctx_rule));
        if (ctx_rule == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
        ctx_rule->rule = rule;
        if ( (! ib_flags_all(rule->flags, IB_RULE_FLAG_MAIN_CTX)) ||
                ib_flags_all(rule->flags, IB_RULE_FLAG_FORCE_EN) )
        {
            ctx_rule->flags = IB_RULECTX_FLAG_ENABLED;
            ib_flags_set(rule->flags, IB_RULE_FLAG_MARK);
        }
        else {
            ctx_rule->flags = IB_RULECTX_FLAG_NONE;
        }
        rc = ib_list_push(all_rules, ctx_rule);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
        ib_log_debug3(ib, "Adding rule \"%s\" from \"%s\" to ctx temp list",
                      ib_rule_id(rule), ib_context_full_get(rule->ctx));
    }

    /* Step 3: Loop through all of the context's rules, add them
     * to the list of all rules if they're not marked... */
    ib_log_debug2(ib, "Adding ctx rules to ctx \"%s\" temp list",
                  ib_context_full_get(ctx));
    skip_flags = (IB_RULE_FLAG_MARK | IB_RULE_FLAG_CHCHILD);
    IB_LIST_LOOP(ctx->rules->rule_list, node) {
        ib_rule_t          *rule = (ib_rule_t *)ib_list_node_data(node);
        ib_rule_ctx_data_t *ctx_rule;

        /* If the rule is chained or marked */
        if (ib_flags_all(rule->flags, skip_flags)) {
            ib_log_debug3(ib, "Skipping marked/chained rule \"%s\" from \"%s\"",
                          ib_rule_id(rule), ib_context_full_get(rule->ctx));
            continue;
        }

        /* Create a ctx object for it, store it in the list */
        ctx_rule = ib_mpool_alloc(ctx->mp, sizeof(*ctx_rule));
        if (ctx_rule == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
        ctx_rule->rule = rule;
        ib_flags_set(ctx_rule->flags, IB_RULECTX_FLAG_ENABLED);
        rc = ib_list_push(all_rules, ctx_rule);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
        ib_log_debug3(ib, "Adding rule \"%s\" from \"%s\" to ctx temp list",
                      ib_rule_id(rule), ib_context_full_get(rule->ctx));
    }

    /* Step 4: Disable rules (All) */
    IB_LIST_LOOP(ctx->rules->disable_list, node) {
        const ib_rule_enable_t *enable;
        enable = (const ib_rule_enable_t *)ib_list_node_data(node);
        if (enable->enable_type != RULE_ENABLE_ALL) {
            continue;
        }

        ib_log_debug2(ib, "Disabling all rules in \"%s\" temp list",
                      ib_context_full_get(ctx));

        /* Apply disable */
        rc = enable_rules(ib, ctx, enable, false, all_rules);
        if (rc != IB_OK) {
            ib_cfg_log_notice_ex(ib, enable->file, enable->lineno,
                                 "Error disabling all rules "
                                 "in \"%s\" temp list",
                                 ib_context_full_get(ctx));
        }
    }

    /* Step 5: Enable marked enabled rules */
    ib_log_debug2(ib, "Enabling specified rules in \"%s\" temp list",
                  ib_context_full_get(ctx));
    IB_LIST_LOOP(ctx->rules->enable_list, node) {
        const ib_rule_enable_t *enable;

        enable = (const ib_rule_enable_t *)ib_list_node_data(node);

        /* Find rule */
        rc = enable_rules(ib, ctx, enable, true, all_rules);
        if (rc != IB_OK) {
            ib_cfg_log_notice_ex(ib, enable->file, enable->lineno,
                                 "Error enabling specified rules "
                                 "in \"%s\" temp list",
                                 ib_context_full_get(ctx));
        }
    }

    /* Step 6: Disable marked rules (except All) */
    ib_log_debug2(ib, "Disabling specified rules in \"%s\" temp list",
                  ib_context_full_get(ctx));
    IB_LIST_LOOP(ctx->rules->disable_list, node) {
        const ib_rule_enable_t *enable;

        enable = (const ib_rule_enable_t *)ib_list_node_data(node);
        if (enable->enable_type == RULE_ENABLE_ALL) {
            continue;
        }

        /* Find rule */
        rc = enable_rules(ib, ctx, enable, false, all_rules);
        if (rc != IB_OK) {
            ib_cfg_log_notice_ex(ib, enable->file, enable->lineno,
                                 "Error disabling specified rules "
                                 "in \"%s\" temp list",
                                 ib_context_full_get(ctx));
        }
    }

    /* Step 7: Add all enabled rules to the appropriate execution list */
    ib_log_debug2(ib, "Adding enabled rules to ctx \"%s\" phase list",
                  ib_context_full_get(ctx));
    skip_flags = IB_RULECTX_FLAG_ENABLED;
    IB_LIST_LOOP(all_rules, node) {
        ib_rule_ctx_data_t *ctx_rule;
        ib_ruleset_phase_t *ruleset_phase;
        ib_list_t          *phase_rule_list;
        ib_rule_phase_t     phase_num;
        ib_rule_t          *rule;

        ctx_rule = (ib_rule_ctx_data_t *)ib_list_node_data(node);
        assert(ctx_rule != NULL);
        rule = ctx_rule->rule;

        /* If it's not enabled, skip to the next rule */
        if (! ib_flags_all(ctx_rule->flags, skip_flags)) {
            ib_log_debug3(ib, "Skipping disabled rule \"%s\" from \"%s\"",
                          ib_rule_id(rule), ib_context_full_get(rule->ctx));
            continue;
        }

        /* Determine what phase list to add it into */
        phase_num = rule->meta.phase;
        ruleset_phase = &(ctx->rules->ruleset.phases[phase_num]);
        assert(ruleset_phase != NULL);
        assert(ruleset_phase->phase_meta == rule->phase_meta);
        phase_rule_list = ruleset_phase->rule_list;

        /* Add it to the list */
        rc = ib_list_push(phase_rule_list, (void *)ctx_rule);
        if (rc != IB_OK) {
            ib_log_error(ib,
                         "Failed to add rule type=\"%s\" phase=%d "
                         "context=\"%s\": %s",
                         rule->phase_meta->is_stream ? "Stream" : "Normal",
                         ruleset_phase->phase_num,
                         ib_context_full_get(ctx),
                         ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        ib_log_debug(ib,
                     "Enabled rule \"%s\" rev=%u type=\"%s\" phase=%d/\"%s\" "
                     "for context \"%s\"",
                     ib_rule_id(rule), rule->meta.revision,
                     rule->phase_meta->is_stream ? "Stream" : "Normal",
                     ruleset_phase->phase_num,
                     rule->phase_meta->name,
                     ib_context_full_get(ctx));
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_mpool_t *ib_rule_mpool(ib_engine_t *ib)
{
    IB_FTRACE_INIT();

    /* Return a pointer to the configuration memory pool */
    IB_FTRACE_RET_PTR(ib_mpool_t, ib_engine_pool_config_get(ib));
}


/**
 * Calculate a rule's position in a chain.
 *
 * @param[in] ib IronBee Engine
 * @param[in] rule The rule
 * @param[out] pos The rule's position in the chain
 *
 * @returns Status code
 */
static ib_status_t chain_position(ib_engine_t *ib,
                                  const ib_rule_t *rule,
                                  ib_num_t *pos)
{
    IB_FTRACE_INIT();
    ib_num_t n = 0;

    /* Loop through all parent rules */
    while (rule != NULL) {
        ++n;
        rule = rule->chained_from;
    }; /* while (rule != NULL); */
    *pos = n;
    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
 * Generate the id for a chained rule.
 *
 * @param[in] ib IronBee Engine
 * @param[in,out] rule The rule
 * @param[in] force Set the ID even if it's already set
 *
 * @returns Status code
 */
static ib_status_t chain_gen_rule_id(ib_engine_t *ib,
                                     ib_rule_t *rule,
                                     bool force)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    char idbuf[128];
    ib_num_t pos;

    /* If it's already set, do nothing */
    if ( (rule->meta.id != NULL) && (! force) ) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    rc = chain_position(ib, rule, &pos);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    snprintf(idbuf, sizeof(idbuf), "%s/%d", rule->meta.chain_id, (int)pos);
    rule->meta.id = ib_mpool_strdup(ib_rule_mpool(ib), idbuf);
    if (rule->meta.id == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

bool ib_rule_should_capture(const ib_rule_t *rule,
                            ib_num_t result)
{
    IB_FTRACE_INIT();

    if ( (result != 0) &&
         (rule != NULL) &&
         (ib_flags_all(rule->flags, IB_RULE_FLAG_CAPTURE)) )
    {
        IB_FTRACE_RET_BOOL(true);
    }
    else {
        IB_FTRACE_RET_BOOL(false);
    }
}

ib_status_t ib_rule_create(ib_engine_t *ib,
                           ib_context_t *ctx,
                           const char *file,
                           unsigned int lineno,
                           bool is_stream,
                           ib_rule_t **prule)
{
    IB_FTRACE_INIT();
    ib_status_t                 rc;
    ib_rule_t                  *rule;
    ib_list_t                  *lst;
    ib_mpool_t                 *mp = ib_rule_mpool(ib);
    ib_rule_context_t          *context_rules;
    ib_rule_t                  *previous;
    const ib_rule_phase_meta_t *phase_meta;

    assert(ib != NULL);
    assert(ctx != NULL);

    /* Initialize the context's rule set (if required) */
    rc = ib_rule_engine_ctx_init(ib, NULL, ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to initialize rules for context \"%s\"",
                     ib_context_full_get(ctx));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Look up the generic rule phase */
    rc = find_phase_stream_meta(is_stream, PHASE_NONE, &phase_meta);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error looking up rule phase: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Allocate the rule */
    rule = (ib_rule_t *)ib_mpool_calloc(mp, sizeof(ib_rule_t), 1);
    if (rule == NULL) {
        ib_log_error(ib, "Failed to allocate rule: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    rule->flags = IB_RULE_FLAG_NONE;
    rule->phase_meta = phase_meta;
    rule->meta.phase = PHASE_NONE;
    rule->meta.revision = 1;
    rule->meta.config_file = file;
    rule->meta.config_line = lineno;
    rule->ctx = ctx;
    rule->opinst = NULL;

    /* Note if this is the main context */
    if (ctx == ib_context_main(ib)) {
        rule->flags |= IB_RULE_FLAG_MAIN_CTX;
    }

    /* Meta tags list */
    lst = NULL;
    rc = ib_list_create(&lst, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create rule meta tags list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    rule->meta.tags = lst;

    /* Target list */
    lst = NULL;
    rc = ib_list_create(&lst, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create rule target field list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    rule->target_fields = lst;

    /* True Action list */
    lst = NULL;
    rc = ib_list_create(&lst, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create rule true action list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    rule->true_actions = lst;

    /* False Action list */
    lst = NULL;
    rc = ib_list_create(&lst, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create rule false action list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    rule->false_actions = lst;

    /* Get the rule engine and previous rule */
    context_rules = ctx->rules;
    previous = context_rules->parser_data.previous;

    /*
     * If the previous rule has it's CHAIN flag set,
     * chain to that rule & update the current rule.
     */
    if (  (previous != NULL) &&
          ((previous->flags & IB_RULE_FLAG_CHPARENT) != 0) )
    {
        previous->chained_rule = rule;
        rule->chained_from = previous;
        rule->meta.phase = previous->meta.phase;
        rule->phase_meta = previous->phase_meta;
        rule->meta.chain_id = previous->meta.chain_id;
        rule->flags |= IB_RULE_FLAG_CHCHILD;
    }

    /* Good */
    rule->parent_rlist = ctx->rules->rule_list;
    *prule = rule;
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_flags_t ib_rule_required_op_flags(const ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    assert(rule != NULL);
    assert(rule->phase_meta != NULL);

    IB_FTRACE_RET_UINT(rule->phase_meta->required_op_flags);
}

bool ib_rule_allow_tfns(const ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    assert(rule != NULL);
    assert(rule->phase_meta != NULL);

    if (ib_flags_all(rule->flags, IB_RULE_FLAG_NO_TGT)) {
        IB_FTRACE_RET_BOOL(false);
    }
    else if (ib_flags_all(rule->phase_meta->flags, PHASE_FLAG_ALLOW_TFNS)) {
        IB_FTRACE_RET_BOOL(true);
    }
    else {
        IB_FTRACE_RET_BOOL(false);
    }
}

bool ib_rule_allow_chain(const ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    assert(rule != NULL);
    assert(rule->phase_meta != NULL);

    if ( (rule->phase_meta->flags & PHASE_FLAG_ALLOW_CHAIN) != 0) {
        IB_FTRACE_RET_BOOL(true);
    }
    else {
        IB_FTRACE_RET_BOOL(false);
    }
}

bool ib_rule_is_stream(const ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    assert(rule != NULL);
    assert(rule->phase_meta != NULL);

    if ( (rule->phase_meta->flags & PHASE_FLAG_IS_STREAM) != 0) {
        IB_FTRACE_RET_BOOL(true);
    }
    else {
        IB_FTRACE_RET_BOOL(false);
    }
}

ib_status_t ib_rule_set_chain(ib_engine_t *ib,
                              ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    assert ((rule->phase_meta->flags & PHASE_FLAG_ALLOW_CHAIN) != 0);

    /* Set the chain flags */
    rule->flags |= IB_RULE_FLAG_CHPARENT;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_set_phase(ib_engine_t *ib,
                              ib_rule_t *rule,
                              ib_rule_phase_t phase_num)
{
    IB_FTRACE_INIT();
    const ib_rule_phase_meta_t *phase_meta;
    ib_status_t rc;

    assert(ib != NULL);
    assert(rule != NULL);
    assert(rule->phase_meta != NULL);

    if ( (rule->meta.phase != PHASE_NONE) &&
         (rule->meta.phase != phase_num) ) {
        ib_log_error(ib,
                     "Cannot set rule phase: already set to %d",
                     rule->meta.phase);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (! is_phase_num_valid(phase_num)) {
        ib_log_error(ib, "Cannot set rule phase: Invalid phase %d",
                     phase_num);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Look up the real rule phase */
    rc = find_phase_stream_meta(rule->phase_meta->is_stream,
                                phase_num,
                                &phase_meta);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error looking up rule phase: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    rule->meta.phase = phase_num;
    rule->phase_meta = phase_meta;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Lookup rule by ID */
ib_status_t ib_rule_lookup(ib_engine_t *ib,
                           ib_context_t *ctx,
                           const char *id,
                           ib_rule_t **rule)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(ctx != NULL);
    assert(id != NULL);
    assert(rule != NULL);

    ib_context_t *main_ctx = ib_context_main(ib);
    assert(main_ctx != NULL);

    ib_status_t rc;

    /* First, look in the context's rule set */
    if ( (ctx != NULL) && (ctx != main_ctx) ) {
        rc = ib_hash_get(ctx->rules->rule_hash, rule, id);
        if (rc != IB_ENOENT) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* If not in the context's rule set, look in the main context */
    rc = ib_hash_get(main_ctx->rules->rule_hash, rule, id);
    IB_FTRACE_RET_STATUS(rc);
}

/* Find rule matching a reference rule */
ib_status_t ib_rule_match(ib_engine_t *ib,
                          ib_context_t *ctx,
                          const ib_rule_t *ref,
                          ib_rule_t **rule)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(ctx != NULL);
    assert(ref != NULL);
    assert(rule != NULL);

    ib_status_t rc;
    ib_rule_t *match = NULL;

    /* Lookup rule with matching ID */
    rc = ib_rule_lookup(ib, ctx, ref->meta.id, &match);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Verify that phase's match */
    if (ref->meta.phase != match->meta.phase) {
        ib_log_error(ib,
                     "\"%s\":%u: Rule phase mismatch @ \"%s\":%u",
                     ref->meta.config_file, ref->meta.config_line,
                     match->meta.config_file, match->meta.config_line);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    *rule = match;
    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t gen_full_id(ib_engine_t *ib,
                               ib_context_t *ctx,
                               ib_rule_t *rule)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(ctx != NULL);
    assert(rule != NULL);
    assert(rule->meta.id != NULL);

    size_t len = 1;    /* Space for trailing nul */
    char *buf;
    const char *part1;
    const char *part2;
    const char *part3;

    /* Calculate the length */
    if (ib_flags_all(rule->flags, IB_RULE_FLAG_MAIN_CTX) ) {
        part1 = "main/";
        part2 = NULL;
        len += 5;                            /* "main/" */
    }
    else {
        const ib_site_t *site = ib_context_site_get(ctx);
        assert(site != NULL);
        part1 = "site/";
        part2 = site->id_str;
        len += 5 + strlen(site->id_str) + 1; /* "site/<id>/" */
    }
    part3 = rule->meta.id;
    len += strlen(part3);

    /* Allocate the buffer */
    buf = (char *)ib_mpool_alloc(ctx->mp, len);
    if (buf == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Finally, build the string */
    strcpy(buf, part1);
    if (part2 != NULL) {
        strcat(buf, part2);
        strcat(buf, "/");
    }
    strcat(buf, part3);
    assert(strlen(buf) == len-1);

    rule->meta.full_id = buf;
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_register(ib_engine_t *ib,
                             ib_context_t *ctx,
                             ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    ib_status_t        rc;
    ib_rule_context_t *context_rules;
    ib_rule_phase_t    phase_num;
    ib_rule_t         *lookup;

    assert(ib != NULL);
    assert(ctx != NULL);
    assert(rule != NULL);
    assert(rule->phase_meta != NULL);

    phase_num = rule->meta.phase;

    /* Sanity checks */
    if( (rule->phase_meta->flags & PHASE_FLAG_IS_VALID) == 0) {
        ib_log_error(ib, "Cannot register rule: Phase is invalid");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (! is_phase_num_valid(phase_num)) {
        ib_log_error(ib, "Cannot register rule: Invalid phase %d", phase_num);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    assert (rule->meta.phase == rule->phase_meta->phase_num);

    /* Verify that we have a valid operator */
    if (rule->opinst == NULL) {
        ib_log_error(ib, "Cannot register rule: No operator instance");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (rule->opinst->op == NULL) {
        ib_log_error(ib, "Cannot register rule: No operator");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (rule->opinst->op->fn_execute == NULL) {
        ib_log_error(ib, "Cannot register rule: No operator function");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Verify that the rule has at least one target */
    if (ib_flags_any(rule->flags, IB_RULE_FLAG_NO_TGT)) {
        if (ib_list_elements(rule->target_fields) != 0) {
            ib_log_error(ib, "Cannot register rule: Action rule has targets");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Give it a fake target */
        assert(ib_list_elements(rule->target_fields) == 0);
        ib_field_t *f = NULL;
        ib_rule_target_t *tgt = NULL;

        rc = ib_field_create(&f, ib_rule_mpool(ib), IB_FIELD_NAME("NULL"),
                             IB_FTYPE_NULSTR, ib_ftype_nulstr_in("NULL"));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        tgt = ib_mpool_calloc(ib_rule_mpool(ib), sizeof(*tgt), 1);
        if (tgt == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
        tgt->field_name = "NULL";
        tgt->target_str = "NULL";
        rc = ib_list_create(&(tgt->tfn_list), ib_rule_mpool(ib));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_list_push(rule->target_fields, tgt);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        if (ib_list_elements(rule->target_fields) == 0) {
            ib_log_error(ib, "Cannot register rule: No targets");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    /* Verify that the rule has an ID */
    if ( (rule->meta.id == NULL) && (rule->meta.chain_id == NULL) ) {
        ib_log_error(ib, "Cannot register rule: No ID");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* If either of the chain flags is set, the chain ID is the rule's ID */
    if (ib_flags_any(rule->flags, IB_RULE_FLAG_CHAIN)) {
        if (rule->chained_from != NULL) {
            rule->meta.chain_id = rule->chained_from->meta.chain_id;
        }
        else {
            rule->meta.chain_id = rule->meta.id;
        }
        rule->meta.id = NULL;
        rc = chain_gen_rule_id(ib, rule, true);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Build the rule's full ID */
    rc = gen_full_id(ib, ctx, rule);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Get the rule engine and previous rule */
    context_rules = ctx->rules;

    /* Handle chained rule */
    if (rule->chained_from != NULL) {
        if (! ib_flags_all(rule->chained_from->flags, IB_RULE_FLAG_VALID)) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        ib_log_debug3(ib,
                      "Registered rule \"%s\" chained from rule \"%s\"",
                      ib_rule_id(rule), ib_rule_id(rule->chained_from));
    }

    /* Put this rule in the hash */
    lookup = NULL;
    rc = ib_rule_match(ib, ctx, rule, &lookup);
    if ( (rc != IB_OK) && (rc != IB_ENOENT) ) {
        ib_cfg_log_error_ex(ib,
                            rule->meta.config_file,
                            rule->meta.config_line,
                            "Error finding matching rule "
                            "for \"%s\" of context=\"%s\": %s",
                            ib_rule_id(rule),
                            ib_context_full_get(ctx),
                            ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Now, replace the existing rule if required */
    if ( (lookup != NULL) &&
         (rule->meta.revision <= lookup->meta.revision) )
    {
        ib_cfg_log_notice_ex(ib,
                             rule->meta.config_file,
                             rule->meta.config_line,
                             "Not replacing rule \"%s\" of context=\"%s\" "
                             "rev=%u with rev=%u",
                             ib_rule_id(rule),
                             ib_context_full_get(ctx),
                             lookup->meta.revision,
                             rule->meta.revision);
        IB_FTRACE_RET_STATUS(IB_EEXIST);
    }

    /* Remove the old version from the hash */
    if (lookup != NULL) {
        ib_hash_remove(context_rules->rule_hash, NULL, rule->meta.id);
    }

    /* Add the new version to the hash */
    rc = ib_hash_set(context_rules->rule_hash, rule->meta.id, rule);
    if (rc != IB_OK) {
        ib_cfg_log_error_ex(ib,
                            rule->meta.config_file,
                            rule->meta.config_line,
                            "Error adding rule \"%s\" "
                            "to context=\"%s\" hash: %s",
                            ib_rule_id(rule),
                            ib_context_full_get(ctx),
                            ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* If no previous rule in the list, add the new rule */
    if (lookup == NULL) {

        /* Add the rule to the list */
        rc = ib_list_push(context_rules->rule_list, rule);
        if (rc != IB_OK) {
            ib_cfg_log_error_ex(ib,
                                rule->meta.config_file,
                                rule->meta.config_line,
                                "Error adding rule \"%s\" "
                                "to context=\"%s\" list: %s",
                                ib_rule_id(rule),
                                ib_context_full_get(ctx),
                                ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
        ib_cfg_log_debug_ex(ib,
                            rule->meta.config_file,
                            rule->meta.config_line,
                            "Added rule \"%s\" rev=%u to context=\"%s\"",
                            ib_rule_id(rule),
                            rule->meta.revision,
                            ib_context_full_get(ctx));
    }
    else {
        /* Walk through the rule list, point at the new rule */
        ib_list_node_t    *node;
        IB_LIST_LOOP(context_rules->rule_list, node) {
            ib_rule_t *r = (ib_rule_t *)ib_list_node_data(node);
            if (strcmp(r->meta.id, rule->meta.id) == 0) {
                node->data = rule;
            }
        }

        ib_cfg_log_info_ex(ib,
                           rule->meta.config_file,
                           rule->meta.config_line,
                           "Replaced rule \"%s\" of context=\"%s\" "
                           "rev=%u with rev=%u",
                           ib_rule_id(rule),
                           ib_context_full_get(ctx),
                           lookup->meta.revision,
                           rule->meta.revision);
    }

    /* Mark the rule as valid */
    rule->flags |= IB_RULE_FLAG_VALID;

    /* Store off this rule for chaining */
    context_rules->parser_data.previous = rule;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_enable(const ib_engine_t *ib,
                           ib_context_t *ctx,
                           ib_rule_enable_type_t etype,
                           const char *name,
                           bool enable,
                           const char *file,
                           unsigned int lineno,
                           const char *str)
{
    IB_FTRACE_INIT();
    ib_status_t        rc;
    ib_rule_enable_t  *item;

    assert(ib != NULL);
    assert(ctx != NULL);
    assert(name != NULL);

    /* Check the string name */
    if (etype != RULE_ENABLE_ALL) {
        assert(str != NULL);
        if (*str == '\0') {
            ib_log_error(ib, "Invalid %s \"\" @ \"%s\":%u: %s",
                         name, file, lineno, str);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    /* Create the enable object */
    item = ib_mpool_alloc(ctx->mp, sizeof(*item));
    item->enable_type = etype;
    item->enable_str = str;
    item->file = file;
    item->lineno = lineno;

    /* Add the item to the appropriate list */
    if (enable) {
        rc = ib_list_push(ctx->rules->enable_list, item);
    }
    else {
        rc = ib_list_push(ctx->rules->disable_list, item);
    }
    if (rc != IB_OK) {
        ib_cfg_log_error_ex(ib, file, lineno,
                            "Error adding %s %s \"%s\" "
                            "to context=\"%s\" list: %s",
                            enable ? "enable" : "disable",
                            str == NULL ? "<None>" : str,
                            name,
                            ib_context_full_get(ctx),
                            ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_enable_all(const ib_engine_t *ib,
                               ib_context_t *ctx,
                               const char *file,
                               unsigned int lineno)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(ctx != NULL);

    ib_status_t rc;

    rc = ib_rule_enable(ib, ctx,
                        RULE_ENABLE_ALL, "all", true,
                        file, lineno, NULL);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_rule_enable_id(const ib_engine_t *ib,
                              ib_context_t *ctx,
                              const char *file,
                              unsigned int lineno,
                              const char *id)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(id != NULL);

    ib_status_t rc;

    rc = ib_rule_enable(ib, ctx,
                        RULE_ENABLE_ID, "id", true,
                        file, lineno, id);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_rule_enable_tag(const ib_engine_t *ib,
                               ib_context_t *ctx,
                               const char *file,
                               unsigned int lineno,
                               const char *tag)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(tag != NULL);

    ib_status_t rc;

    rc = ib_rule_enable(ib, ctx,
                        RULE_ENABLE_TAG, "tag", true,
                        file, lineno, tag);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_rule_disable_all(const ib_engine_t *ib,
                                ib_context_t *ctx,
                                const char *file,
                                unsigned int lineno)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(ctx != NULL);

    ib_status_t rc;

    rc = ib_rule_enable(ib, ctx,
                        RULE_ENABLE_ALL, "all", false,
                        file, lineno, NULL);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_rule_disable_id(const ib_engine_t *ib,
                               ib_context_t *ctx,
                               const char *file,
                               unsigned int lineno,
                               const char *id)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(id != NULL);

    ib_status_t rc;

    rc = ib_rule_enable(ib, ctx,
                        RULE_ENABLE_ID, "id", false,
                        file, lineno, id);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_rule_disable_tag(const ib_engine_t *ib,
                                ib_context_t *ctx,
                                const char *file,
                                unsigned int lineno,
                                const char *tag)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(tag != NULL);

    ib_status_t rc;

    rc = ib_rule_enable(ib, ctx,
                        RULE_ENABLE_TAG, "tag", false,
                        file, lineno, tag);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_rule_set_operator(ib_engine_t *ib,
                                 ib_rule_t *rule,
                                 ib_operator_inst_t *opinst)
{
    IB_FTRACE_INIT();

    if ( (rule == NULL) || (opinst == NULL) ) {
        ib_log_error(ib,
                     "Cannot set rule operator: Invalid rule or operator");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    rule->opinst = opinst;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_set_id(ib_engine_t *ib,
                           ib_rule_t *rule,
                           const char *id)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(rule != NULL);

    if ( (rule == NULL) || (id == NULL) ) {
        ib_log_error(ib, "Cannot set rule id: Invalid rule or id");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (rule->chained_from != NULL) {
        ib_log_error(ib, "Cannot set rule id of chained rule");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (rule->meta.id != NULL) {
        ib_log_error(ib, "Cannot set rule id: already set to \"%s\"",
                     rule->meta.id);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rule->meta.id = id;

    IB_FTRACE_RET_STATUS(IB_OK);
}

const char *ib_rule_id(const ib_rule_t *rule)
{
    IB_FTRACE_INIT();

    assert(rule != NULL);

    if (rule->meta.full_id != NULL) {
        IB_FTRACE_RET_CONSTSTR(rule->meta.full_id);
    }

    if (rule->meta.id != NULL) {
        IB_FTRACE_RET_CONSTSTR(rule->meta.id);
    }
    IB_FTRACE_RET_CONSTSTR(rule->meta.id);
}

bool ib_rule_id_match(const ib_rule_t *rule,
                      const char *id,
                      bool parents,
                      bool children)
{
    IB_FTRACE_INIT();

    /* First match the rule's ID and full ID */
    if ( (strcasecmp(id, rule->meta.id) == 0) ||
         (strcasecmp(id, rule->meta.full_id) == 0) )
    {
        IB_FTRACE_RET_BOOL(true);
    }

    /* Check parent rules if requested */
    if ( parents && (rule->chained_from != NULL) ) {
        bool match = ib_rule_id_match(rule->chained_from, id,
                                      parents, children);
        if (match) {
            IB_FTRACE_RET_BOOL(true);
        }
    }

    /* Check child rules if requested */
    if ( children && (rule->chained_rule != NULL) ) {
        bool match = ib_rule_id_match(rule->chained_rule,
                                      id, parents, children);
        if (match) {
            IB_FTRACE_RET_BOOL(true);
        }
    }

    /* Finally, no match */
    IB_FTRACE_RET_BOOL(false);
}

bool ib_rule_tag_match(const ib_rule_t *rule,
                       const char *tag,
                       bool parents,
                       bool children)
{
    IB_FTRACE_INIT();
    const ib_list_node_t *node;

    /* First match the rule's tags */
    IB_LIST_LOOP_CONST(rule->meta.tags, node) {
        const char *ruletag = (const char *)ib_list_node_data_const(node);
        if (strcasecmp(tag, ruletag) == 0) {
            IB_FTRACE_RET_BOOL(true);
        }
    }

    /* Check parent rules if requested */
    if ( parents && (rule->chained_from != NULL) ) {
        bool match = ib_rule_tag_match(rule->chained_from,
                                       tag, parents, children);
        if (match) {
            IB_FTRACE_RET_BOOL(true);
        }
    }

    /* Check child rules if requested */
    if ( children && (rule->chained_rule != NULL) ) {
        bool match = ib_rule_tag_match(rule->chained_rule,
                                       tag, parents, children);
        if (match) {
            IB_FTRACE_RET_BOOL(true);
        }
    }

    /* Finally, no match */
    IB_FTRACE_RET_BOOL(false);
}

ib_status_t ib_rule_create_target(ib_engine_t *ib,
                                  const char *str,
                                  const char *name,
                                  ib_list_t *tfn_names,
                                  ib_rule_target_t **target,
                                  int *tfns_not_found)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    assert(ib != NULL);
    assert(target != NULL);

    /* Basic checks */
    if (name == NULL) {
        ib_log_error(ib,
                     "Cannot add rule target: Invalid rule or target");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Allocate a rule field structure */
    *target = (ib_rule_target_t *)
        ib_mpool_calloc(ib_rule_mpool(ib), sizeof(**target), 1);
    if (target == NULL) {
        ib_log_error(ib,
                     "Error allocating rule target object \"%s\"", name);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Copy the name */
    (*target)->field_name = (char *)ib_mpool_strdup(ib_rule_mpool(ib), name);
    if ((*target)->field_name == NULL) {
        ib_log_error(ib, "Error copying target field name \"%s\"", name);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Copy the original */
    if (str == NULL) {
        (*target)->target_str = NULL;
    }
    else {
        (*target)->target_str =
            (char *)ib_mpool_strdup(ib_rule_mpool(ib), str);
        if ((*target)->target_str == NULL) {
            ib_log_error(ib, "Error copying target string \"%s\"", str);
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }

    /* Create the field transformation list */
    rc = ib_list_create(&((*target)->tfn_list), ib_rule_mpool(ib));
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Error creating field operator list for target \"%s\": %s",
                     name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Add the transformations in the list (if provided) */
    *tfns_not_found = 0;
    if (tfn_names != NULL) {
        ib_list_node_t *node = NULL;

        IB_LIST_LOOP(tfn_names, node) {
            const char *tfn = (const char *)ib_list_node_data(node);
            rc = ib_rule_target_add_tfn(ib, *target, tfn);
            if (rc == IB_ENOENT) {
                ++(*tfns_not_found);
            }
            else if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Add a target to a rule */
ib_status_t ib_rule_add_target(ib_engine_t *ib,
                               ib_rule_t *rule,
                               ib_rule_target_t *target)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;

    assert(ib != NULL);
    assert(rule != NULL);
    assert(target != NULL);

    /* Enforce the no target flag */
    if (ib_flags_any(rule->flags, IB_RULE_FLAG_NO_TGT)) {
        ib_log_error(ib, "Attempt to add target to action rule \"%s\"",
                     rule->meta.id);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Push the field */
    rc = ib_list_push(rule->target_fields, (void *)target);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to add target \"%s\" to rule \"%s\": %s",
                     target->field_name, rule->meta.id,
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Add a transformation to a target */
ib_status_t ib_rule_target_add_tfn(ib_engine_t *ib,
                                   ib_rule_target_t *target,
                                   const char *name)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_tfn_t *tfn;

    assert(ib != NULL);
    assert(target != NULL);
    assert(name != NULL);

    /* Lookup the transformation by name */
    rc = ib_tfn_lookup(ib, name, &tfn);
    if (rc == IB_ENOENT) {
        ib_log_error(ib, "Transformation \"%s\" not found", name);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (rc != IB_OK) {
        ib_log_error(ib,
                     "Error looking up trans \"%s\" for target \"%s\": %s",
                     name, target->field_name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Add the transformation to the list */
    rc = ib_list_push(target->tfn_list, tfn);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Error adding transformation \"%s\" to list: %s",
                     name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Add a transformation to all targets of a rule */
ib_status_t ib_rule_add_tfn(ib_engine_t *ib,
                            ib_rule_t *rule,
                            const char *name)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_tfn_t *tfn;
    ib_list_node_t *node = NULL;

    assert(ib != NULL);
    assert(rule != NULL);
    assert(name != NULL);

    /* Lookup the transformation by name */
    rc = ib_tfn_lookup(ib, name, &tfn);
    if (rc == IB_ENOENT) {
        ib_log_error(ib, "Transformation \"%s\" not found", name);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (rc != IB_OK) {
        ib_log_error(ib,
                     "Error looking up trans \"%s\" for rule \"%s\": %s",
                     name, rule->meta.id, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Walk through the list of targets, add the transformation to it */
    IB_LIST_LOOP(rule->target_fields, node) {
        ib_rule_target_t *target = (ib_rule_target_t *)ib_list_node_data(node);
        rc = ib_rule_target_add_tfn(ib, target, name);
        if (rc != IB_OK) {
            ib_log_notice(ib,
                          "Error adding tfn \"%s\" to target \"%s\" "
                          "rule \"%s\"",
                          name, target->field_name, rule->meta.id);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Add an action to a rule */
ib_status_t ib_rule_add_action(ib_engine_t *ib,
                               ib_rule_t *rule,
                               ib_action_inst_t *action,
                               ib_rule_action_t which)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    assert(ib != NULL);

    if ( (rule == NULL) || (action == NULL) ) {
        ib_log_error(ib,
                     "Cannot add rule action: Invalid rule or action");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Add the rule to the appropriate action list */
    if (which == RULE_ACTION_TRUE) {
        rc = ib_list_push(rule->true_actions, (void *)action);
    }
    else if (which == RULE_ACTION_FALSE) {
        rc = ib_list_push(rule->false_actions, (void *)action);
    }
    else {
        rc = IB_EINVAL;
    }

    /* Problems? */
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to add rule action \"%s\": %s",
                     action->action->name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_chain_invalidate(ib_engine_t *ib,
                                     ib_context_t *ctx,
                                     ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    ib_status_t tmp_rc;
    ib_flags_t orig;

    if (rule == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    orig = rule->flags;
    rule->flags &= ~IB_RULE_FLAG_VALID;

    /* Invalidate the entire chain upwards */
    if (rule->chained_from != NULL) {
        tmp_rc = ib_rule_chain_invalidate(ib, NULL, rule->chained_from);
        if (tmp_rc != IB_OK) {
            rc = tmp_rc;
        }
    }

    /* If this rule was previously valid, walk down the chain, too */
    if ( (orig & IB_RULE_FLAG_VALID) && (rule->chained_rule != NULL) ) {
        tmp_rc = ib_rule_chain_invalidate(ib, NULL, rule->chained_rule);
        if (tmp_rc != IB_OK) {
            rc = tmp_rc;
        }
    }

    /* Clear the previous rule pointer */
    if (ctx != NULL) {
        ctx->rules->parser_data.previous = NULL;
    }

    IB_FTRACE_RET_STATUS(rc);
}
