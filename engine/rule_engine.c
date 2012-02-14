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

#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/debug.h>
#include <ironbee/operator.h>
#include <ironbee/action.h>
#include <ironbee/rule_engine.h>

#include "ironbee_private.h"
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
    ib_log_debug(ib, 4, "Executing rule %s", rule->meta.id);

    /* Special case: External rules */
    if ( (rule->flags & IB_RULE_FLAG_EXTERNAL) != 0) {
        ib_status_t rc;

        /* Execute the operator */
        ib_log_debug(ib, 4, "Executing external rule");
        rc = ib_operator_execute(ib, tx, opinst, NULL, rule_result);
        if (rc != IB_OK) {
            ib_log_debug(ib, 4,
                         "External operator %s returned an error: %d",
                         opinst->op->name, rc);
        }
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Loop through all of the fields */
    IB_LIST_LOOP(rule->input_fields, node) {
        const char   *fname = (const char *)node->data;
        ib_field_t   *value = 0;
        ib_num_t      result = 0;
        ib_status_t   rc = IB_OK;

        /* Get the field value */
        rc = ib_data_get(tx->dpi, fname, &value);
        if (rc == IB_ENOENT) {
            ib_log_debug(ib, 4, "Field %s not found", fname );
            if ( (opinst->op->flags & IB_OP_FLAG_ALLOW_NULL) == 0) {
                continue;
            }
        }
        else if (rc != IB_OK) {
            ib_log_debug(ib, 4, "Error getting field %s: %d\n", fname, rc);
            continue;
        }

        /* Execute the operator */
        rc = ib_operator_execute(ib, tx, opinst, value, &result);
        if (rc != IB_OK) {
            ib_log_debug(ib, 4,
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

    ib_log_debug(ib, 4,
                 "Executing %s rule %s action %s",
                 rule->meta.id, name, action->action->name);

    /* Run it, check the results */
    rc = ib_action_execute(action, rule, tx);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4,
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

    ib_log_debug(ib, 4, "Executing %s rule %s actions", rule->meta.id, name);

    /* Loop through all of the fields */
    IB_LIST_LOOP(actions, node) {
        ib_status_t       arc;     /* Action's return code */
        ib_action_inst_t *action = (ib_action_inst_t *)node->data;

        /* Execute the action */
        arc = execute_action(ib, rule, tx, result, action);
        if (arc == IB_DECLINED) {
            ib_log_debug(ib, 4,
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
 * @param[in] cbdata Callback data (actually rule_cbdata_t *)
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
    ib_status_t  rc;

    /* Initialize the rule result */
    *rule_result = 0;

    /* Execute the rule */
    rc = execute_rule_operator(ib, rule, tx, rule_result);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Error executing rule %s", rule->meta.id);
    }

    /* Execute the actions */
    if (*rule_result != 0) {
        actions = rule->true_actions;
    }
    else {
        actions = rule->false_actions;
    }
    rc = execute_actions(ib, rule, tx, *rule_result, actions);
    if (rc != IB_OK) {
        ib_log_error(ib, 4,
                     "Error executing action for rule %s", rule->meta.id);
    }

    /* Execute chained rule */
    if ( (*rule_result != 0) && (rule->chained_rule != NULL) ) {
        ib_log_debug(ib, 4,
                     "Chaining to rule %s",
                     rule->chained_rule->meta.id);
        rc = execute_rule(ib, rule->chained_rule, tx, rule_result);
        if (rc != IB_OK) {
            ib_log_error(ib, 4, "Error executing chained rule %s",
                         rule->chained_rule->meta.id);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
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
    ib_context_t         *pctx = ctx->parent;
    ib_rule_phase_data_t *phase = &(pctx->rules->ruleset.phases[rdata->phase]);
    ib_list_t            *rules = phase->rules.rule_list;
    ib_list_node_t       *node = NULL;

    /* Sanity check */
    if (phase->phase != rdata->phase) {
        ib_log_error(ib, 4, "Rule engine: Phase %d (%s) is %d",
                     rdata->phase, rdata->name, phase->phase );
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Walk through the rules & execute them */
    if (IB_LIST_ELEMENTS(rules) == 0) {
        ib_log_debug(ib, 4,
                     "No rules rules for phase %d/%s in context p=%p",
                     rdata->phase, rdata->name, (void*)pctx);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    ib_log_debug(ib, 4,
                 "Executing %d rules for phase %d/%s in context %p",
                 IB_LIST_ELEMENTS(rules),
                 rdata->phase, rdata->name, (void*)pctx);

    /* Loop through all of the rules for this phase, execute them */
    IB_LIST_LOOP(rules, node) {
        ib_rule_t   *rule = (ib_rule_t*)node->data;
        ib_num_t     rule_result = 0;
        ib_status_t  rc;

        /* Execute the rule, it's actions and chains */
        rc = execute_rule(ib, rule, tx, &rule_result);
        if (rc != IB_OK) {
            ib_log_error(ib, 4, "Error executing rule %s", rule->meta.id);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
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

    /* Input list */
    rc = ib_list_create(&lst, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to create rule input field list: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    rule->input_fields = lst;

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
    static ib_rule_t     *chain_rule = NULL;

    /* Chain (if required) */
    if (chain_rule != NULL) {

        /* Verify that the rule phase's match */
        if (chain_rule->meta.phase != rule->meta.phase) {
            ib_log_error(ib, 4,
                         "Chained rule '%s' phase %d != rule phase %d",
                         chain_rule->meta.id,
                         chain_rule->meta.phase,
                         rule->meta.phase);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Chain to the rule, update the our rule's flags */
        chain_rule->chained_rule = rule;
        rule->flags |= IB_RULE_FLAG_CHAINED_TO;

        ib_log_debug(ib, 4,
                     "Rule '%s' chained from rule '%s'",
                     rule->meta.id, chain_rule->meta.id);
    }

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

    /* If the rule isn't chained to, add it to the appropriate phase list */
    if ( (rule->flags & IB_RULE_FLAG_CHAINED_TO) == 0) {
        phasep = &(ctx->rules->ruleset.phases[phase]);
        rules = phasep->rules.rule_list;

        /* Add it to the list */
        rc = ib_list_push(rules, (void*)rule);
        if (rc != IB_OK) {
            ib_log_error(ib, 4,
                         "Failed to add rule phase=%d context=%p: %d",
                         phase, (void*)ctx, rc);
            IB_FTRACE_RET_STATUS(rc);
        }

        ib_log_debug(ib, 4,
                     "Registered rule %s for phase %d of context %p",
                     rule->meta.id, phase, (void*)ctx);
    }

    /* Store off this rule for chaining */
    if ( (rule->flags & IB_RULE_FLAG_CHAIN) != 0) {
        chain_rule = rule;
    }
    else {
        chain_rule = NULL;
    }

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

ib_status_t DLL_PUBLIC ib_rule_add_input(ib_engine_t *ib,
                                         ib_rule_t *rule,
                                         const char *name)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if ( (rule == NULL) || (name == NULL) ) {
        ib_log_error(ib, 4,
                     "Can't add rule input: Invalid rule or input");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_list_push(rule->input_fields, (void*)name);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to add rule input '%s': %d", name, rc);
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
