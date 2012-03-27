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

/* Callback data */
typedef struct {
    ib_rule_phase_t         phase;
    const char             *name;
    ib_state_event_type_t   event;
} rule_cbdata_t;

static rule_cbdata_t rule_cbdata[] = {
    {PHASE_REQUEST_HEADER,  "Request Header",  handle_request_headers_event},
    {PHASE_REQUEST_BODY,    "Request Body",    handle_request_event},
    {PHASE_RESPONSE_HEADER, "Response Header", handle_response_headers_event},
    {PHASE_RESPONSE_BODY,   "Response Body",   handle_response_event},
    {PHASE_POSTPROCESS,     "Post Process",    handle_postprocess_event},
    {PHASE_INVALID,         NULL,              (ib_state_event_type_t) -1}
};

/**
 * Init rule flags.  Used to specify which parts of the rules object
 * will be initialized by ib_rules_init()
 **/
#define IB_RULES_INIT_RULESET     (1 << 0)   /**< Initialize the ruleset */
#define IB_RULES_INIT_CALLBACKS   (1 << 1)   /**< Initialize the callbacks */

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
    if (f->type == IB_FTYPE_NULSTR) {
        char *p = (char *)ib_field_value_nulstr(f);
        ib_log_debug(ib, n, "%s = '%s'", label, p);
    }
    else if (f->type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *bs = (ib_bytestr_t *)ib_field_value_bytestr(f);
        ib_log_debug(ib, n, "%s = '%*s'",
                     label,
                     (int)ib_bytestr_length(bs),
                     (char *)ib_bytestr_ptr(bs));
    }
    else {
        ib_log_debug(ib, n, "%s type = %d\n", label, f->type);
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
                         "Error executing field operator #%d field %s: %d",
                         n, target->field_name, rc);
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
        // @todo Remove const casting once list is const correct.
        ib_list_t *vlist = (ib_list_t *)ib_field_value_list(value);
        ib_list_node_t *node = NULL;
        ib_num_t n = 0;

        IB_LIST_LOOP(vlist, node) {
            ib_field_t *nvalue = (ib_field_t *)ib_list_node_data(node);
            ++n;

            rc = execute_rule_operator(
                ib, tx, opinst, fname, nvalue, recursion, rule_result);
            if (rc != IB_OK) {
                ib_log_debug(ib, 4,
                             "Error executing %s on list element #%d: %d",
                             opinst->op->name, n, rc);
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
                         "Operator %s returned an error for field %s: %d",
                         opinst->op->name, fname, rc);
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
 * Execute a single rule's operator
 * @internal
 *
 * @param[in] ib Engine
 * @param[in] rule Rule to execute
 * @param[in,out] tx Transaction
 * @param[out] rule_result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t execute_rule(ib_engine_t *ib,
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
                         "External operator %s returned an error: %d",
                         opinst->op->name, rc);
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
        ib_field_t       *fopvalue = NULL;  /* Value after field operators */
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
            ib_log_error(ib, 4, "Error getting field %s: %d\n", fname, rc);
            continue;
        }

        /* Execute the field operators */
        rc = execute_field_tfns(ib, tx, target, value, &fopvalue);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Error executing transformation for %s on %s: %d",
                         opinst->op->name, fname, rc);
            continue;
        }

        /* Execute the rule operator */
        rc = execute_rule_operator(ib,
                                   tx,
                                   opinst,
                                   fname,
                                   fopvalue,
                                   MAX_LIST_RECURSION,
                                   &result);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Operator %s returned an error for field %s: %d",
                         opinst->op->name, fname, rc);
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
                     "Action %s returned an error: %d",
                     action->action->name, rc);
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
 * Execute a single rule, it's actions, and it's chained rules.
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
static ib_status_t execute_rule_all(ib_engine_t *ib,
                                    ib_rule_t *rule,
                                    ib_tx_t *tx,
                                    ib_num_t recursion,
                                    ib_num_t *rule_result)
{
    IB_FTRACE_INIT();
    ib_list_t   *actions;
    ib_status_t  rc = IB_OK;
    ib_status_t  trc;         /* Temporary status code */

    /* Limit recursion */
    --recursion;
    if (recursion <= 0) {
        ib_log_error(ib, 4, "Rule engine: Chain recursion limit reached");
        IB_FTRACE_RET_STATUS(IB_EOTHER);
    }

    /* Initialize the rule result */
    *rule_result = 0;

    /**
     * Execute the rule
     *
     * @todo The current behavior is to keep running even after an operator
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    trc = execute_rule(ib, rule, tx, rule_result);
    if (trc != IB_OK) {
        ib_log_error(ib, 4, "Error executing rule %s: %d", rule->meta.id, trc);
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
        trc = execute_rule_all(ib,
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
 * Run a set of rules.
 * @internal
 *
 * @param[in] ib Engine
 * @param[in] event Event type
 * @param[in,out] tx Transaction
 * @param[in] cbdata Callback data (actually rule_cbdata_t *)
 *
 * @returns Status code
 */
static ib_status_t ib_rule_engine_execute(ib_engine_t *ib,
                                          ib_state_event_type_t event,
                                          ib_tx_t *tx,
                                          void *cbdata)
{
    IB_FTRACE_INIT();
    const rule_cbdata_t  *rdata = (const rule_cbdata_t *) cbdata;
    ib_context_t         *ctx = tx->ctx;
    ib_rule_phase_data_t *phase = &(ctx->rules->ruleset.phases[rdata->phase]);
    ib_list_t            *rules = phase->rules.rule_list;
    ib_list_node_t       *node = NULL;

    assert(ib != NULL);
    assert(tx != NULL);
    assert(cbdata != NULL);

    /* Sanity check */
    if (phase->phase != rdata->phase) {
        ib_log_error(ib, 4, "Rule engine: Phase %d (%s) is %d",
                     rdata->phase, rdata->name, phase->phase );
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Walk through the rules & execute them */
    if (IB_LIST_ELEMENTS(rules) == 0) {
        ib_log_debug(ib, 9,
                     "No rules rules for phase %d/%s in context p=%p",
                     rdata->phase, rdata->name, (void*)ctx);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    ib_log_debug(ib, 9,
                 "Executing %d rules for phase %d/%s in context %p",
                 IB_LIST_ELEMENTS(rules),
                 rdata->phase, rdata->name, (void*)ctx);

    /**
     * Loop through all of the rules for this phase, execute them.
     *
     * @todo The current behavior is to keep running even after rule execution
     * returns an error.  This needs further discussion to determine what the
     * correct behavior should be.
     */
    IB_LIST_LOOP(rules, node) {
        ib_rule_t   *rule = (ib_rule_t*)node->data;
        ib_num_t     rule_result = 0;
        ib_status_t  rule_rc;

        /* Execute the rule, it's actions and chains */
        rule_rc = execute_rule_all(ib,
                                   rule,
                                   tx,
                                   MAX_CHAIN_RECURSION,
                                   &rule_result);
        if (rule_rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Error executing rule %s: %d",
                         rule->meta.id, rule_rc);
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
 * Initialize a rule engine object.
 * @internal
 *
 * @param[in] ib Engine
 * @param[in,out] mp Memory pool to use for allocations
 * @param[in] flags Initialization flags (IB_RULES_INIT_*)
 * @param[out] p_rule_engine Pointer to new rule engine object
 *
 * @returns Status code
 */
static ib_status_t ib_rules_init(ib_engine_t *ib,
                                 ib_mpool_t *mp,
                                 ib_flags_t flags,
                                 ib_rule_engine_t **p_rule_engine)
{
    IB_FTRACE_INIT();
    rule_cbdata_t    *cbdata;
    ib_rule_engine_t *rule_engine;
    ib_status_t       rc;

    /* Create the rule object */
    rule_engine = (ib_rule_engine_t *)
        ib_mpool_calloc(mp, 1, sizeof(*rule_engine));
    if (rule_engine == NULL) {
        ib_log_error(ib, 4,
                     "Rule engine failed to allocate rule engine object");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Create the rule list */
    rc = ib_list_create(&(rule_engine->rule_list.rule_list), mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Rule engine failed to initialize rule list: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Initialize the rule set */
    if (flags & IB_RULES_INIT_RULESET) {
        ib_num_t phase;
        for (phase = (ib_num_t)PHASE_NONE;
             phase <= (ib_num_t)PHASE_MAX;
             ++phase) {
            ib_rule_phase_data_t *p = &(rule_engine->ruleset.phases[phase]);
            p->phase = (ib_rule_phase_t)phase;
            rc = ib_list_create(&(p->rules.rule_list), mp);
            if (rc != IB_OK) {
                ib_log_error(ib, 4,
                             "Rule engine failed to create ruleset list: %d",
                             rc);
                IB_FTRACE_RET_STATUS(IB_EALLOC);
            }
        }
    }

    /* Register specific handlers for specific events, and a
     * generic handler for the rest */
    if (flags & IB_RULES_INIT_CALLBACKS) {
        for (cbdata = rule_cbdata; cbdata->phase != PHASE_INVALID; ++cbdata) {
            rc = ib_hook_tx_register(ib,
                                     cbdata->event,
                                     ib_rule_engine_execute,
                                     (void*)cbdata);
            if (rc != IB_OK) {
                ib_log_error(ib, 4, "Hook register for %d/%d/%s returned %d",
                             cbdata->phase, cbdata->event, cbdata->name, rc);
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }

    *p_rule_engine = rule_engine;
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_engine_init(ib_engine_t *ib,
                                ib_module_t *mod)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_rules_init(ib, ib->mp, IB_RULES_INIT_CALLBACKS, &(ib->rules) );
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to initialize rule engine: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_engine_ctx_init(ib_engine_t *ib,
                                    ib_module_t *mod,
                                    ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_status_t  rc;

    /* If the rules are already initialized, do nothing */
    if (ctx->rules != NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Call the init function */
    rc = ib_rules_init(ib, ctx->mp, IB_RULES_INIT_RULESET, &(ctx->rules));
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to initialize context rules: %d", rc);
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

ib_status_t DLL_PUBLIC ib_rule_create(ib_engine_t *ib,
                                      ib_context_t *ctx,
                                      ib_rule_t **prule)
{
    IB_FTRACE_INIT();
    ib_status_t  rc;
    ib_rule_t   *rule;
    ib_list_t   *lst;
    ib_mpool_t  *mp = ib_rule_mpool(ib);

    assert(ib != NULL);
    assert(ctx != NULL);

    /* Allocate the rule */
    rule = (ib_rule_t *)ib_mpool_calloc(mp, sizeof(ib_rule_t), 1);
    if (rule == NULL) {
        ib_log_error(ib, 1, "Failed to allocate rule: %d");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Init flags */
    rule->flags = IB_RULE_FLAG_NONE;

    /* meta tags list */
    rc = ib_list_create(&lst, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to create rule meta tags list: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    rule->meta.tags = lst;

    /* Target list */
    rc = ib_list_create(&lst, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to create rule target field list: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    rule->target_fields = lst;

    /* True Action list */
    rc = ib_list_create(&lst, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to create rule true action list: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    rule->true_actions = lst;

    /* False Action list */
    rc = ib_list_create(&lst, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to create rule false action list: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    rule->false_actions = lst;

    /* Chained rule */
    rule->chained_rule = NULL;

    /* Good */
    rule->parent_rlist = &(ctx->rules->rule_list);
    *prule = rule;
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_rule_register(ib_engine_t *ib,
                             ib_context_t *ctx,
                             ib_rule_t *rule,
                             ib_rule_phase_t phase)
{
    IB_FTRACE_INIT();
    ib_status_t           rc;
    ib_rule_phase_data_t *phasep;
    ib_list_t            *rules;
    ib_rule_engine_t     *rule_engine;
    ib_rule_t            *chain_rule;

    assert(ib != NULL);
    assert(ctx != NULL);
    assert(rule != NULL);

    /* Sanity checks */
    if ( (phase <= PHASE_NONE) || (phase > PHASE_MAX) ) {
        ib_log_error(ib, 4, "Can't register rule: Invalid phase %d", phase);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (rule == NULL) {
        ib_log_error(ib, 4, "Can't register rule: Rule is NULL");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
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
    if (rule->meta.id == NULL) {
        ib_log_error(ib, 4, "Can't register rule: No ID");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (ctx->rules == NULL){
        rc = ib_rules_init(ib, ctx->mp, IB_RULES_INIT_RULESET, &ctx->rules);
        if (rc != IB_OK) {
            ib_log_error(ib, 4, "Failed to initialize rules for context %p",
                         (void*)ctx);
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Get the rule engine and previous rule */
    rule_engine = ctx->rules;
    chain_rule = rule_engine->parser_data.previous;

    /* Chain (if required) */
    if ( (chain_rule != NULL)
         && ((chain_rule->flags & IB_RULE_FLAG_CHAIN) != 0) )
    {

        /* Verify that the rule phase's match */
        if (chain_rule->meta.phase != rule->meta.phase) {
            ib_log_error(ib, 4,
                         "Chained rule '%s' phase %d != rule phase %d",
                         chain_rule->meta.id,
                         chain_rule->meta.phase,
                         rule->meta.phase);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Chain to the rule, update the our rule's flags. */
        chain_rule->chained_rule = rule;
        rule->flags |= IB_RULE_FLAG_CHAINED_TO;

        ib_log_debug(ib, 9,
                     "Rule '%s' chained from rule '%s'",
                     rule->meta.id, chain_rule->meta.id);
    }

    /* If the rule isn't chained to, add it to the appropriate phase list */
    else {
        phasep = &(rule_engine->ruleset.phases[phase]);
        rules = phasep->rules.rule_list;

        /* Add it to the list */
        rc = ib_list_push(rules, (void*)rule);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Failed to add rule phase=%d context=%p: %d",
                         phase, (void*)ctx, rc);
            IB_FTRACE_RET_STATUS(rc);
        }

        ib_log_debug(ib, 7,
                     "Registered rule %s for phase %d of context %p",
                     rule->meta.id, phase, (void*)ctx);
    }

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

    rule->meta.id = id;

    IB_FTRACE_RET_STATUS(IB_OK);
}

const char DLL_PUBLIC *ib_rule_id(const ib_rule_t *rule)
{
    IB_FTRACE_INIT();
    assert(rule != NULL);
    IB_FTRACE_RET_CONSTSTR(rule->meta.id);
}

ib_status_t DLL_PUBLIC ib_rule_update_flags(ib_engine_t *ib,
                                            ib_rule_t *rule,
                                            ib_rule_flagop_t op,
                                            ib_flags_t flags)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(rule != NULL);

    if (rule == NULL) {
        ib_log_error(ib, 4, "Can't update rule flags: Invalid rule");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    switch(op) {
        case FLAG_OP_SET:
            rule->flags = flags;
            break;
        case FLAG_OP_OR:
            rule->flags |= flags;
            break;
        case FLAG_OP_CLEAR:
            rule->flags &= (~flags);
            break;
        default:
            ib_log_error(ib, 4,
                         "Can't update rule flags: Invalid operation %d\n",
                         (int)op);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_flags_t DLL_PUBLIC ib_rule_flags(const ib_rule_t *rule)
{
    return rule->flags;
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
                     "Error creating field operator list for target '%s': %d",
                     name, rc);
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
    rc = ib_list_push(rule->target_fields, (void*)target);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to add target '%s' to rule '%s': %d",
                     target->field_name, rule->meta.id, rc);
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
                     "Error looking up trans '%s' for target '%s': %d",
                     name, target->field_name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Add the transformation to the list */
    rc = ib_list_push(target->tfn_list, tfn);
    if (rc != IB_OK) {
        ib_log_alert(ib, 4,
                     "Error adding transformation '%s' to list: %d", name, rc);
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
    else {
        ib_log_error(ib, 4,
                     "Error looking up trans '%s' for rule '%s': %d",
                     name, rule->meta.id, rc);
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
        rc = ib_list_push(rule->true_actions, (void*)action);
    }
    else if (which == RULE_ACTION_FALSE) {
        rc = ib_list_push(rule->false_actions, (void*)action);
    }
    else {
        rc = IB_EINVAL;
    }

    /* Problems? */
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to add rule action '%s': %d",
                     action->action->name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
