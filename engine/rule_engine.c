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

/**
 * @file
 * @brief IronBee
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <assert.h>

#include <ironbee/bytestr.h>
#include <ironbee/rule_engine.h>
#include <ironbee/field.h>
#include <ironbee/debug.h>
#include <ironbee/mpool.h>
#include <ironbee/transformation.h>
#include <ironbee/operator.h>
#include <ironbee/action.h>
#include <ironbee/rule_engine.h>

#include <ironbee/debug.h>
#include <ironbee/mpool.h>

#include "ironbee_private.h"

/* Phase rule callback data */
typedef struct {
    ib_rule_phase_t         phase;
    const char             *name;
    ib_state_event_type_t   event;
} phase_rule_cbdata_t;

/* Stream rule callback data */
#define MAX_DATA_TYPES 4
typedef struct {
    ib_rule_stream_t        stream;
    const char             *name;
    ib_state_event_type_t   event;
    ib_data_type_t          dtypes[MAX_DATA_TYPES];
    ib_num_t                num_dtypes;
} stream_rule_cbdata_t;

static phase_rule_cbdata_t phase_rule_cbdata[] = {
    {PHASE_REQUEST_HEADER,  "Request Header",  handle_request_headers_event},
    {PHASE_REQUEST_BODY,    "Request Body",    handle_request_event},
    {PHASE_RESPONSE_HEADER, "Response Header", handle_response_headers_event},
    {PHASE_RESPONSE_BODY,   "Response Body",   handle_response_event},
    {PHASE_POSTPROCESS,     "Post Process",    handle_postprocess_event},
    {PHASE_INVALID,         NULL,              (ib_state_event_type_t) -1}
};

static stream_rule_cbdata_t stream_rule_cbdata[] = {
    {
        STREAM_REQUEST_HEADER,
        "Request Header",
        tx_data_in_event,
        { IB_DTYPE_HTTP_HEADER }, 1
    },
    {
        STREAM_REQUEST_BODY,
        "Request Body",
        tx_data_in_event,
        { IB_DTYPE_HTTP_LINE, IB_DTYPE_HTTP_BODY, IB_DTYPE_HTTP_TRAILER }, 3
    },
    {
        STREAM_RESPONSE_HEADER,
        "Response Header",
        tx_data_out_event,
        { IB_DTYPE_HTTP_HEADER }, 1
    },
    {
        STREAM_RESPONSE_BODY,
        "Response Body",
        tx_data_out_event,
        { IB_DTYPE_HTTP_LINE, IB_DTYPE_HTTP_BODY, IB_DTYPE_HTTP_TRAILER }, 3
    },
    {
        STREAM_INVALID,
        NULL,
        (ib_state_event_type_t) -1,
        { }, 0
    }
};

/* The rule engine uses recursion to walk through lists and chains.  These
 * define the limits to the depth of those recursions. */
#define MAX_LIST_RECURSION   (5)       /**< Max list recursion limit */
#define MAX_CHAIN_RECURSION  (10)      /**< Max chain recursion limit */


/**
 * Log a field's value
 * @internal
 *
 * @param[in] ib Engine
 * @param[in] n Log level
 * @param[in] label Label string
 * @param[in] f Field
 */
static void log_field(ib_engine_t *ib,
                      ib_num_t n,
                      const char *label,
                      const ib_field_t *f)
{
    ib_status_t rc;

    if (f->type == IB_FTYPE_NULSTR) {
        const char *s;
        rc = ib_field_value(f, ib_ftype_nulstr_out(&s));
        if (rc != IB_OK) {
            return;
        }
        ib_log_debug(ib, n, "%s = '%s'", label, s);
    }
    else if (f->type == IB_FTYPE_BYTESTR) {
        const ib_bytestr_t *bs;
        rc = ib_field_value(f, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            return;
        }
        ib_log_debug(ib, n, "%s = '%.*s'",
                     label,
                     (int)ib_bytestr_length(bs),
                     (const char *)ib_bytestr_const_ptr(bs));
    }
    else {
        ib_log_debug(ib, n, "%s type = %d", label, f->type);
    }
}

/**
 * Execute a field's transformations.
 * @internal
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction
 * @param[in] target Target field
 * @param[in] value Initial value of the target field
 * @param[out] result Pointer to field in which to store the result
 *
 * @returns Status code
 */
