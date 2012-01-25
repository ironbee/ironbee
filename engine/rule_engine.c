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

/* Init rule flags */
#define IB_RULES_INIT_RULESET     0x0001
#define IB_RULES_INIT_CALLBACKS   0x0002

/**
 * @internal
 * Execute a single rule on a single field
 *
 * @param ib Engine
 * @param opinst Operator instance
 * @param field Field argument to operator
 * @param fname Name of the field
 * @param result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t execute_operator(ib_engine_t *ib,
                                    ib_operator_inst_t *opinst,
                                    ib_field_t *field,
                                    const char *fname,
                                    ib_num_t *result)
{
    IB_FTRACE_INIT(execute_operator);
    ib_status_t   rc;

    /* Run it, check the results */
    rc = opinst->op->fn_execute(opinst->data, field, result);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4,
                     "Operator %s returned an error for field %s: %d",
                     opinst->op->name, fname, rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_log_debug(ib, 9,
                 "Operator %s, field %s => %d",
                 opinst->op->name, fname, *result);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Execute a single rule
 *
 * @param ib Engine
 * @param rule Rule to execute
 * @param tx Transaction
 * @param result Pointer to number in which to store the result
 *
 * @returns Status code
 */
static ib_status_t execute_rule(ib_engine_t *ib,
                                ib_rule_t *rule,
                                ib_tx_t *tx,
                                ib_num_t *rule_result)
{
    IB_FTRACE_INIT(execute_rule);
    ib_list_node_t      *node = NULL;
    ib_operator_inst_t  *opinst = rule->condition.opinst;

    /* Initialize the rule result */
    *rule_result = 0;

    ib_log_debug(ib, 4, "Executing rule %s", rule->meta.id);

    /* Loop through all of the fields */
    IB_LIST_LOOP(rule->input_fields, node) {
        ib_status_t   rc;
        const char   *fname = (const char *)node->data;
        ib_field_t   *value = 0;
        ib_num_t      result = 0;

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
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Execute the operator */
        rc = execute_operator(ib, opinst, value, fname, &result);
        if (rc != IB_OK) {
            ib_log_debug(ib, 4, "Operator %s returned an error: %d",
                         opinst->op->name, rc);
            continue;
        }

        /* Store the result */
        if (result != 0) {
            *rule_result = result;
        }
    }

    /* Invert? */
    if (opinst->flags & IB_OPINST_FLAG_INVERT) {
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
 * @param ib Engine
 * @param rule Rule to execute
 * @param tx Transaction
 * @param name Action list name ("TRUE"/"FALSE")
 * @param action The action to execute
 *
 * @returns Status code
 */
static ib_status_t execute_action(ib_engine_t *ib,
                                  ib_rule_t *rule,
                                  ib_tx_t *tx,
                                  const char *name,
                                  ib_action_inst_t *action)
{
    IB_FTRACE_INIT(execute_action);
    ib_status_t   rc;

    ib_log_debug(ib, 4,
                 "Executing %s rule %s action %s",
                 rule->meta.id, name, action->action->name);

    /* Run it, check the results */
    rc = action->action->fn_execute(action->data, rule, tx);
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
 * @param ib Engine
 * @param rule Rule to execute
 * @param tx Transaction
 * @param name Name of the actions ("TRUE" or "FALSE")
 * @param actions List of actions to execute
 *
 * @returns Status code
 */
static ib_status_t execute_actions(ib_engine_t *ib,
                                   ib_rule_t *rule,
                                   ib_tx_t *tx,
                                   const char *name,
                                   ib_list_t *actions)
{
    ib_list_node_t      *node = NULL;
 
     ib_log_debug(ib, 4, "Executing %s rule %s actions", rule->meta.id, name);

    /* Loop through all of the fields */
    IB_LIST_LOOP(actions, node) {
        ib_status_t       rc;
        ib_action_inst_t *action = (ib_action_inst_t *)node->data;

        /* Execute the action */
        rc = execute_action(ib, rule, tx, name, action);
        if (rc != IB_OK) {
            ib_log_debug(ib, 4,
                         "Action %s/%s returned an error: %d",
                         name, action->action->name, rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Run a set of rules.
 *
 * @param ib Engine
 * @param tx Transaction
 * @param cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t ib_rule_engine_execute(ib_engine_t *ib,
                                          ib_tx_t *tx,
                                          void *cbdata)
{
    IB_FTRACE_INIT(ib_rule_engine_execute);
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

    /* Loop through all of the rules, execute them */
    IB_LIST_LOOP(rules, node) {
        ib_rule_t   *rule = (ib_rule_t*)node->data;
        ib_num_t     rule_result = 0;
        ib_status_t  rc;

        /* Execute the rule */
        rc = execute_rule(ib, rule, tx, &rule_result);
        if (rc != IB_OK) {
            ib_log_error(ib, 4, "Error executing rule %s", rule->meta.id);
        }

        /* Execute the actions */
        if (rule_result != 0) {
            rc = execute_actions(ib, rule, tx, "TRUE",  rule->true_actions);
        }
        else {
            rc = execute_actions(ib, rule, tx, "FALSE", rule->false_actions);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Initialize a rule engine object.
 *
 * @param ib Engine
 * @param mp Memory pool to use for allocations
 * @param flags Initialization flags (IB_RULES_INIT_*)
 * @param prules Pointer to new rules object
 *
 * @returns Status code
 */
static ib_status_t ib_rules_init(ib_engine_t *ib,
                                 ib_mpool_t *mp,
                                 ib_flags_t flags,
                                 ib_rules_t **prules)
{
    IB_FTRACE_INIT(ib_rule_engine_init);
    rule_cbdata_t  *cbdata;
    ib_rules_t     *rules;
    ib_status_t     rc;

    /* Create the rule object */
    rules = (ib_rules_t *)ib_mpool_calloc(mp, 1, sizeof(ib_rules_t));
    if (rules == NULL) {
        ib_log_error(ib, 4, "Rule engine failed to allocate rules object");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Create the rule list */
    rc = ib_list_create(&(rules->rule_list.rule_list), mp);
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
            ib_rule_phase_data_t *p = &(rules->ruleset.phases[phase]);
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
            rc = ib_hook_register(ib,
                                  cbdata->event,
                                  (ib_void_fn_t)ib_rule_engine_execute,
                                  (void*)cbdata);
            if (rc != IB_OK) {
                ib_log_error(ib, 4, "Hook register for %d/%d/%s returned %d",
                             cbdata->phase, cbdata->event, cbdata->name, rc);
                IB_FTRACE_RET_STATUS(rc);
            }
        }
    }

    *prules = rules;
    IB_FTRACE_RET_STATUS(IB_OK);
}


ib_status_t ib_rule_engine_init(ib_engine_t *ib,
                                ib_module_t *mod)
{
    IB_FTRACE_INIT(ib_rule_engine_init);
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
    IB_FTRACE_INIT(ib_rule_engine_ctx_init);
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
    IB_FTRACE_INIT(ib_rule_mpool);
    IB_FTRACE_RET_PTR(ib_mpool_t, ib_engine_pool_config_get(ib));
}

ib_status_t DLL_PUBLIC ib_rule_create(ib_engine_t *ib,
                                      ib_context_t *ctx,
                                      ib_rule_t **prule)
{
    IB_FTRACE_INIT(ib_rule_create);
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
    rule->condition.flags = IB_RULE_FLAG_NONE;

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
    IB_FTRACE_INIT(ib_rule_register);
    ib_status_t           rc;
    ib_rule_phase_data_t *phasep;
    ib_list_t            *rules;

    /* Sanity checks */
    if ( (phase <= PHASE_NONE) || (phase > PHASE_MAX) ) {
        ib_log_error(ib, 4, "Can't register rule: Invalid phase %d", phase);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (rule == NULL) {
        ib_log_error(ib, 4, "Can't register rule: Rule is NULL");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (rule->condition.opinst == NULL) {
        ib_log_error(ib, 4, "Can't register rule: No operator instance");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (rule->condition.opinst->op == NULL) {
        ib_log_error(ib, 4, "Can't register rule: No operator");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    if (rule->condition.opinst->op->fn_execute == NULL) {
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

    phasep = &(ctx->rules->ruleset.phases[phase]);
    rules = phasep->rules.rule_list;

    /* Add it to the list */
    rc = ib_list_push(rules, (void*)rule);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4,
                     "Failed to add rule phase=%d context=%p: %d",
                     phase, (void*)ctx, rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_log_debug(ib, 4,
                 "Registered rule for phase %d of context %p",
                 phase, (void*)ctx);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_rule_set_operator(ib_engine_t *ib,
                                            ib_rule_t *rule,
                                            ib_operator_inst_t *opinst)
{
    IB_FTRACE_INIT(ib_rule_set_operator);

    if ( (rule == NULL) || (opinst == NULL) ) {
        ib_log_error(ib, 4,
                     "Can't set rule operator: Invalid rule or operator");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    rule->condition.opinst = opinst;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_rule_set_id(ib_engine_t *ib,
                                      ib_rule_t *rule,
                                      const char *id)
{
    IB_FTRACE_INIT(ib_rule_set_id);

    if ( (rule == NULL) || (id == NULL) ) {
        ib_log_error(ib, 4, "Can't set rule id: Invalid rule or id");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rule->meta.id = id;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t DLL_PUBLIC ib_rule_update_flags(ib_engine_t *ib,
                                            ib_rule_t *rule,
                                            ib_rule_flagop_t op,
                                            ib_flags_t flags)
{
    IB_FTRACE_INIT(ib_rule_update_flags);

    if (rule == NULL) {
        ib_log_error(ib, 4, "Can't update rule flags: Invalid rule");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    switch(op) {
        case FLAG_OP_SET:
            rule->condition.flags = flags;
            break;
        case FLAG_OP_OR:
            rule->condition.flags |= flags;
            break;
        case FLAG_OP_CLEAR:
            rule->condition.flags &= (~flags);
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
    return rule->condition.flags;
}

ib_status_t DLL_PUBLIC ib_rule_add_input(ib_engine_t *ib,
                                         ib_rule_t *rule,
                                         const char *name)
{
    IB_FTRACE_INIT(ib_rule_add_input);
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
