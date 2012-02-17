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

#include <ironbee/bytestr.h>
#include <ironbee/rule_engine.h>
#include <ironbee/field.h>
#include <ironbee/debug.h>
#include <ironbee/mpool.h>
#include <ironbee/rule_engine.h>

#include <ironbee/debug.h>
#include <ironbee/mpool.h>

#include "ironbee_core_private.h"

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


/**
 * @internal
 * Execute a single rule's operator
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction
 * @param[in] target Target field
 * @param[out] result Pointer to field in which to store the result
 *
 * @returns Status code
 */
static ib_status_t execute_field_operators(ib_engine_t *ib,
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

    /* No functions?  Do nothing. */
    if (IB_LIST_ELEMENTS(target->field_ops) == 0) {
        *result = value;
        ib_log_debug(ib, 9,
                     "No field operators for field %s", target->field_name);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug(ib, 9,
                 "Executing %d field operators for field %s",
                 IB_LIST_ELEMENTS(target->field_ops), target->field_name);


    /**
     * Loop through all of the field operators.
     */
    in_field = value;
    IB_LIST_LOOP(target->field_ops, node) {
        ib_field_op_fn_t  fn = (ib_field_op_fn_t)node->data;
        ib_field_t       *out;

        /* Run it */
        ++n;
        ib_log_debug(ib, 9,
                     "Executing field operator #%d field %s",
                     n, target->field_name);
        rc = fn(ib, tx->mp, in_field, &out);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Error executing field operator #%d field %s: %d",
                         n, target->field_name, rc);
            IB_FTRACE_RET_STATUS(rc);
        }

        /* The output of the operator is now the result */
        *result = out;

        /* The output of the operator is now input for the next field op. */
        in_field = out;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Execute a single rule's operator
 *
 * @param[in] ib Engine
 * @param[in] rule Rule to execute
 * @param[in,out] tx Transaction
 * @param[out] rule_result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t execute_rule_operator(ib_engine_t *ib,
                                         ib_rule_t *rule,
                                         ib_tx_t *tx,
                                         ib_num_t *rule_result)
{
    IB_FTRACE_INIT();
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

    /**
     * Loop through all of the fields.
     *
     * @todo The current behavior is to keep running even after an operator
     * returns an error.
     */
    IB_LIST_LOOP(rule->target_fields, node) {
        ib_rule_target_t *target = (ib_rule_target_t *)node->data;
        const char       *fname = target->field_name;
        ib_field_t       *value = NULL;     /* Value from the DPI */
        ib_field_t       *fopvalue = NULL;  /* Value after field operators */
        ib_num_t          result = 0;
        ib_status_t       rc = IB_OK;

        /* Get the field value */
        rc = ib_data_get(tx->dpi, fname, &value);
        if (rc == IB_ENOENT) {
            ib_log_error(ib, 4, "Field %s not found", fname );
            if ( (opinst->op->flags & IB_OP_FLAG_ALLOW_NULL) == 0) {
                continue;
            }
        }
        else if (rc != IB_OK) {
            ib_log_error(ib, 4, "Error getting field %s: %d\n", fname, rc);
            continue;
        }

        /* Execute the field operators */
        rc = execute_field_operators(
            ib, tx, target, value, &fopvalue);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Error executing field operators for %s on %s: %d",
                         opinst->op->name, fname, rc);
            continue;
        }

        /* Execute the operator */
        rc = ib_operator_execute(ib, tx, opinst, fopvalue, &result);
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
 * @internal
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
 * @internal
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

    /**
     * Loop through all of the fields
     *
     * @todo The current behavior is to keep running even after an action
     * returns an error.
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
 * @internal
 * Execute a single rule, it's actions, and it's chained rules.
 *
 * @param[in] ib Engine
 * @param[in] event Event type
 * @param[in,out] tx Transaction
 * @param[in,out] rule_result Result of rule execution
 *
 * @returns Status code
 */
static ib_status_t execute_rule(ib_engine_t *ib,
                                ib_rule_t *rule,
                                ib_tx_t *tx,
                                ib_num_t *rule_result)
{
    IB_FTRACE_INIT();
    ib_list_t   *actions;
    ib_status_t  rc = IB_OK;
    ib_status_t  trc;         /* Temporary status code */

    /* Initialize the rule result */
    *rule_result = 0;

    /**
     * Execute the rule
     *
     * @todo The current behavior is to keep running even after an operator
     * returns an error.
     */
    trc = execute_rule_operator(ib, rule, tx, rule_result);
    if (trc != IB_OK) {
        ib_log_error(ib, 4, "Error executing rule %s: %d", rule->meta.id, trc);
        rc = trc;
    }

    /**
     * Execute the actions.
     *
     * @todo The current behavior is to keep running even after action(s)
     * returns an error.
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
     * rule returns an error.
     *
     * @note Chaining is currently done via recursion.
     */
    if ( (*rule_result != 0) && (rule->chained_rule != NULL) ) {
        ib_log_debug(ib, 9,
                     "Chaining to rule %s",
                     rule->chained_rule->meta.id);
        trc = execute_rule(ib, rule->chained_rule, tx, rule_result);
        if (trc != IB_OK) {
            ib_log_error(ib, 4, "Error executing chained rule %s",
                         rule->chained_rule->meta.id);
            rc = trc;
        }
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Run a set of rules.
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
    ib_status_t           rc = IB_OK;

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
     * returns an error.
     */
    IB_LIST_LOOP(rules, node) {
        ib_rule_t   *rule = (ib_rule_t*)node->data;
        ib_num_t     rule_result = 0;
        ib_status_t  rule_rc;

        /* Execute the rule, it's actions and chains */
        rule_rc = execute_rule(ib, rule, tx, &rule_result);
        if (rule_rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Error executing rule %s: %d",
                         rule->meta.id, rule_rc);
            rc = rule_rc;
        }
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Initialize a rule engine object.
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

    if ( (rule == NULL) || (id == NULL) ) {
        ib_log_error(ib, 4, "Can't set rule id: Invalid rule or id");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rule->meta.id = id;

    IB_FTRACE_RET_STATUS(IB_OK);
}

const char DLL_PUBLIC *ib_rule_id(const ib_rule_t *rule)
{
    return rule->meta.id;
}

ib_status_t DLL_PUBLIC ib_rule_update_flags(ib_engine_t *ib,
                                            ib_rule_t *rule,
                                            ib_rule_flagop_t op,
                                            ib_flags_t flags)
{
    IB_FTRACE_INIT();

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

/**
 * Field operator function: Length
 *
 * @param[in] ib Ironbee engine.
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] field The field to operate on.
 * @param[out] result The result of the operator.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t fieldop_length(ib_engine_t *ib,
                                  ib_mpool_t *mp,
                                  ib_field_t *field,
                                  ib_field_t **result)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;

    /**
     * This works on C-style (NUL terminated) and byte strings.  Note
     * that data is assumed to be a NUL terminated string (because our
     * configuration parser can't produce anything else).
     **/
    if (field->type == IB_FTYPE_NULSTR) {
        const char *fval = ib_field_value_nulstr( field );
        size_t      len = strlen(fval);
        rc = ib_field_create(
            result, mp, "Length", IB_FTYPE_NUM, &len);
    }
    else if (field->type == IB_FTYPE_BYTESTR) {
        ib_bytestr_t *value = ib_field_value_bytestr(field);
        size_t len = ib_bytestr_length(value);
        rc = ib_field_create(
            result, mp, "Length", IB_FTYPE_NUM, &len);
    }
    else if (field->type == IB_FTYPE_LIST) {
        ib_list_node_t *node = NULL;
        ib_list_t      *ilist;           /** Incoming list */

        /* Get the incoming list */
        ilist = ib_field_value_list(field);
        if (ilist == NULL) {
            IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
        }

        /* Create the outgoing list field */
        rc = ib_field_create(
            result, mp, "Length", IB_FTYPE_LIST, NULL);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Walk through the incoming fields */
        IB_LIST_LOOP(ilist, node) {
            ib_field_t *ifield = (ib_field_t*)node->data;
            ib_field_t *ofield = NULL;

            rc = fieldop_length(ib, mp, ifield, &ofield);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
            rc = ib_field_list_add(*result, ofield);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }
    else {
        size_t len = 1;
        rc = ib_field_create_ex(
            result, mp, field->name, field->nlen, IB_FTYPE_NUM, &len);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Field operator function: Count
 *
 * @param[in] ib Ironbee engine.
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] field The field to operate on.
 * @param[out] result The result of the operator.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t fieldop_count(ib_engine_t *ib,
                                 ib_mpool_t *mp,
                                 ib_field_t *field,
                                 ib_field_t **result)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    ib_num_t value = 0;

    /* If this is a list, return it's count */
    if (field->type == IB_FTYPE_LIST) {
        ib_list_t *lst = ib_field_value_list(field);
        value = IB_LIST_ELEMENTS(lst);
    }
    else {
        value = 1;
    }

    /* Create the output field */
    rc = ib_field_create_ex(
        result, mp, field->name, field->nlen, IB_FTYPE_NUM, &value);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Field operator function: Max (list version)
 *
 * @param[in] ib Ironbee engine.
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] field The field to operate on.
 * @param[out] result The result of the operator.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t fieldop_max_list(ib_engine_t *ib,
                                    ib_mpool_t *mp,
                                    ib_field_t *field,
                                    ib_field_t **result)
{
    IB_FTRACE_INIT();
    ib_status_t     rc = IB_OK;
    ib_list_node_t *node = NULL;
    ib_num_t        maxvalue = 0;
    ib_num_t        n = 0;
    ib_list_t      *lst;

    /* Get the incoming list */
    lst = ib_field_value_list(field);
    if (lst == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Walk through the incoming fields */
    IB_LIST_LOOP(lst, node) {
        ib_field_t *ifield = (ib_field_t*)node->data;
        ib_num_t value;

        switch (ifield->type) {
            case IB_FTYPE_NUM:
                {
                    ib_num_t *fval = ib_field_value_num(ifield);
                    value = *fval;
                }
                break;

            case IB_FTYPE_UNUM:
                {
                    ib_unum_t *fval = ib_field_value_unum(ifield);
                    value = (ib_num_t)(*fval);
                }
                break;

            case IB_FTYPE_NULSTR:
                {
                    const char *fval = ib_field_value_nulstr(ifield);
                    value = (ib_num_t)strlen(fval);
                }
                break;

            case IB_FTYPE_BYTESTR:
                {
                    ib_bytestr_t *fval = ib_field_value_bytestr(ifield);
                    value = (ib_num_t)ib_bytestr_length(fval);
                }
                break;

            case IB_FTYPE_LIST:
                {
                    ib_field_t *tmp = NULL;
                    ib_num_t   *nptr = NULL;

                    rc = fieldop_max_list(ib, mp, ifield, &tmp);
                    if (rc != IB_OK) {
                        IB_FTRACE_RET_STATUS(rc);
                    }
                    nptr = ib_field_value_num(tmp);
                    if (nptr == NULL) {
                        IB_FTRACE_RET_STATUS(IB_EINVAL);
                    }
                    value = *nptr;
                }
                break;

            default:
                IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        if ( (n == 0) || (value > maxvalue) ) {
            maxvalue = value;
        }
    }

    /* Create the output field */
    rc = ib_field_create_ex(
        result, mp, field->name, field->nlen, IB_FTYPE_NUM, &maxvalue);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Field operator function: Min (list version)
 *
 * @param[in] ib Ironbee engine.
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] field The field to operate on.
 * @param[out] result The result of the operator.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t fieldop_min_list(ib_engine_t *ib,
                                    ib_mpool_t *mp,
                                    ib_field_t *field,
                                    ib_field_t **result)
{
    IB_FTRACE_INIT();
    ib_status_t     rc = IB_OK;
    ib_list_node_t *node = NULL;
    ib_num_t        minvalue = 0;
    ib_num_t        n = 0;
    ib_list_t      *lst;

    /* Get the incoming list */
    lst = ib_field_value_list(field);
    if (lst == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Walk through the incoming fields */
    IB_LIST_LOOP(lst, node) {
        ib_field_t *ifield = (ib_field_t*)node->data;
        ib_num_t value;

        switch (field->type) {
            case IB_FTYPE_NUM:
                {
                    ib_num_t *fval = ib_field_value_num(ifield);
                    value = *fval;
                }
                break;
            case IB_FTYPE_UNUM:
                {
                    ib_unum_t *fval = ib_field_value_unum(ifield);
                    value = (ib_num_t)(*fval);
                }
                break;

            case IB_FTYPE_NULSTR:
                {
                    const char *fval = ib_field_value_nulstr(field);
                    value = (ib_num_t)strlen(fval);
                }
                break;

            case IB_FTYPE_BYTESTR:
                {
                    ib_bytestr_t *fval = ib_field_value_bytestr(field);
                    value = (ib_num_t)ib_bytestr_length(fval);
                }
                break;

            case IB_FTYPE_LIST:
                {
                    ib_field_t *tmp = NULL;
                    ib_num_t   *nptr = NULL;

                    rc = fieldop_min_list(ib, mp, field, &tmp);
                    if (rc != IB_OK) {
                        IB_FTRACE_RET_STATUS(rc);
                    }
                    nptr = ib_field_value_num(tmp);
                    if (nptr == NULL) {
                        IB_FTRACE_RET_STATUS(IB_EINVAL);
                    }
                    value = *nptr;
                }
                break;

            default:
                IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        if ( (n == 0) || (value < minvalue) ) {
            minvalue = value;
        }
    }

    /* Create the output field */
    rc = ib_field_create_ex(
        result, mp, field->name, field->nlen, IB_FTYPE_NUM, &minvalue);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Field operator function: Max
 *
 * @param[in] ib Ironbee engine.
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] field The field to operate on.
 * @param[out] result The result of the operator.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t fieldop_max(ib_engine_t *ib,
                               ib_mpool_t *mp,
                               ib_field_t *field,
                               ib_field_t **result)
{
    IB_FTRACE_INIT();

    switch (field->type) {
        case IB_FTYPE_NUM:
        case IB_FTYPE_UNUM:
            *result = field;
            IB_FTRACE_RET_STATUS(IB_OK);

        case IB_FTYPE_LIST:
            IB_FTRACE_RET_STATUS(fieldop_max_list(ib, mp, field, result));
            break;

        default:
            IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_EINVAL);
}

/**
 * Field operator function: Min
 *
 * @param[in] ib Ironbee engine.
 * @param[in] mp Memory pool to use for allocations.
 * @param[in] field The field to operate on.
 * @param[out] result The result of the operator.
 *
 * @returns IB_OK if successful.
 */
static ib_status_t fieldop_min(ib_engine_t *ib,
                               ib_mpool_t *mp,
                               ib_field_t *field,
                               ib_field_t **result)
{
    IB_FTRACE_INIT();

    switch (field->type) {
        case IB_FTYPE_NUM:
        case IB_FTYPE_UNUM:
            *result = field;
            IB_FTRACE_RET_STATUS(IB_OK);

        case IB_FTYPE_LIST:
            IB_FTRACE_RET_STATUS(fieldop_min_list(ib, mp, field, result));
            break;

        default:
            IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_EINVAL);
}

/**
 * @internal
 * Execute a single rule's operator
 *
 * @param[in] ib Engine
 * @param[in] target_str Target field string
 * @param[in,out] target_field Rule target structure
 *
 * @returns Status code
 */
static ib_status_t parse_field_ops(ib_engine_t *ib,
                                   const char *target_str,
                                   ib_rule_target_t *target_field)
{
    IB_FTRACE_INIT();
    char             *cur;               /* Current position */
    char             *opname;            /* Operator name */
    char             *dup_str;           /* Duplicate string */
    ib_field_op_fn_t  last_opfn = NULL;  /* Operator function */


    /* No parens?  Just store the target string as the field name & return. */
    if (strstr(target_str, "()") == NULL) {
        target_field->field_name = target_str;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Make a duplicate of the target string to work on */
    dup_str = ib_mpool_strdup(ib->mp, target_str);
    if (dup_str == NULL) {
        ib_log_error(ib, 4, "Error duplicating target string '%s'", target_str);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Walk through the string */
    cur = dup_str;
    while (cur != NULL) {
        ib_field_op_fn_t  opfn = NULL;  /* Operator function */
        char             *separator;    /* Current separator */
        char             *next_sep;     /* Next separator */
        char             *parens;       /* Paren pair '()' */

        opname = NULL;

        /* First time through the loop? */
        if (cur == dup_str) {
            separator = strchr(cur, '.');
            if (separator == NULL) {
                break;
            }
        }
        else {
            separator = cur;
        }

        /* Find the next separator and paren set */
        next_sep = strchr(separator+1, '.');
        parens   = strstr(separator+1, "()");

        /* No parens?  We're done. */
        if (parens == NULL) {
            ib_log_debug(ib, 9, "parens == NULL");
            cur = NULL;
        }
        else if (next_sep == NULL) {
            ib_log_debug(ib, 9, "next_sep == NULL");
            *separator = '\0';
            opname = separator + 1;
            cur = NULL;
        }
        else if (parens < next_sep) {
            ib_log_debug(ib, 9, "parens < next_sep");
            *separator = '\0';
            opname = separator + 1;
            cur = next_sep;
        }
        else {
            ib_log_debug(ib, 9, "parens >= next_sep");
            cur = next_sep + 1;
        }

#if 1
        ib_log_debug(ib, 9,
                     "cur='%s', opname='%s' sep=%p/'%s' next=%p/'%s' p=%p/'%s'",
                     cur, opname,
                     (void*)separator, separator,
                     (void*)next_sep, next_sep,
                     (void*)parens, parens);
#endif

        /* Skip to top of loop if there's no operator */
        if (opname == NULL) {
            continue;
        }

        /* Lookup the function name */
        if (strncasecmp(opname, "length", 6) == 0) {
            ib_log_debug(ib, 9, "Length field operator found");
            opfn = fieldop_length;
        }
        else if (strncasecmp(opname, "count", 5) == 0) {
            ib_log_debug(ib, 9, "Count field operator found");
            opfn = fieldop_count;
        }
        else if (strncasecmp(opname, "max", 3) == 0) {
            ib_log_debug(ib, 9, "Max field operator found");
            opfn = fieldop_max;
        }
        else if (strncasecmp(opname, "min", 3) == 0) {
            ib_log_debug(ib, 9, "Min field operator found");
            opfn = fieldop_min;
        }
        else {
            ib_log_error(ib, 4, "Unknown field operator: '%s'", opname);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Add the function to the list. */
        if (opfn != NULL) {
            ib_list_push(target_field->field_ops, (void *)opfn);
            last_opfn = opfn;
        }
    }

    /**
     * Add a max operator of the last operator is length.
     */
    if (last_opfn == fieldop_length) {
        ib_log_debug(ib, 9, "Adding max field operator");
        ib_list_push(target_field->field_ops, (void *)fieldop_max);
    }

    /**
     * The field name is the start of the duplicate string, even after
     * it's been chopped up into pieces.
     */
    target_field->field_name = dup_str;
    ib_log_debug(ib, 9, "Final target field name is '%s'", dup_str);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_rule_add_target(ib_engine_t *ib,
                                          ib_rule_t *rule,
                                          const char *name)
{
    IB_FTRACE_INIT();
    ib_rule_target_t *target = NULL;
    ib_status_t rc = IB_OK;

    /* Basic checks */
    if ( (rule == NULL) || (name == NULL) ) {
        ib_log_error(ib, 4,
                     "Can't add rule target: Invalid rule or target");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Allocate a rule field structure */
    target = (ib_rule_target_t *)
        ib_mpool_calloc(ib_rule_mpool(ib), sizeof(*target), 1);
    if (target == NULL) {
        ib_log_error(ib, 4,
                     "Error allocating rule field for '%s': %d", name, rc);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Create the field operator list */
    rc = ib_list_create(&(target->field_ops), ib_rule_mpool(ib));
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Error creating field operator list for target '%s': %d",
                     name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Parse out the calls */
    rc = parse_field_ops(ib, name, target);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Error splitting rule target '%s': %d", name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Push the field */
    rc = ib_list_push(rule->target_fields, (void*)target);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to add rule target '%s': %d", name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_rule_add_action(ib_engine_t *ib,
                                          ib_rule_t *rule,
                                          ib_action_inst_t *action,
                                          ib_rule_action_t which)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

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