static ib_status_t execute_field_tfns(ib_engine_t *ib,
                                      ib_tx_t *tx,
                                      ib_rule_target_t *target,
                                      ib_field_t *value,
                                      ib_field_t **result)
{
    IB_FTRACE_INIT();
    ib_status_t     rc;
    ib_num_t        n = 0;
    ib_list_node_t *node = NULL;
    ib_field_t     *in_field;
    ib_field_t     *out = NULL;

    assert(ib != NULL);
    assert(tx != NULL);
    assert(target != NULL);
    assert(result != NULL);

    /* No transformations?  Do nothing. */
    if (IB_LIST_ELEMENTS(target->tfn_list) == 0) {
        *result = value;
        ib_log_debug(ib, 9,
                     "No transformations for field %s", target->field_name);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (value == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug(ib, 9,
                 "Executing %d transformations on field %s",
                 IB_LIST_ELEMENTS(target->tfn_list), target->field_name);

    /*
     * Loop through all of the field operators.
     */
    in_field = value;
    IB_LIST_LOOP(target->tfn_list, node) {
        ib_tfn_t  *tfn = (ib_tfn_t *)node->data;
        ib_flags_t flags = 0;

        /* Run it */
        ++n;
        ib_log_debug(ib, 9,
                     "Executing field transformation #%d '%s' on '%s'",
                     n, tfn->name, target->field_name);
        log_field(ib, 7, "before tfn", in_field);
        rc = ib_tfn_transform(ib, tx->mp, tfn, in_field, &out, &flags);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Error executing field operator #%d field %s: %s",
                         n, target->field_name, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
        log_field(ib, 7, "after tfn", out);

        /* Verify that out isn't NULL */
        if (out == NULL) {
            ib_log_error(ib, 4,
                         "Field operator #%d field %s returned NULL",
                         n, target->field_name);
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
 * Execute a rule on a list of values
 * @internal
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction
 * @param[in] opinst Operator instance
 * @param[in] fname Field name
 * @param[in] value Field value to operate on
 * @param[in] recursion Recursion limit -- won't recurse if recursion is zero
 * @param[out] rule_result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t execute_rule_operator(ib_engine_t *ib,
                                         ib_tx_t *tx,
                                         ib_operator_inst_t *opinst,
                                         const char *fname,
                                         ib_field_t *value,
                                         ib_num_t recursion,
                                         ib_num_t *rule_result)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Limit recursion */
    --recursion;
    if (recursion <= 0) {
        ib_log_error(ib, 4, "Rule engine: List recursion limit reached");
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    /* Handle a list by looping through it */
    if ( (value != NULL) && (value->type == IB_FTYPE_LIST) ) {
        // @todo Remove mutable once list is const correct.
        ib_list_t *vlist;
        rc = ib_field_mutable_value(value, ib_ftype_list_mutable_out(&vlist));
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        ib_list_node_t *node = NULL;
        ib_num_t n = 0;

        IB_LIST_LOOP(vlist, node) {
            ib_field_t *nvalue = (ib_field_t *)ib_list_node_data(node);
            ++n;

            rc = execute_rule_operator(
                ib, tx, opinst, fname, nvalue, recursion, rule_result);
            if (rc != IB_OK) {
                ib_log_debug(ib, 4,
                             "Error executing %s on list element #%d: %s",
                             opinst->op->name, n, ib_status_to_string(rc));
            }
        }
        ib_log_debug(ib, 9, "Operator %s, field %s (list %zd) => %d",
                     opinst->op->name, fname, vlist->nelts, *rule_result);
    }
    else {
        /* Execute the operator */
        ib_num_t result;
        rc = ib_operator_execute(ib, tx, opinst, value, &result);
        if (rc != IB_OK) {
            ib_log_debug(ib, 4,
                         "Operator %s returned an error for field %s: %s",
                         opinst->op->name, fname, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Store the result */
        if (result != 0) {
            *rule_result = result;
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute a single rule's operator on all target fields.
 * @internal
 *
 * @param[in] ib Engine
 * @param[in] rule Rule to execute
 * @param[in,out] tx Transaction
 * @param[out] rule_result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t execute_phase_rule_targets(ib_engine_t *ib,
                                              ib_rule_t *rule,
                                              ib_tx_t *tx,
                                              ib_num_t *rule_result)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(rule != NULL);
    assert(tx != NULL);
    assert(rule_result != NULL);
    ib_list_node_t      *node = NULL;
    ib_operator_inst_t  *opinst = rule->opinst;

    /* Log what we're going to do */
    ib_log_debug(ib, 9, "Executing rule %s", rule->meta.id);

    /* Special case: External rules */
    if ( (rule->flags & IB_RULE_FLAG_EXTERNAL) != 0) {
        ib_status_t rc;

        /* Execute the operator */
        ib_log_debug(ib, 9, "Executing external rule");
        rc = ib_operator_execute(ib, tx, opinst, NULL, rule_result);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "External operator %s returned an error: %s",
                         opinst->op->name, ib_status_to_string(rc));
        }
        IB_FTRACE_RET_STATUS(rc);
    }

    /*
     * Loop through all of the fields.
     *
     * @todo The current behavior is to keep running even after an operator
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    IB_LIST_LOOP(rule->target_fields, node) {
        ib_rule_target_t *target = (ib_rule_target_t *)node->data;
        assert(target != NULL);
        const char       *fname = target->field_name;
        assert(fname != NULL);
        ib_field_t       *value = NULL;     /* Value from the DPI */
        ib_field_t       *tfnvalue = NULL;  /* Value after transformations */
        ib_num_t          result = 0;
        ib_status_t       rc = IB_OK;

        /* Get the field value */
        rc = ib_data_get(tx->dpi, fname, &value);
        if (rc == IB_ENOENT) {
            if ( (opinst->op->flags & IB_OP_FLAG_ALLOW_NULL) == 0) {
                continue;
            }
        }
        else if (rc != IB_OK) {
            ib_log_error(ib, 4, "Error getting field %s: %s",
                         fname, ib_status_to_string(rc));
            continue;
        }

        /* Execute the field operators */
        rc = execute_field_tfns(ib, tx, target, value, &tfnvalue);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Error executing transformation for %s on %s: %s",
                         opinst->op->name, fname, ib_status_to_string(rc));
            continue;
        }

        /* Execute the rule operator */
        rc = execute_rule_operator(ib,
                                   tx,
                                   opinst,
                                   fname,
                                   tfnvalue,
                                   MAX_LIST_RECURSION,
                                   &result);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Operator %s returned an error for field %s: %s",
                         opinst->op->name, fname, ib_status_to_string(rc));
            continue;
        }
        ib_log_debug(ib, 9, "Operator %s, field %s => %d",
                     opinst->op->name, fname, result);

        /* Store the result */
        if (result != 0) {
            *rule_result = result;
        }
    }

    /* Invert? */
    if ( (opinst->flags & IB_OPINST_FLAG_INVERT) != 0) {
        *rule_result = ( (*rule_result) == 0);
    }

    ib_log_debug(ib, 9, "Rule %s Operator %s => %d",
                 rule->meta.id, opinst->op->name, *rule_result);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute a single rule action
 * @internal
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
                                  ib_rule_t *rule,
                                  ib_tx_t *tx,
                                  ib_num_t result,
                                  ib_action_inst_t *action)
{
    IB_FTRACE_INIT();
    ib_status_t   rc;
    const char   *name = (result != 0) ? "True" : "False";

    ib_log_debug(ib, 9,
                 "Executing %s rule %s action %s",
                 rule->meta.id, name, action->action->name);

    /* Run it, check the results */
    rc = ib_action_execute(action, rule, tx);
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Action %s returned an error: %s",
                     action->action->name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Execute a rule's actions
 * @internal
 *
 * @param[in] ib Engine
 * @param[in] rule Rule to execute
 * @param[in,out] tx Transaction
 * @param[in] result Rule execution result
 * @param[in] actions List of actions to execute
 *
 * @returns Status code
 */
static ib_status_t execute_actions(ib_engine_t *ib,
                                   ib_rule_t *rule,
                                   ib_tx_t *tx,
                                   ib_num_t result,
                                   ib_list_t *actions)
{
    IB_FTRACE_INIT();
    ib_list_node_t   *node = NULL;
    ib_status_t       rc = IB_OK;
    const char       *name = (result != 0) ? "True" : "False";

    ib_log_debug(ib, 9, "Executing %s rule %s actions", rule->meta.id, name);

    /*
     * Loop through all of the fields
     *
     * @todo The current behavior is to keep running even after an action
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    IB_LIST_LOOP(actions, node) {
        ib_status_t       arc;     /* Action's return code */
        ib_action_inst_t *action = (ib_action_inst_t *)node->data;

        /* Execute the action */
        arc = execute_action(ib, rule, tx, result, action);
        if (arc == IB_DECLINED) {
            ib_log_error(ib, 4,
                         "Action %s/%s did not run",
                         name, action->action->name);
        }
        else if (arc != IB_OK) {
            ib_log_error(ib, 4,
                         "Action %s/%s returned an error: %d",
                         name, action->action->name, arc);
            rc = arc;
        }
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Execute a single phase rule, it's actions, and it's chained rules.
 * @internal
 *
 * @param[in] ib Engine
 * @param[in] event Event type
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
    ib_list_t   *actions;
    ib_status_t  rc = IB_OK;
    ib_status_t  trc;         /* Temporary status code */

    assert(ib != NULL);
    assert(rule != NULL);
    assert(rule->meta.type == RULE_TYPE_PHASE);
    assert(tx != NULL);
    assert(rule_result != NULL);

    /* Limit recursion */
    --recursion;
    if (recursion <= 0) {
        ib_log_error(ib, 4, "Rule engine: Phase chain recursion limit reached");
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    /* Initialize the rule result */
    *rule_result = 0;

    /**
     * Execute the rule operator on the target fields.
     *
     * @todo The current behavior is to keep running even after an operator
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    trc = execute_phase_rule_targets(ib, rule, tx, rule_result);
    if (trc != IB_OK) {
        ib_log_error(ib, 4, "Error executing rule %s: %s",
                     rule->meta.id, ib_status_to_string(trc));
        rc = trc;
    }

    /**
     * Execute the actions.
     *
     * @todo The current behavior is to keep running even after action(s)
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    if (*rule_result != 0) {
        actions = rule->true_actions;
    }
    else {
        actions = rule->false_actions;
    }
    trc = execute_actions(ib, rule, tx, *rule_result, actions);
    if (trc != IB_OK) {
        ib_log_error(ib, 4,
                     "Error executing action for rule %s", rule->meta.id);
        rc = trc;
    }

    /**
     * Execute chained rule
     *
     * @todo The current behavior is to keep running even after a chained rule
     * returns an error.  This needs further discussion to determine what
     * the correct behavior should be.
     *
     * @note Chaining is currently done via recursion.
     */
    if ( (*rule_result != 0) && (rule->chained_rule != NULL) ) {
        ib_log_debug(ib, 9,
                     "Chaining to rule %s",
                     rule->chained_rule->meta.id);
        trc = execute_phase_rule(ib,
                                 rule->chained_rule,
                                 tx,
                                 recursion,
                                 rule_result);
        if (trc != IB_OK) {
            ib_log_error(ib, 4, "Error executing chained rule %s",
                         rule->chained_rule->meta.id);
            rc = trc;
        }
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Run a set of phase rules.
 * @internal
 *
 * @param[in] ib Engine.
 * @param[in,out] tx Transaction.
 * @param[in] event Event type.
 * @param[in] cbdata Callback data (actually phase_rule_cbdata_t).
 *
 * @returns Status code.
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

    const phase_rule_cbdata_t *rdata = (const phase_rule_cbdata_t *) cbdata;
    ib_context_t              *ctx = tx->ctx;
    ib_rule_phase_data_t      *phase;
    ib_list_t                 *rules;
    ib_list_node_t            *node = NULL;

    phase = &(ctx->rules->ruleset.phases[rdata->phase]);
    assert(phase != NULL);
    rules = phase->rules.rule_list;
    assert(rules != NULL);

    /* Sanity check */
    if (phase->phase != rdata->phase) {
        ib_log_error(ib, 4, "Rule engine: Phase %d (%s) is %d",
                     rdata->phase, rdata->name, phase->phase );
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Walk through the rules & execute them */
    if (IB_LIST_ELEMENTS(rules) == 0) {
        ib_log_debug(ib, 9,
                     "No rules for phase %d/%s in context p=%p",
                     rdata->phase, rdata->name, (void *)ctx);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    ib_log_debug(ib, 9,
                 "Executing %d rules for phase %d/%s in context %p",
                 IB_LIST_ELEMENTS(rules),
                 rdata->phase, rdata->name, (void *)ctx);

    /**
     * Loop through all of the rules for this phase, execute them.
     *
     * @todo The current behavior is to keep running even after rule execution
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    IB_LIST_LOOP(rules, node) {
        ib_rule_t   *rule = (ib_rule_t *)node->data;
        ib_num_t     rule_result = 0;
        ib_status_t  rule_rc;

        /* Skip invalid / disabled rules */
        if ( (rule->flags & IB_RULE_FLAGS_RUNABLE) != IB_RULE_FLAGS_RUNABLE) {
            ib_log_debug(ib, 7,
                         "Not executing invalid/disabled phase rule %s",
                         rule->meta.id);
            continue;
        }

        /* Execute the rule, it's actions and chains */
        rule_rc = execute_phase_rule(ib,
                                     rule,
                                     tx,
                                     MAX_CHAIN_RECURSION,
                                     &rule_result);
        if (rule_rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Error executing rule %s: %s",
                         rule->meta.id, ib_status_to_string(rule_rc));
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
 * Execute a single stream rule, and it's actions
 * @internal
 *
 * @param[in] ib Engine
 * @param[in] event Event type
 * @param[in] tx Transaction
 * @param[in,out] txdata Transaction data
 * @param[in,out] rule_result Result of rule execution
 *
 * @returns Status code
 */
static ib_status_t execute_stream_rule(ib_engine_t *ib,
                                       ib_rule_t *rule,
                                       ib_tx_t *tx,
                                       ib_txdata_t *txdata,
                                       ib_num_t *result)
{
    IB_FTRACE_INIT();
    ib_status_t          rc = IB_OK;
    ib_operator_inst_t  *opinst = rule->opinst;
    ib_field_t          *value = NULL;

    assert(ib != NULL);
    assert(rule != NULL);
    assert(rule->meta.type == RULE_TYPE_STREAM);
    assert(txdata != NULL);
    assert(result != NULL);


    /**
     * Execute the rule operator.
     *
     * @todo The current behavior is to keep running even after action(s)
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */

    /* Create a field to hold the data */
    rc = ib_field_create_bytestr_alias(&value,
                                       tx->mp,
                                       "tmp", 3,
                                       txdata->data, txdata->dlen);
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Error creating field for stream rule data: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Execute the rule operator */
    rc = ib_operator_execute(ib, tx, opinst, value, result);
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Operator %s returned an error: %s",
                     opinst->op->name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_log_debug(ib, 9, "Operator %s => %d", opinst->op->name, *result);

    /* Invert? */
    if ( (opinst->flags & IB_OPINST_FLAG_INVERT) != 0) {
        *result = ( (*result) == 0);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Run a set of stream rules.
 * @internal
 *
 * @param[in] ib Engine.
 * @param[in] tx Transaction.
 * @param[in] event Event type.
 * @param[in,out] txdata Transaction data.
 * @param[in] cbdata Callback data (actually phase_rule_cbdata_t)
 *
 * @returns Status code
 */
static ib_status_t run_stream_rules(ib_engine_t *ib,
                                    ib_tx_t *tx,
                                    ib_state_event_type_t event,
                                    ib_txdata_t *txdata,
                                    void *cbdata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(txdata != NULL);
    assert(cbdata != NULL);

    const stream_rule_cbdata_t *rdata = (const stream_rule_cbdata_t *) cbdata;
    ib_context_t               *ctx = tx->ctx;
    ib_rule_stream_data_t      *stream =
        &(ctx->rules->ruleset.streams[rdata->stream]);
    ib_list_t                  *rules = stream->rules.rule_list;
    ib_list_node_t             *node = NULL;

    /* Sanity check */
    if (stream->stream != rdata->stream) {
        ib_log_error(ib, 4, "Rule engine: Stream %d (%s) is %d",
                     rdata->stream, rdata->name, stream->stream );
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Walk through the rules & execute them */
    if (IB_LIST_ELEMENTS(rules) == 0) {
        ib_log_debug(ib, 9,
                     "No rules for stream %d/%s in context p=%p",
                     rdata->stream, rdata->name, (void *)ctx);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    ib_log_debug(ib, 9,
                 "Executing %d rules for stream %d/%s in context %p",
                 IB_LIST_ELEMENTS(rules),
                 rdata->stream, rdata->name, (void *)ctx);

    /**
     * Loop through all of the rules for this phase, execute them.
     *
     * @todo The current behavior is to keep running even after rule execution
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    IB_LIST_LOOP(rules, node) {
        ib_rule_t   *rule = (ib_rule_t *)node->data;
        ib_list_t   *actions;
        ib_num_t     result = 0;
        ib_num_t     dtype_num;
        ib_bool_t    dtype_found = IB_FALSE;
        ib_status_t  rc;

        /*
         * Determine if this event applies to this rule
         */
        assert(rdata->event == event);
        for (dtype_num = 0;  dtype_num < rdata->num_dtypes;  ++dtype_num) {
            if (txdata->dtype == rdata->dtypes[dtype_num]) {
                dtype_found = IB_TRUE;
                break;
            }
        }
        if (dtype_found == IB_FALSE) {
            continue;
        }

        /* Skip invalid / disabled rules */
        if ( (rule->flags & IB_RULE_FLAGS_RUNABLE) != IB_RULE_FLAGS_RUNABLE) {
            ib_log_debug(ib, 7,
                         "Not executing invalid/disabled stream rule %s",
                         rule->meta.id);
            continue;
        }

        /*
         * Execute the rule
         *
         * @todo The current behavior is to keep running even after an
         * operator returns an error.  This needs further discussion to
         * determine what the correct behavior should be.
         */
        rc = execute_stream_rule(ib, rule, tx, txdata, &result);
        if (rc != IB_OK) {
            ib_log_error(ib, 4, "Error executing rule %s: %s",
                         rule->meta.id, ib_status_to_string(rc));
        }

        /*
         * Execute the actions.
         *
         * @todo The current behavior is to keep running even after action(s)
         * returns an error.  This needs further discussion to determine what
         * the correct behavior should be.
         */
        if (result != 0) {
            actions = rule->true_actions;
        }
        else {
            actions = rule->false_actions;
        }
        rc = execute_actions(ib, rule, tx, result, actions);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Error executing action for rule %s: %s",
                         rule->meta.id, ib_status_to_string(rc));
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
 * Initialize phase rule set.
 * @internal
 *
 * @param[in] ib Engine
 * @param[in] mp Memory pool to use for allocations
 * @param[in,out] rule_engine Rule engine
 *
 * @returns Status code
 */
static ib_status_t init_phase_ruleset(ib_engine_t *ib,
                                      ib_mpool_t *mp,
                                      ib_rule_engine_t *rule_engine)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_num_t    phase_num;

    /* Initialize the phase rules */
    for (phase_num = (ib_num_t)PHASE_NONE;
         phase_num <= (ib_num_t)PHASE_MAX;
         ++phase_num)
    {
        ib_rule_phase_data_t *phase =
            &(rule_engine->ruleset.phases[phase_num]);
        phase->phase = (ib_rule_phase_t)phase_num;
        rc = ib_list_create(&(phase->rules.rule_list), mp);
        if (rc != IB_OK) {
            ib_log_error(
                ib, 4,
                "Rule engine failed to create phase ruleset list: %s",
                rc);
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Register phase rules callbacks
 * @internal
 *
 * @param[in] ib Engine
 * @param[in] mp Memory pool to use for allocations
 * @param[in,out] rule_engine Rule engine
 *
 * @returns Status code
 */
static ib_status_t register_phase_callbacks(ib_engine_t *ib,
                                            ib_mpool_t *mp,
                                            ib_rule_engine_t *rule_engine)
{
    IB_FTRACE_INIT();
    phase_rule_cbdata_t *cbdata;
    ib_status_t          rc;

    /* Register specific handlers for specific events, and a
     * generic handler for the rest */
    for (cbdata = phase_rule_cbdata; cbdata->phase != PHASE_INVALID; ++cbdata)
    {
        rc = ib_hook_tx_register(ib,
                                 cbdata->event,
                                 run_phase_rules,
                                 (void *)cbdata);
        if (rc != IB_OK) {
            ib_log_error(ib, 4, "Hook register for phase %d/%d/%s returned %d",
                         cbdata->phase, cbdata->event, cbdata->name,
                         ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Initialize stream rule set.
 * @internal
 *
 * @param[in] ib Engine
 * @param[in] mp Memory pool to use for allocations
 * @param[in,out] rule_engine Rule engine
 *
 * @returns Status code
 */
static ib_status_t init_stream_ruleset(ib_engine_t *ib,
                                       ib_mpool_t *mp,
                                       ib_rule_engine_t *rule_engine)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_num_t    stream_num;

    /* Initialize the stream rules */
    for (stream_num = (ib_num_t)STREAM_NONE;
         stream_num <= (ib_num_t)STREAM_MAX;
         ++stream_num)
    {
        ib_rule_stream_data_t *stream =
            &(rule_engine->ruleset.streams[stream_num]);
        stream->stream = (ib_rule_stream_t)stream_num;
        rc = ib_list_create(&(stream->rules.rule_list), mp);
        if (rc != IB_OK) {
            ib_log_error(
                ib, 4,
                "Rule engine failed to create stream ruleset list: %s",
                rc);
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Register stream rules callbacks
 * @internal
 *
 * @param[in] ib Engine
 * @param[in] mp Memory pool to use for allocations
 * @param[in,out] rule_engine Rule engine
 *
 * @returns Status code
 */
static ib_status_t register_stream_callbacks(ib_engine_t *ib,
                                             ib_mpool_t *mp,
                                             ib_rule_engine_t *rule_engine)
{
    IB_FTRACE_INIT();
    stream_rule_cbdata_t *cbdata;
    ib_status_t           rc;

    /* Register specific handlers for specific events, and a
     * generic handler for the rest */
    for (cbdata = stream_rule_cbdata;
         cbdata->stream != STREAM_INVALID;
         ++cbdata)
    {
        rc = ib_hook_txdata_register(ib,
                                     cbdata->event,
                                     run_stream_rules,
                                     (void *)cbdata);
        if (rc != IB_OK) {
            ib_log_error(ib, 4, "Hook register for stream %d/%d/%s returned %d",
                         cbdata->stream, cbdata->event, cbdata->name,
                         ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Initialize a rule engine object.
 * @internal
 *
 * @param[in] ib Engine
 * @param[in,out] mp Memory pool to use for allocations
 * @param[out] p_rule_engine Pointer to new rule engine object
 *
 * @returns Status code
 */
static ib_status_t create_rule_engine(ib_engine_t *ib,
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
        ib_log_error(ib, 4,
                     "Rule engine failed to allocate rule engine object");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Create the rule list */
    rc = ib_list_create(&(rule_engine->rule_list.rule_list), mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Rule engine failed to initialize rule list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    *p_rule_engine = rule_engine;
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_engine_init(ib_engine_t *ib,
                                ib_module_t *mod)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Create the rule engine object */
    rc = create_rule_engine(ib, ib->mp, &(ib->rules));
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Rule engine failed to create rule engine: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the phase rule callbacks */
    rc = register_phase_callbacks(ib, ib->mp, ib->rules);
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Rule engine failed to register phase callbacks: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the stream rule callbacks */
    rc = register_stream_callbacks(ib, ib->mp, ib->rules);
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Rule engine failed to register stream callbacks: %s",
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
    rc = create_rule_engine(ib, ctx->mp, &(ctx->rules));
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Rule engine failed to initialize context rules: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Initialize the phase rule sets */
    rc = init_phase_ruleset(ib, ctx->mp, ctx->rules);
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Rule engine failed to initialize phase ruleset: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Initialize the stream rule sets */
    rc = init_stream_ruleset(ib, ctx->mp, ctx->rules);
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Rule engine failed to initialize stream ruleset: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
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
 * @internal
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
        if (rule->flags & IB_RULE_FLAG_ENABLED) {
            ++n;
        }
        rule = rule->chained_from;
    }; /* while (rule != NULL); */
    *pos = n;
    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
 * Generate the id for a chained rule.
 * @internal
 *
 * @param[in] ib IronBee Engine
 * @param[in,out] rule The rule
 * @param[in] force Set the ID even if it's already set
 *
 * @returns Status code
 */
static ib_status_t chain_gen_rule_id(ib_engine_t *ib,
                                     ib_rule_t *rule,
                                     ib_bool_t force)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    char idbuf[128];
    ib_num_t pos;

    /* If it's already set, do nothing */
    if ( (rule->meta.id != NULL) && (force == IB_FALSE) ) {
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

ib_status_t DLL_PUBLIC ib_rule_create(ib_engine_t *ib,
                                      ib_context_t *ctx,
                                      ib_rule_type_t type,
                                      ib_rule_t **prule)
{
    IB_FTRACE_INIT();
    ib_status_t       rc;
    ib_rule_t        *rule;
    ib_list_t        *lst;
    ib_mpool_t       *mp = ib_rule_mpool(ib);
    ib_rule_engine_t *rule_engine;
    ib_rule_t        *previous;

    assert(ib != NULL);
    assert(ctx != NULL);
    assert( (type == RULE_TYPE_PHASE) || (type == RULE_TYPE_STREAM) );

    /* Initialize the context's rule set (if required) */
    rc = ib_rule_engine_ctx_init(ib, NULL, ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to initialize rules for context %p",
                     (void *)ctx);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Allocate the rule */
    rule = (ib_rule_t *)ib_mpool_calloc(mp, sizeof(ib_rule_t), 1);
    if (rule == NULL) {
        ib_log_error(ib, 1, "Failed to allocate rule: %s");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Init type & flags */
    rule->meta.type = type;
    rule->flags = IB_RULE_FLAG_NONE;
    if (rule->meta.type == RULE_TYPE_PHASE) {
        rule->flags |= IB_RULE_FLAGS_PHASE;
    }
    else {
        rule->flags |= IB_RULE_FLAGS_STREAM;
    }

    /* meta tags list */
    lst = NULL;
    rc = ib_list_create(&lst, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to create rule meta tags list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    rule->meta.tags = lst;

    /* Target list */
    lst = NULL;
    rc = ib_list_create(&lst, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to create rule target field list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    rule->target_fields = lst;

    /* True Action list */
    lst = NULL;
    rc = ib_list_create(&lst, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to create rule true action list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    rule->true_actions = lst;

    /* False Action list */
    lst = NULL;
    rc = ib_list_create(&lst, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to create rule false action list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    rule->false_actions = lst;

    /* Get the rule engine and previous rule */
    rule_engine = ctx->rules;
    previous = rule_engine->parser_data.previous;

    /*
     * If the previous rule has it's CHAIN flag set,
     * chain to that rule & update the current rule.
     */
    if (  (previous != NULL) &&
          ((previous->flags & IB_RULE_FLAG_CHAIN) != 0) )
    {
        assert (rule->meta.type == previous->meta.type);
        previous->chained_rule = rule;
        rule->chained_from = previous;
        rule->meta.phase = previous->meta.phase;
        rule->meta.chain_id = previous->meta.chain_id;
        rule->flags |= IB_RULE_FLAG_IN_CHAIN;
    }

    /* Good */
    rule->parent_rlist = &(ctx->rules->rule_list);
    *prule = rule;
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_flags_t ib_rule_required_op_flags(const ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    assert( (rule->meta.type == RULE_TYPE_PHASE) ||
            (rule->meta.type == RULE_TYPE_STREAM) );

    /* Verify that this operator is valid for this rule type */
    if (rule->meta.type == RULE_TYPE_PHASE) {
        IB_FTRACE_RET_UINT(IB_OP_FLAG_PHASE);
    }
    else if (rule->meta.type == RULE_TYPE_STREAM) {
        IB_FTRACE_RET_UINT(IB_OP_FLAG_STREAM);
    }
    assert(0 && "Rule type not PHASE or STREAM");
}

ib_status_t ib_rule_set_chain(ib_engine_t *ib,
                              ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    assert ((rule->flags & IB_RULE_FLAG_ALLOW_CHAIN) != 0);

    /* Set the chain flags */
    rule->flags |= IB_RULE_FLAGS_CHAIN;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_set_phase(ib_engine_t *ib,
                              ib_rule_t *rule,
                              ib_rule_phase_t phase)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(rule != NULL);

    if (rule->meta.type != RULE_TYPE_PHASE) {
        ib_log_error(ib, 4, "Can't set rule phase: rule type not PHASE");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if ( (phase <= PHASE_NONE) || (phase > PHASE_MAX) ) {
        ib_log_error(ib, 4, "Can't set rule phase: Invalid phase %d", phase);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    rule->meta.phase = phase;
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_set_stream(ib_engine_t *ib,
                               ib_rule_t *rule,
                               ib_rule_stream_t stream)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(rule != NULL);

    if (rule->meta.type != RULE_TYPE_STREAM) {
        ib_log_error(ib, 4, "Can't set rule stream: rule type not STREAM");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if ( (stream <= STREAM_NONE) || (stream > STREAM_MAX) ) {
        ib_log_error(ib, 4,
                     "Can't set rule stream: Invalid stream %d", stream);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    rule->meta.stream = stream;
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_register(ib_engine_t *ib,
                             ib_context_t *ctx,
                             ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    ib_status_t       rc;
    ib_list_t        *rules;
    ib_rule_engine_t *rule_engine;

    assert(ib != NULL);
    assert(ctx != NULL);
    assert(rule != NULL);
    assert( (rule->meta.type == RULE_TYPE_PHASE) ||
            (rule->meta.type == RULE_TYPE_STREAM) );

    /* Sanity checks */
    if (rule->meta.type == RULE_TYPE_PHASE) {
        ib_rule_phase_t phase = rule->meta.phase;
        if ( (phase <= PHASE_NONE) || (phase > PHASE_MAX) ) {
            ib_log_error(ib, 4,
                         "Can't register rule: Invalid phase %d", phase);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }
    else {
        ib_rule_stream_t stream = rule->meta.stream;
        if ( (stream <= STREAM_NONE) || (stream > STREAM_MAX) ) {
            ib_log_error(ib, 4,
                         "Can't register rule: Invalid stream %d", stream);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    /* Verify that we have a valid operator */
    if (rule->opinst == NULL) {
        ib_log_error(ib, 4, "Can't register rule: No operator instance");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (rule->opinst->op == NULL) {
        ib_log_error(ib, 4, "Can't register rule: No operator");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (rule->opinst->op->fn_execute == NULL) {
        ib_log_error(ib, 4, "Can't register rule: No operator function");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Verify that the rule has an ID */
    if ( (rule->meta.id == NULL) && (rule->meta.chain_id == NULL) ) {
        ib_log_error(ib, 4, "Can't register rule: No ID");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* If the "chain" flag is set, the chain ID is the rule's ID */
    if (rule->flags & IB_RULE_FLAGS_CHAIN) {
        if (rule->chained_from != NULL) {
            rule->meta.chain_id = rule->chained_from->meta.chain_id;
        }
        else {
            rule->meta.chain_id = rule->meta.id;
        }
        rule->meta.id = NULL;
        rc = chain_gen_rule_id(ib, rule, IB_TRUE);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Get the rule engine and previous rule */
    rule_engine = ctx->rules;

    /* Handle chained rule */
    if (rule->chained_from != NULL) {
        if ( (rule->chained_from->flags & IB_RULE_FLAG_VALID) == 0) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        ib_log_debug(ib, 9,
                     "Registered rule '%s' chained from rule '%s'",
                     rule->meta.id, rule->chained_from->meta.id);
    }

    /* If the rule isn't chained to, add it to the appropriate list */
    else {
        if (rule->meta.type == RULE_TYPE_PHASE) {
            ib_rule_phase_t phase = rule->meta.phase;
            ib_rule_phase_data_t *phasep =
                &(rule_engine->ruleset.phases[phase]);

            /* Add it to the list */
            rules = phasep->rules.rule_list;
            rc = ib_list_push(rules, (void *)rule);
            if (rc != IB_OK) {
                ib_log_error(ib, 4,
                             "Failed to add rule phase=%d context=%p: %s",
                             phase, (void *)ctx, ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }

            ib_log_debug(ib, 7,
                         "Registered rule %s for phase %d of context %p",
                         rule->meta.id, phase, (void *)ctx);
        }
        else if (rule->meta.type == RULE_TYPE_STREAM) {
            ib_rule_stream_t stream = rule->meta.stream;
            ib_rule_stream_data_t *streamp =
                &(rule_engine->ruleset.streams[stream]);

            /* Add it to the list */
            rules = streamp->rules.rule_list;
            rc = ib_list_push(rules, (void *)rule);
            if (rc != IB_OK) {
                ib_log_error(ib, 4,
                             "Failed to add rule stream=%d context=%p: %s",
                             stream, (void *)ctx, ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }

            ib_log_debug(ib, 7,
                         "Registered rule %s for stream %d of context %p",
                         rule->meta.id, stream, (void *)ctx);
        }
        else {
            assert(0 && "Rule type not PHASE or STREAM");
        }
    }

    /* Enable & validate this rule */
    rule->flags |= IB_RULE_FLAGS_RUNABLE;

    /* Store off this rule for chaining */
    rule_engine->parser_data.previous = rule;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_rule_set_operator(ib_engine_t *ib,
                                            ib_rule_t *rule,
                                            ib_operator_inst_t *opinst)
{
    IB_FTRACE_INIT();

    if ( (rule == NULL) || (opinst == NULL) ) {
        ib_log_error(ib, 4,
                     "Can't set rule operator: Invalid rule or operator");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    rule->opinst = opinst;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_rule_set_id(ib_engine_t *ib,
                                      ib_rule_t *rule,
                                      const char *id)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(rule != NULL);

    if ( (rule == NULL) || (id == NULL) ) {
        ib_log_error(ib, 4, "Can't set rule id: Invalid rule or id");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (rule->chained_from != NULL) {
        ib_log_error(ib, 4, "Can't set rule id of chained rule");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (rule->meta.id != NULL) {
        ib_log_error(ib, 4, "Can't set rule id: already set to '%s'",
                     rule->meta.id);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rule->meta.id = id;

    IB_FTRACE_RET_STATUS(IB_OK);
}

const char DLL_PUBLIC *ib_rule_id(const ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    assert(rule != NULL);
    IB_FTRACE_RET_CONSTSTR(rule->meta.id);
}

ib_status_t DLL_PUBLIC ib_rule_create_target(ib_engine_t *ib,
                                             const char *name,
                                             ib_list_t *tfn_names,
                                             ib_rule_target_t **target,
                                             ib_num_t *tfns_not_found)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    assert(ib != NULL);
    assert(target != NULL);

    /* Basic checks */
    if (name == NULL) {
        ib_log_error(ib, 4,
                     "Can't add rule target: Invalid rule or target");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Allocate a rule field structure */
    *target = (ib_rule_target_t *)
        ib_mpool_calloc(ib_rule_mpool(ib), sizeof(**target), 1);
    if (target == NULL) {
        ib_log_error(ib, 4,
                     "Error allocating rule target object '%s'", name);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Copy the name */
    (*target)->field_name = (char *)ib_mpool_strdup(ib_rule_mpool(ib), name);
    if ((*target)->field_name == NULL) {
        ib_log_error(ib, 4, "Error copying target field name '%s'", name);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Create the field transformation list */
    rc = ib_list_create(&((*target)->tfn_list), ib_rule_mpool(ib));
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Error creating field operator list for target '%s': %s",
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
ib_status_t DLL_PUBLIC ib_rule_add_target(ib_engine_t *ib,
                                          ib_rule_t *rule,
                                          ib_rule_target_t *target)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;

    assert(ib != NULL);
    assert(rule != NULL);
    assert(target != NULL);

    /* Push the field */
    rc = ib_list_push(rule->target_fields, (void *)target);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to add target '%s' to rule '%s': %s",
                     target->field_name, rule->meta.id,
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Add a transformation to a target */
ib_status_t DLL_PUBLIC ib_rule_target_add_tfn(ib_engine_t *ib,
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
        ib_log_alert(ib, 4, "Transformation '%s' not found", name);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Error looking up trans '%s' for target '%s': %s",
                     name, target->field_name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Add the transformation to the list */
    rc = ib_list_push(target->tfn_list, tfn);
    if (rc != IB_OK) {
        ib_log_alert(ib, 4,
                     "Error adding transformation '%s' to list: %s",
                     name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Add a transformation to all targets of a rule */
ib_status_t DLL_PUBLIC ib_rule_add_tfn(ib_engine_t *ib,
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
        ib_log_alert(ib, 4, "Transformation '%s' not found", name);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Error looking up trans '%s' for rule '%s': %s",
                     name, rule->meta.id, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Walk through the list of targets, add the transformation to it */
    IB_LIST_LOOP(rule->target_fields, node) {
        ib_rule_target_t *target = (ib_rule_target_t *)ib_list_node_data(node);
        rc = ib_rule_target_add_tfn(ib, target, name);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Error adding tfn '%s' to target '%s' rule '%s':%d",
                         name, target->field_name, rule->meta.id);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Add an action to a rule */
ib_status_t DLL_PUBLIC ib_rule_add_action(ib_engine_t *ib,
                                          ib_rule_t *rule,
                                          ib_action_inst_t *action,
                                          ib_rule_action_t which)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    assert(ib != NULL);

    if ( (rule == NULL) || (action == NULL) ) {
        ib_log_error(ib, 4,
                     "Can't add rule action: Invalid rule or action");
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
        ib_log_error(ib, 4, "Failed to add rule action '%s': %s",
                     action->action->name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_rule_chain_invalidate(ib_engine_t *ib,
                                                ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    ib_status_t tmp_rc;

    if (rule == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    rule->flags &= ~IB_RULE_FLAG_VALID;

    /* Invalidate the entire chain */
    if (rule->chained_from != NULL) {
        tmp_rc = ib_rule_chain_invalidate(ib, rule->chained_from);
        if (tmp_rc != IB_OK) {
            rc = tmp_rc;
        }
    }
    if (rule->chained_rule != NULL) {
        tmp_rc = ib_rule_chain_invalidate(ib, rule->chained_rule);
        if (tmp_rc != IB_OK) {
            rc = tmp_rc;
        }
    }

    IB_FTRACE_RET_STATUS(rc);
}
