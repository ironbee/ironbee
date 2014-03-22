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
 * @brief IronBee --- Rules module.
 */

#include "ironbee_config_auto.h"

#include "engine_private.h"
#include "rule_engine_private.h"

#include <ironbee/action.h>
#include <ironbee/cfgmap.h>
#include <ironbee/config.h>
#include <ironbee/context.h>
#include <ironbee/core.h>
#include <ironbee/engine.h>
#include <ironbee/flags.h>
#include <ironbee/list.h>
#include <ironbee/lock.h>
#include <ironbee/mm.h>
#include <ironbee/module.h>
#include <ironbee/operator.h>
#include <ironbee/path.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <strings.h>

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        rules
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/* Configuration */
typedef struct {
    /* Where to trace rules to. */
    const char *trace_path;
    /* List of rules being traced. Element: ib_rule_t* */
    ib_list_t *trace_rules;
} per_context_t;

/** Initial value for per-context data. */
static per_context_t c_per_context_initial = {
    NULL, NULL
};

/**
 * Parse rule's operator.
 *
 * Parses the rule's operator and operand strings (@a operator and @a
 * operand), and stores the results in the rule object @a rule.
 *
 * @param cp IronBee configuration parser
 * @param rule Rule object to update
 * @param operator_string Operator string
 * @param operand Operand string
 * @param is_stream Look for stream operator.
 *
 * @returns Status code
 */
static ib_status_t parse_operator(ib_cfgparser_t *cp,
                                  ib_rule_t *rule,
                                  const char *operator_string,
                                  const char *operand,
                                  bool is_stream)
{
    assert(cp != NULL);
    assert(rule != NULL);
    assert(operator_string != NULL);

    ib_status_t rc = IB_OK;
    const char *opname = NULL;
    const char *cptr = operator_string;
    bool invert = false;
    ib_rule_operator_inst_t *opinst;
    const ib_operator_t *operator = NULL;
    ib_mm_t main_mm = ib_engine_mm_main_get(cp->ib);

    /* Leading '!' (invert flag)? */
    if (*cptr == '!') {
        invert = true;
        ++cptr;
    }

    /* Better be an '@' next... */
    if ( (*cptr != '@') || (isalpha(*(cptr+1)) == 0) ) {
        ib_cfg_log_error(cp, "Invalid rule syntax \"%s\"", operator_string);
        return IB_EINVAL;
    }
    opname = cptr + 1;

    /* Acquire operator */
    if (is_stream) {
        rc = ib_operator_stream_lookup(cp->ib, opname, &operator);
    }
    else {
        rc = ib_operator_lookup(cp->ib, opname, &operator);
    }
    if (rc == IB_ENOENT) {
        ib_cfg_log_error(cp, "Unknown operator: %s %s", opname, operand);
        return IB_EINVAL;
    }
    else if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error acquiring operator %s %s: %s",
                         opname, operand, ib_status_to_string(rc));
        return rc;
    }
    assert(operator != NULL);

    /* Allocate instance data. */
    opinst = ib_mm_calloc(main_mm, 1, sizeof(*opinst));
    if (opinst == NULL) {
        return IB_EALLOC;
    }
    opinst->op = operator;
    opinst->params = operand;
    opinst->invert = invert;
    rc = ib_field_create(&(opinst->fparam),
                         main_mm,
                         IB_S2SL("param"),
                         IB_FTYPE_NULSTR,
                         ib_ftype_nulstr_in(operand));
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error creating operand field %s %s: %s",
                         opname, operand, ib_status_to_string(rc));
        return rc;
    }

    /* Create the operator instance */
    rc = ib_operator_inst_create(
        operator,
        cp->cur_ctx,
        ib_rule_required_op_flags(rule),
        operand,
        &(opinst->instance_data)
    );
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error creating operator instance "
                         "operator=\"%s\" operand=\"%s\": %s",
                         opname,
                         operand == NULL ? "" : operand,
                         ib_status_to_string(rc));
        return rc;
    }

    /* Set the operator */
    rule->opinst = opinst;

    return rc;
}

/**
 * Rewrite the target string if required
 *
 * Parses the rule's target field list string @a target_str, looking for
 * the '&' tokens at the start of it.
 *
 * @param[in] cp IronBee configuration parser
 * @param[in] target_str Target field name.
 * @param[out] rewritten Rewritten string.
 * @param[out] rewrites Number of rewrites found in @a target_str.
 *
 * @returns Status code
 */
#define MAX_TFN_TOKENS 10
static ib_status_t rewrite_target_tokens(ib_cfgparser_t *cp,
                                         const char *target_str,
                                         const char **rewritten,
                                         int *rewrites)
{
    char const *ops[MAX_TFN_TOKENS];
    const char *cur = target_str;
    char *new;
    int count = 0;
    int n;
    int target_len = strlen(target_str) + 1;

    /**
     * Loop 'til we reach max count
     */
    while ( (*cur != '\0') && (count < MAX_TFN_TOKENS) ) {
        if (*cur == '&') {
            ops[count] = ".count()";
        }
        else {
            break;
        }

        /* Update the required length */
        target_len += strlen(ops[count]) - 1;
        ++count;
        ++cur;
    }

    /* No rewrites?  Done */
    *rewrites = count;
    if (count == 0) {
        *rewritten = target_str;
        return IB_OK;
    }

    /**
     * Allocate & build the new string
     */
    new = ib_mm_alloc(cp->mm, target_len);
    if (new == NULL) {
        ib_cfg_log_error(cp,
                         "Failed to duplicate target field string \"%s\".",
                         target_str);
        return IB_EALLOC;
    }

    /* Add the functions in reverse order */
    strcpy(new, target_str+count);
    for (n = count-1;  n >= 0;  --n) {
        strcat(new, ops[n] );
    }

    /* Log our rewrite */
    ib_cfg_log_debug3(cp, "Rewrote: \"%s\" -> \"%s\"", target_str, new);

    /* Done */
    *rewritten = new;
    return IB_OK;
}

/**
 * Parse a rule's target string.
 *
 * Parses the rule's target field list string @a target_str, and stores the
 * results in the rule object @a rule.
 *
 * @param[in] cp IronBee configuration parser
 * @param[in,out] rule Rule to operate on
 * @param[in] target_str Target string to parse
 *
 * @returns
 *  - IB_OK if there is one or more targets.
 *  - IB_EINVAL if not targets are found, including if the string is empty.
 *  - IB_EALLOC if a memory allocation fails.
 */
static ib_status_t parse_target(ib_cfgparser_t *cp,
                                ib_rule_t *rule,
                                const char *target_str)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(rule != NULL);
    assert(target_str != NULL);

    ib_status_t rc;
    ib_engine_t *ib = cp->ib;
    const char *rewritten_target_str = NULL;
    const char *final_target_str; /* Holder for the final target name. */
    ib_list_t *tfns_str = NULL;          /* Transformations to perform. */
    ib_rule_target_t *ib_rule_target;
    ib_list_t *tfns;
    int rewrites;
    ib_mm_t mm = ib_engine_mm_main_get(ib);

    /* First, rewrite cur into rewritten_target_str. */
    rc = rewrite_target_tokens(cp, target_str,
                               &rewritten_target_str, &rewrites);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to rewriting target \"%s\".", target_str);
        return rc;
    }

    rc = ib_list_create(&tfns_str, mm);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Cannot allocate transformation list.");
        return rc;
    }

    /* Parse the rewritten string into the final_target_str. */
    rc = ib_cfg_parse_target_string(
        ib_engine_mm_main_get(cp->ib),
        rewritten_target_str,
        &final_target_str,
        tfns_str);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error parsing target string \"%s\": %s",
                         target_str, ib_status_to_string(rc));
        return rc;
    }

    rc = ib_list_create(&tfns, mm);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Cannot allocate transformations list.");
        return rc;
    }

    /* Take parsed transformations and build transformation instances. */
    rc = ib_rule_tfn_fields_to_inst(ib, mm, tfns_str, tfns);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create the target object */
    rc = ib_rule_create_target(cp->ib,
                               final_target_str,
                               tfns,
                               &ib_rule_target);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error creating rule target \"%s\": %s",
                         final_target_str, ib_status_to_string(rc));
        return rc;
    }

    /* Add the target to the rule */
    rc = ib_rule_add_target(cp->ib, rule, ib_rule_target);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to add rule target \"%s\"", target_str);
        return rc;
    }

    return IB_OK;
}

/**
 * Attempt to register a string as an action.
 *
 * Treats the rule's modifier string @a name as a action, and registers
 * the appropriate action with @a rule.
 *
 * @param[in] cp IronBee configuration parser
 * @param[in,out] rule Rule to operate on
 * @param[in] name Action name
 * @param[in] params Parameters string
 *
 * @returns Status code
 */
static ib_status_t register_action_modifier(ib_cfgparser_t *cp,
                                            ib_rule_t *rule,
                                            const char *name,
                                            const char *params)
{
    ib_status_t        rc = IB_OK;
    ib_action_inst_t  *action;
    ib_rule_action_t   atype = IB_RULE_ACTION_TRUE;

    /* Select the action type */
    switch (*name) {
    case '!':
        atype = IB_RULE_ACTION_FALSE;
        ++name;
        break;
    case '+':
        atype = IB_RULE_ACTION_AUX;
        ++name;
        break;
    default:
        atype = IB_RULE_ACTION_TRUE;
    }

    /* Create a new action instance */
    rc = ib_action_inst_create(cp->ib,
                               name,
                               params,
                               &action);
    if (rc == IB_ENOENT) {
        ib_cfg_log_notice(cp, "Ignoring unknown modifier \"%s\"", name);
        return IB_OK;
    }
    else if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error creating action instance \"%s\": %s",
                         name, ib_status_to_string(rc));
        return rc;
    }

    /* Add the action to the rule */
    rc = ib_rule_add_action(cp->ib, rule, action, atype);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error adding action \"%s\" to rule \"%s\": %s",
                         name, ib_rule_id(rule), ib_status_to_string(rc));
        return rc;
    }

    return IB_OK;
}

/**
 * Check that a rule has all the proper modifiers.
 *
 * @param[in] cp The configuration parser
 * @param[in] rule The rule to check
 *
 * @returns IB_EINVAL.
 */
static ib_status_t check_rule_modifiers(ib_cfgparser_t *cp,
                                        ib_rule_t *rule)
{
    bool child = ib_flags_all(rule->flags, IB_RULE_FLAG_CHCHILD);

    if ( (! child) && (ib_rule_id(rule) == NULL) )
    {
        ib_cfg_log_error(cp, "No rule id specified (flags=0x%04"PRIx64")",
                         rule->flags);
        return IB_EINVAL;
    }

    return IB_OK;
}
/**
 * Parse a rule's modifier string.
 *
 * Parses the rule's modifier string @a modifier_str, and stores the results
 * in the rule object @a rule.
 *
 * @param[in] cp IronBee configuration parser
 * @param[in,out] rule Rule to operate on
 * @param[in] modifier_str Modifier string
 *
 * @returns Status code
 */
static ib_status_t parse_modifier(ib_cfgparser_t *cp,
                                  ib_rule_t *rule,
                                  const char *modifier_str)
{
    ib_status_t rc = IB_OK;
    const char *name;
    char *colon;
    char *copy;
    const char *value = NULL;

    assert(cp != NULL);
    assert(rule != NULL);
    assert(modifier_str != NULL);

    /* Copy the string */
    copy = ib_mm_strdup(ib_rule_mm(cp->ib), modifier_str);
    if (copy == NULL) {
        ib_cfg_log_error(cp,
                         "Failed to copy rule modifier \"%s\".", modifier_str);
        return IB_EALLOC;
    }

    /* Modifier name */
    name = copy;
    colon = strchr(copy, ':');
    if ( (colon != NULL) && ( *(colon+1) != '\0' ) ) {
        *colon = '\0';
        value = colon + 1;
        while( isspace(*value) ) {
            ++value;
        }
        if (*value == '\0') {
            value = NULL;
        }
    }

    /* ID modifier */
    if (strcasecmp(name, "id") == 0) {
        if (value == NULL) {
            ib_cfg_log_error(cp, "Modifier ID with no value.");
            return IB_EINVAL;
        }
        rc = ib_rule_set_id(cp->ib, rule, value);
        return rc;
    }

    /* Message modifier */
    if ( (strcasecmp(name, "msg") == 0) ||
         (strcasecmp(name, "logdata") == 0) )
    {
        bool is_msg = toupper(*name) == 'M';
        const char *error_message;
        int error_offset;

        /* Check the parameter */
        rc = ib_rule_check_params(cp->ib, rule, value);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Error checking %s parameter value \"%s\": %s",
                             name,
                             value == NULL ? "" : value,
                             ib_status_to_string(rc));
            return rc;
        }

        rc = ib_var_expand_acquire(
            (is_msg ? &(rule->meta.msg) : &(rule->meta.data)),
            ib_rule_mm(cp->ib),
            IB_S2SL(value == NULL ? "" : value),
            ib_engine_var_config_get(cp->ib),
            &error_message, &error_offset
        );

        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                "Error creating %s expansion value \"%s\": %s (%s, %d)",
                name,
                value == NULL ? "" : value,
                ib_status_to_string(rc),
                error_message == NULL ? "NA" : error_message,
                error_message == NULL ? 0 : error_offset
            );
            return rc;
        }
        return IB_OK;
    }

    /* Tag modifier */
    if (strcasecmp(name, "tag") == 0) {
        rc = ib_list_push(rule->meta.tags, (void *)value);
        return rc;
    }

    /* Severity modifier */
    if (strcasecmp(name, "severity") == 0) {
        int severity = value ? atoi(value) : 0;

        if (severity > UINT8_MAX) {
            ib_cfg_log_error(cp, "Invalid severity: %s", value);
            return IB_EINVAL;
        }
        rule->meta.severity = (uint8_t)severity;
        return IB_OK;
    }

    /* Confidence modifier */
    if (strcasecmp(name, "confidence") == 0) {
        int confidence = value ? atoi(value) : 0;

        if (confidence > UINT8_MAX) {
            ib_cfg_log_error(cp, "Invalid confidence: %s", value);
            return IB_EINVAL;
        }
        rule->meta.confidence = (uint8_t)confidence;
        return IB_OK;
    }

    /* Revision modifier */
    if (strcasecmp(name, "rev") == 0) {
        int rev = value ? atoi(value) : 0;

        if ( (rev < 0) || (rev > UINT16_MAX) ) {
            ib_cfg_log_error(cp, "Invalid revision: %s", value);
            return IB_EINVAL;
        }
        rule->meta.revision = (uint16_t)rev;
        return IB_OK;
    }

    /* Phase modifiers (Not valid for stream rules) */
    if (! ib_rule_is_stream(rule)) {
        ib_rule_phase_num_t phase = IB_PHASE_NONE;
        if (strcasecmp(name, "phase") == 0) {
            if (value == NULL) {
                ib_cfg_log_error(cp, "Modifier PHASE with no value.");
                return IB_EINVAL;
            }
            phase = ib_rule_lookup_phase(value, false);
            if (phase == IB_PHASE_INVALID) {
                ib_cfg_log_error(cp, "Invalid phase: %s", value);
                return IB_EINVAL;
            }
        }
        else {
            ib_rule_phase_num_t tphase;
            tphase = ib_rule_lookup_phase(name, false);
            if (tphase != IB_PHASE_INVALID) {
                phase = tphase;
            }
        }

        /* If we encountered a phase modifier, set it */
        if (phase != IB_PHASE_NONE && phase != IB_PHASE_INVALID) {
            rc = ib_rule_set_phase(cp->ib, rule, phase);
            if (rc != IB_OK) {
                ib_cfg_log_error(cp, "Error setting rule phase: %s",
                                 ib_status_to_string(rc));
                return rc;
            }
            return IB_OK;
        }

        /* Not a phase modifier, so don't return */
    }

    /* Chain modifier */
    if ( (ib_rule_allow_chain(rule)) &&
         (strcasecmp(name, "chain") == 0) )
    {
        rc = ib_rule_set_chain(cp->ib, rule);
        return rc;
    }

    /* Capture modifier */
    if (strcasecmp(name, "capture") == 0) {
        rc = ib_rule_set_capture(cp->ib, rule, value);
        if (rc == IB_ENOTIMPL) {
            ib_cfg_log_error(cp, "Capture not supported by operator %s.",
                             ib_operator_get_name(rule->opinst->op));
            return IB_EINVAL;
        }
        else if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Error enabling capture: %s",
                             ib_status_to_string(rc));
            return rc;
        }
        return IB_OK;
    }

    /* Finally, try to match it to an action */
    rc = register_action_modifier(cp, rule, name, value);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error registering action \"%s\": %s",
                         name, ib_status_to_string(rc));
        return rc;
    }

    return rc;
}

/**
 * Parse a RuleExt directive.
 *
 * Register lua function. RuleExt lua:/path/to/rule.lua phase:REQUEST
 *
 * @param[in,out] cp Configuration parser that contains the engine being
 *                configured.
 * @param[in] name The directive name.
 * @param[in] vars The list of variables passed to @a name.
 * @param[in] cbdata User data. Unused.
 */
static ib_status_t parse_ruleext_params(ib_cfgparser_t *cp,
                                        const char *name,
                                        const ib_list_t *vars,
                                        void *cbdata)
{
    ib_status_t rc;
    const ib_list_node_t *targets;
    const ib_list_node_t *mod;
    ib_rule_t *rule;
    const char *file_name;
    const char *colon;
    const char *tag;
    const char *location;

    /* Get the targets string */
    targets = ib_list_first_const(vars);

    file_name = (const char *)ib_list_node_data_const(targets);

    if ( file_name == NULL ) {
        ib_cfg_log_error(cp, "No targets for rule.");
        return IB_EINVAL;
    }

    ib_cfg_log_debug3(cp, "Processing external rule: %s", file_name);

    /* Allocate a rule */
    rc = ib_rule_create(cp->ib, cp->cur_ctx,
                        cp->curr->file, cp->curr->line,
                        false, &rule);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error creating rule: %s",
                         ib_status_to_string(rc));
        return rc;
    }
    ib_flags_set(rule->flags, (IB_RULE_FLAG_EXTERNAL | IB_RULE_FLAG_NO_TGT));

    /* Parse all of the modifiers */
    mod = targets;
    while( (mod = ib_list_node_next_const(mod)) != NULL) {
        rc = parse_modifier(cp, rule, mod->data);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                "Error parsing external rule modifier \"%s\": %s",
                (const char *)mod->data,
                ib_status_to_string(rc)
            );
            return rc;
        }
    }

    /* Check the rule modifiers. */
    rc = check_rule_modifiers(cp, rule);
    if (rc != IB_OK) {
        return rc;
    }

    /* Using the rule->meta and file_name, load and stage the ext rule. */
    ib_rule_driver_t *driver;
    colon = strchr(file_name, ':');
    if (colon == NULL) {
        ib_cfg_log_error(cp,
            "Error parsing external rule location %s:  No colon found.",
            file_name
        );
        return IB_EINVAL;
    }
    tag = ib_mm_memdup_to_str(cp->mm, file_name, colon - file_name);
    if (tag == NULL) {
        return IB_EALLOC;
    }
    location = ib_util_relative_file(cp->mm, cp->curr->file, colon + 1);
    if (location == NULL) {
        return IB_EALLOC;
    }
    rc = ib_rule_lookup_external_driver(cp->ib, tag, &driver);
    if (rc != IB_ENOENT) {
        rc = driver->function(cp, rule, tag, location, driver->cbdata);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Error in external rule driver for \"%s\": %s",
                             tag, ib_status_to_string(rc)
            );
            return rc;
        }
    }
    else {
        ib_cfg_log_error(cp, "No external rule driver for \"%s\"", tag);
        return IB_EINVAL;
    }

    /* Finally, register the rule */
    rc = ib_rule_register(cp->ib, cp->cur_ctx, rule);
    if (rc == IB_EEXIST) {
        ib_cfg_log_warning(cp, "Not overwriting existing rule");
        return IB_OK;
    }
    else if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error registering rule: %s",
                         ib_status_to_string(rc));
        return rc;
    }

    /* Disable the entire chain if this rule is invalid */
    if ( (rule->flags & IB_RULE_FLAG_VALID) == 0) {
        rc = ib_rule_chain_invalidate(cp->ib, cp->cur_ctx, rule);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Error invalidating rule chain: %s",
                             ib_status_to_string(rc));
            return rc;
        }
    }

    ib_cfg_log_debug(cp,
                     "Registered external rule \"%s\" for "
                     "phase \"%s\" context \"%s\"",
                     ib_rule_id(rule),
                     ib_rule_phase_name(rule->meta.phase),
                     ib_context_full_get(cp->cur_ctx));

    /* Done */
    return IB_OK;
}

/**
 * Parse a Rule directive.
 *
 * Register a Rule directive to the engine.
 *
 * @param[in,out] cp Configuration parser that contains the engine being
 *                configured.
 * @param[in] name The directive name.
 * @param[in] vars The list of variables passed to @a name.
 * @param[in] cbdata User data. Unused.
 */
static ib_status_t parse_rule_params(ib_cfgparser_t *cp,
                                     const char *name,
                                     const ib_list_t *vars,
                                     void *cbdata)
{
    ib_status_t rc;
    const ib_list_node_t *node;
    const char *nodestr;
    const char *operator;
    const char *operand;
    ib_rule_t *rule = NULL;
    int targets = 0;

    if (cbdata != NULL) {
            }

    /* Allocate a rule */
    rc = ib_rule_create(cp->ib, cp->cur_ctx,
                        cp->curr->file, cp->curr->line,
                        false, &rule);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to allocate rule: %s",
                         ib_status_to_string(rc));
        goto cleanup;
    }

    /* Loop through the targets, stop when we encounter an operator */
    IB_LIST_LOOP_CONST(vars, node) {
        if (node->data == NULL) {
            ib_cfg_log_error(cp, "Found invalid rule target");
            rc = IB_EINVAL;
            goto cleanup;
        }
        nodestr = (const char *)node->data;
        if ( (*nodestr == '@') ||
             ((*nodestr != '\0') && (*(nodestr+1) == '@')) )
        {
            break;
        }
        rc = parse_target(cp, rule, nodestr);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Error parsing rule target \"%s\": %s",
                             nodestr, ib_status_to_string(rc));
            goto cleanup;
        }
        ++targets;
    }

    /* No targets??? */
    if (targets == 0) {
        ib_cfg_log_error(cp, "No rule targets found");
        rc = IB_EINVAL;
        goto cleanup;
    }

    /* Verify that we have an operator and operand */
    if ( (node == NULL) || (node-> data == NULL) ) {
        ib_cfg_log_error(cp, "No rule operator found");
        rc = IB_EINVAL;
        goto cleanup;
    }
    operator = (const char *)node->data;
    node = ib_list_node_next_const(node);
    if ( (node == NULL) || (node-> data == NULL) ) {
        ib_cfg_log_error(cp, "No rule operand found");
        rc = IB_EINVAL;
        goto cleanup;
    }
    operand = (const char *)node->data;

    /* Parse the operator */
    rc = parse_operator(cp, rule, operator, operand, false);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error parsing rule operator \"%s\": %s",
                         operator, ib_status_to_string(rc));
        goto cleanup;
    }

    /* Parse all of the modifiers */
    while( (node = ib_list_node_next_const(node)) != NULL) {
        if (node->data == NULL) {
            ib_cfg_log_error(cp, "Found invalid rule modifier");
            rc = IB_EINVAL;
            goto cleanup;
        }
        nodestr = (const char *)node->data;
        rc = parse_modifier(cp, rule, nodestr);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Error parsing rule modifier \"%s\": %s",
                             nodestr, ib_status_to_string(rc));
            goto cleanup;
        }
    }

    /* Check the rule modifiers. */
    rc = check_rule_modifiers(cp, rule);
    if (rc != IB_OK) {
        return rc;
    }

    /* Finally, register the rule */
    rc = ib_rule_register(cp->ib, cp->cur_ctx, rule);
    if (rc == IB_EEXIST) {
        ib_cfg_log_warning(cp, "Not overwriting existing rule");
        rc = IB_OK;
    }
    else if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error registering rule: %s",
                         ib_status_to_string(rc));
        goto cleanup;
    }

    /* Disable the entire chain if this rule is invalid */
cleanup:
    if ((rule != NULL) && ((rule->flags & IB_RULE_FLAG_VALID) == 0)) {
        ib_status_t irc = ib_rule_chain_invalidate(cp->ib, cp->cur_ctx, rule);
        if (irc != IB_OK) {
            ib_cfg_log_error(cp, "Error invalidating rule chain: %s",
                             ib_status_to_string(irc));
            return rc;
        }
        else {
            const char *chain = \
                rule->meta.chain_id == NULL ? "UNKNOWN" : rule->meta.chain_id;
            ib_cfg_log_debug2(cp,
                              "Invalidated all rules in chain \"%s\"",
                              chain);
        }
    }

    /* Done */
    return rc;
}

/**
 * Parse a StreamInspect directive.
 *
 * Register the StreamInspect directive to the engine.
 *
 * @param[in,out] cp Configuration parser that contains the engine being
 *                configured.
 * @param[in] name The directive name.
 * @param[in] vars The list of variables passed to @a name.
 * @param[in] cbdata User data. Unused.
 */
static ib_status_t parse_streaminspect_params(ib_cfgparser_t *cp,
                                              const char *name,
                                              const ib_list_t *vars,
                                              void *cbdata)
{
    ib_status_t rc;
    const ib_list_node_t *node;
    ib_rule_phase_num_t phase = IB_PHASE_INVALID;
    const char *str;
    const char *operator;
    const char *operand;
    ib_rule_t *rule;

    /* Get the phase string */
    node = ib_list_first_const(vars);
    if ( (node == NULL) || (node->data == NULL) ) {
        ib_cfg_log_error(cp, "No stream for StreamInspect");
        return IB_EINVAL;
    }
    str = node->data;

    /* Lookup the phase name */
    phase = ib_rule_lookup_phase(str, true);
    if (phase == IB_PHASE_INVALID) {
        ib_cfg_log_error(cp, "Invalid phase: %s", str);
        return IB_EINVAL;
    }

    /* Get the operator string */
    node = ib_list_node_next_const(node);
    if ( (node == NULL) || (node->data == NULL) ) {
        ib_cfg_log_error(cp, "No operator for rule");
        return IB_EINVAL;
    }
    operator = (const char *)node->data;

    /* Allocate a rule */
    rc = ib_rule_create(cp->ib, cp->cur_ctx,
                        cp->curr->file, cp->curr->line,
                        true, &rule);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to create rule: %s",
                         ib_status_to_string(rc));
        return rc;
    }
    ib_flags_set(rule->flags, IB_RULE_FLAG_NO_TGT);

    /* Set the rule's stream */
    rc = ib_rule_set_phase(cp->ib, rule, phase);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error setting rule phase: %s",
                         ib_status_to_string(rc));
        return rc;
    }

    /* Verify that we have an operand */
    node = ib_list_node_next_const(node);
    if ( (node == NULL) || (node-> data == NULL) ) {
        ib_cfg_log_error(cp, "No rule operand found");
        return rc;
    }
    operand = (const char *)node->data;

    /* Parse the operator */
    rc = parse_operator(cp, rule, operator, operand, true);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error parsing rule operator \"%s\": %s",
                         operator, ib_status_to_string(rc));
        return rc;
    }

    /* Parse all of the modifiers */
    while( (node = ib_list_node_next_const(node)) != NULL) {
        rc = parse_modifier(cp, rule, node->data);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Error parsing stream rule modifier \"%s\": %s",
                             (const char *)node->data,
                             ib_status_to_string(rc));
            return rc;
        }
    }

    /* Finally, register the rule */
    rc = ib_rule_register(cp->ib, cp->cur_ctx, rule);
    if (rc == IB_EEXIST) {
        ib_cfg_log_warning(cp, "Not overwriting existing rule.");
        return IB_OK;
    }
    else if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error registering rule: %s",
                         ib_status_to_string(rc));
        return rc;
    }

    /* Done */
    return IB_OK;
}

/**
 * Parse a RuleEnable directive.
 *
 * Handle the RuleEnable directive to the engine.
 *
 * @param[in,out] cp Configuration parser that contains the engine being
 *                configured.
 * @param[in] name The directive name.
 * @param[in] vars The list of variables passed to @c name.
 * @param[in] cbdata User data. Unused.
 */
static ib_status_t parse_ruleenable_params(ib_cfgparser_t *cp,
                                           const char *name,
                                           const ib_list_t *vars,
                                           void *cbdata)
{
    const ib_list_node_t *node;
    ib_status_t rc = IB_OK;

    if (cbdata != NULL) {
            }

    /* Loop through all of the parameters in the list */
    IB_LIST_LOOP_CONST(vars, node) {
        const char *param = (const char *)node->data;

        if (strcasecmp(param, "all") == 0) {
            rc = ib_rule_enable_all(cp->ib, cp->cur_ctx,
                                    cp->curr->file, cp->curr->line);
        }
        else if (strncasecmp(param, "id:", 3) == 0) {
            const char *id = param + 3;
            rc = ib_rule_enable_id(cp->ib, cp->cur_ctx,
                                   cp->curr->file, cp->curr->line,
                                   id);
        }
        else if (strncasecmp(param, "tag:", 4) == 0) {
            const char *tag = param + 4;
            rc = ib_rule_enable_tag(cp->ib, cp->cur_ctx,
                                    cp->curr->file, cp->curr->line,
                                    tag);
        }
        else {
            ib_cfg_log_error(cp, "Invalid %s parameter \"%s\"", name, param);
            rc = IB_EINVAL;
            continue;
        }
    }

    /* Done */
    return rc;
}

/**
 * Parse a RuleDisable directive.
 *
 * Handle the RuleDisable directive to the engine.
 *
 * @param[in,out] cp Configuration parser that contains the engine being
 *                configured.
 * @param[in] name The directive name.
 * @param[in] vars The list of variables passed to @c name.
 * @param[in] cbdata User data. Unused.
 */
static ib_status_t parse_ruledisable_params(ib_cfgparser_t *cp,
                                            const char *name,
                                            const ib_list_t *vars,
                                            void *cbdata)
{
    const ib_list_node_t *node;
    ib_status_t rc = IB_OK;

    if (cbdata != NULL) {
            }

    /* Loop through all of the parameters in the list */
    IB_LIST_LOOP_CONST(vars, node) {
        const char *param = (const char *)node->data;

        if (strcasecmp(param, "all") == 0) {
            rc = ib_rule_disable_all(cp->ib, cp->cur_ctx,
                                     cp->curr->file, cp->curr->line);
        }
        else if (strncasecmp(param, "id:", 3) == 0) {
            const char *id = param + 3;
            rc = ib_rule_disable_id(cp->ib, cp->cur_ctx,
                                    cp->curr->file, cp->curr->line,
                                    id);
        }
        else if (strncasecmp(param, "tag:", 4) == 0) {
            const char *tag = param + 4;
            rc = ib_rule_disable_tag(cp->ib, cp->cur_ctx,
                                     cp->curr->file, cp->curr->line,
                                     tag);
        }
        else {
            ib_cfg_log_error(cp, "Invalid %s parameter \"%s\"", name, param);
            rc = IB_EINVAL;
            continue;
        }
    }

    /* Done */
    return rc;
}

/**
 * Parse a RuleMarker directive.
 *
 * Register a RuleMarker directive to the engine.
 *
 * @param[in,out] cp Configuration parser that contains the engine being
 *                configured.
 * @param[in] name The directive name.
 * @param[in] vars The list of variables passed to @a name.
 * @param[in] cbdata User data. Unused.
 */
static ib_status_t parse_rulemarker_params(ib_cfgparser_t *cp,
                                           const char *name,
                                           const ib_list_t *vars,
                                           void *cbdata)
{
    ib_status_t rc;
    const ib_list_node_t *node;
    ib_rule_t *rule = NULL;

    if (cbdata != NULL) {
            }

    /* Allocate a rule */
    rc = ib_rule_create(cp->ib, cp->cur_ctx,
                        cp->curr->file, cp->curr->line,
                        false, &rule);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error allocating rule: %s",
                         ib_status_to_string(rc));
        return rc;
    }
    ib_flags_set(rule->flags, IB_RULE_FLAG_ACTION);

    /* Force the operator to one that will not execute (negated nop). */
    rc = parse_operator(cp, rule, "!@nop", NULL, false);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error parsing rule operator \"nop\": %s",
                         ib_status_to_string(rc));
        return rc;
    }

    /* Parse all of the modifiers, only allowing id and phase. */
    IB_LIST_LOOP_CONST(vars, node) {
        const char *param = (const char *)node->data;

        if (   (strncasecmp(param, "id:", 3) == 0)
            || (strncasecmp(param, "phase:", 6) == 0))
        {
            rc = parse_modifier(cp, rule, node->data);
            if (rc != IB_OK) {
                ib_cfg_log_error(cp,
                                 "Error parsing %s modifier \"%s\": %s",
                                 name,
                                 (const char *)node->data,
                                 ib_status_to_string(rc));
                return rc;
            }
        }
        else {
            ib_cfg_log_error(cp, "Invalid %s parameter \"%s\"", name, param);
            return IB_EINVAL;
        }
    }

    /* Force a zero revision so it can always be overridden. */
    rc = parse_modifier(cp, rule, "rev:0");
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error parsing %s modifier \"rev:0\": %s",
                         name,
                         ib_status_to_string(rc));
        return rc;
    }

    /* Check the rule modifiers. */
    rc = check_rule_modifiers(cp, rule);
    if (rc != IB_OK) {
        return rc;
    }

    /* Finally, register the rule. */
    rc = ib_rule_register(cp->ib, cp->cur_ctx, rule);
    if (rc == IB_EEXIST) {
        ib_cfg_log_notice(cp, "Not overwriting existing rule.");
        rc = IB_OK;
    }
    else if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error registering rule marker: %s",
                         ib_status_to_string(rc));
    }

    return rc;
}

/**
 * Parse an Action directive.
 *
 * Register an Action directive to the engine.
 *
 * @param[in,out] cp Configuration parser that contains the engine being
 *                configured.
 * @param[in] name The directive name.
 * @param[in] vars The list of variables passed to @a name.
 * @param[in] cbdata User data. Unused.
 */
static ib_status_t parse_action_params(ib_cfgparser_t *cp,
                                       const char *name,
                                       const ib_list_t *vars,
                                       void *cbdata)
{
    ib_status_t rc;
    const ib_list_node_t *node;
    ib_rule_t *rule = NULL;

    if (cbdata != NULL) {
            }

    /* Allocate a rule */
    rc = ib_rule_create(cp->ib, cp->cur_ctx,
                        cp->curr->file, cp->curr->line,
                        false, &rule);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error allocating rule: %s",
                         ib_status_to_string(rc));
        goto cleanup;
    }
    ib_flags_set(rule->flags, IB_RULE_FLAG_ACTION);

    /* Parse the operator */
    rc = parse_operator(cp, rule, "@nop", NULL, false);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error parsing rule operator \"nop\": %s",
                         ib_status_to_string(rc));
        goto cleanup;
    }

    /* Parse all of the modifiers */
    IB_LIST_LOOP_CONST(vars, node) {
        rc = parse_modifier(cp, rule, node->data);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Error parsing action modifier \"%s\": %s",
                             (const char *)node->data,
                             ib_status_to_string(rc));
            goto cleanup;
        }
    }

    /* Check the rule modifiers. */
    rc = check_rule_modifiers(cp, rule);
    if (rc != IB_OK) {
        return rc;
    }

    /* Finally, register the rule */
    rc = ib_rule_register(cp->ib, cp->cur_ctx, rule);
    if (rc == IB_EEXIST) {
        ib_cfg_log_warning(cp, "Not overwriting existing rule.");
        rc = IB_OK;
    }
    else if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error registering rule: %s",
                         ib_status_to_string(rc));
        goto cleanup;
    }

    /* Disable the entire chain if this rule is invalid */
cleanup:
    if ((rule != NULL) && ((rule->flags & IB_RULE_FLAG_VALID) == 0)) {
        ib_status_t irc = ib_rule_chain_invalidate(cp->ib, cp->cur_ctx, rule);
        if (irc != IB_OK) {
            ib_cfg_log_error(cp, "Error invalidating rule chain: %s",
                             ib_status_to_string(irc));
            return rc;
        }
        else {
            const char *chain = \
                rule->meta.chain_id == NULL ? "UNKNOWN" : rule->meta.chain_id;
            ib_cfg_log_debug2(cp,
                              "Invalidated all rules in chain \"%s\".",
                              chain);
        }
    }

    /* Done */
    return rc;
}

/* ifdef is because only rule tracing code currently uses this method.  Remove
 * ifdef if used outside of rule tracing. */
#ifdef IB_RULE_TRACE

/**
 * Fetch per-context configuration for @a ctx.
 *
 * @param[in] ctx Context to fetch configuration for.
 * @return Fetch configuration.
 **/
static
per_context_t *fetch_per_context(ib_context_t *ctx)
{
    assert(ctx != NULL);

    ib_status_t    rc;
    per_context_t *per_context = NULL;
    ib_module_t   *module      = NULL;

    rc = ib_engine_module_get(
        ib_context_get_engine(ctx),
        MODULE_NAME_STR,
        &module
    );
    assert(rc == IB_OK);

    rc = ib_context_module_config(ctx, module, &per_context);
    assert(rc == IB_OK);

    return per_context;
}
#endif

/**
 * Parse RuleTrace directive.
 *
 * Outputs warning if IB_RULE_TRACE not defined.
 *
 * @param[in] cp Configuration parser.
 * @param[in] name Name of directive.
 * @param[in] rule_id Parameter; ID of rule to trace.
 * @param[in] cbdata Callback data; Unused.
 * @returns IB_OK on success; IB_E* on error.
 **/
static
ib_status_t parse_ruletrace_params(
    ib_cfgparser_t *cp,
    const char     *name,
    const char     *rule_id,
    void           *cbdata
)
{
    assert(cp != NULL);
    assert(rule_id != NULL);

#ifdef IB_RULE_TRACE
    ib_mm_t mm = ib_engine_mm_main_get(cp->ib);
    ib_status_t rc;
    ib_rule_t *rule;
    per_context_t *per_context = fetch_per_context(cp->cur_ctx);

    rc = ib_rule_lookup(cp->ib, cp->cur_ctx, rule_id, &rule);
    if (rc == IB_ENOENT) {
        ib_cfg_log_error(
            cp,
            "RuleTrace could not find rule with id: %s",
            rule_id
        );
        return IB_ENOENT;
    }
    else if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "RuleTrace experienced unexpected error looking up rule: %s (%s)",
            rule_id, ib_status_to_string(rc)
        );
        return rc;
    }

    ib_flags_set(rule->flags, IB_RULE_FLAG_TRACE);

    if (per_context->trace_rules == NULL) {
        rc = ib_list_create(&per_context->trace_rules, mm);
        if (rc != IB_OK) {
            ib_cfg_log_error(
                cp,
                "RuleTrace could not create traced rules list: %s (%s)",
                rule_id, ib_status_to_string(rc)
            );
            return rc;
        }
    }
    rc = ib_list_push(per_context->trace_rules, rule);
    if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "RuleTrace could not pushed traced rules: %s (%s)",
            rule_id, ib_status_to_string(rc)
        );
        return rc;
    }

    return IB_OK;
#else
    ib_cfg_log_warning(
        cp,
        "IronBee compiled without rule tracing.  "
        "RuleTrace will have no effect."
    );
    return IB_OK;
#endif
}

/**
 * Parse RuleTraceFile directive.
 *
 * Outputs warning if IB_RULE_TRACE not defined.
 *
 * @param[in] cp Configuration parser.
 * @param[in] name Name of directive.
 * @param[in] path Path to write traces to.
 * @param[in] cbdata Callback data; Unused.
 * @returns IB_OK on success; IB_E* on error.
 **/
static
ib_status_t parse_ruletracefile_params(
    ib_cfgparser_t *cp,
    const char     *name,
    const char     *path,
    void           *cbdata
)
{
#ifdef IB_RULE_TRACE
    per_context_t *per_context = fetch_per_context(cp->cur_ctx);

    per_context->trace_path = path;

    return IB_OK;
#else
    ib_cfg_log_warning(
        cp,
        "IronBee compiled without rule tracing.  "
        "RuleTraceFile will have no effect."
    );
    return IB_OK;
#endif
}

/**
 * Handle postprocessing.
 *
 * Does nothing unless IB_RULE_TRACE is defined, in which case, outputs
 * traces.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx Transaction.
 * @param[in] event Current event.
 * @param[in] cbdata Callback data; unused.
 * @returns IB_OK on success; IB_EOTHER on file system failure.
 **/
static
ib_status_t postprocess(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    void *cbdata
)
{
    assert(ib != NULL);
    assert(tx != NULL);

#ifdef IB_RULE_TRACE
    per_context_t *per_context = fetch_per_context(tx->ctx);
    if (per_context->trace_rules != NULL) {
        ib_list_node_t *node;
        FILE *trace_fp = stderr;
        if (per_context->trace_path) {
            trace_fp = fopen(per_context->trace_path, "a");
            if (trace_fp == NULL) {
                ib_log_error_tx(
                    tx,
                    "Failed to open trace file: %s",
                    per_context->trace_path
                );
                return IB_EOTHER;
            }
        }

        IB_LIST_LOOP(per_context->trace_rules, node) {
            const ib_rule_t *rule =
                (const ib_rule_t *)ib_list_node_data_const(node);

            fprintf(trace_fp, "%s,%d,%s,%d,%s,%s,%zd,%" PRIu64 "\n",
                tx->conn->local_ipstr, tx->conn->local_port,
                tx->conn->remote_ipstr, tx->conn->remote_port,
                tx->id,
                rule->meta.full_id,
                tx->rule_exec->traces[rule->meta.index].evaluation_n,
                tx->rule_exec->traces[rule->meta.index].evaluation_time
            );
        }
    }
#endif

    return IB_OK;
}

/**
 * Initialize module.
 *
 * @param[in] ib IronBee engine.
 * @param[in] m Module.
 * @param[in] cbdata Callback data; unused.
 * @returns IB_OK
 **/
static
ib_status_t rules_init(
    ib_engine_t *ib,
    ib_module_t *m,
    void        *cbdata
)
{
    assert(ib != NULL);

    ib_hook_tx_register(
        ib,
        handle_postprocess_event,
        postprocess,  NULL
    );

    return IB_OK;
}

static IB_DIRMAP_INIT_STRUCTURE(rules_directive_map) = {

    /* Give the config parser a callback for the Rule and RuleExt directive */
    IB_DIRMAP_INIT_LIST(
        "Rule",
        parse_rule_params,
        NULL
    ),

    IB_DIRMAP_INIT_LIST(
        "RuleExt",
        parse_ruleext_params,
        NULL
    ),

    IB_DIRMAP_INIT_LIST(
        "RuleMarker",
        parse_rulemarker_params,
        NULL
    ),

    IB_DIRMAP_INIT_LIST(
        "StreamInspect",
        parse_streaminspect_params,
        NULL
    ),

    IB_DIRMAP_INIT_LIST(
        "RuleEnable",
        parse_ruleenable_params,
        NULL
    ),

    IB_DIRMAP_INIT_LIST(
        "RuleDisable",
        parse_ruledisable_params,
        NULL
    ),

    IB_DIRMAP_INIT_LIST(
        "Action",
        parse_action_params,
        NULL
    ),

    IB_DIRMAP_INIT_PARAM1(
        "RuleTrace",
        parse_ruletrace_params,
        NULL
    ),

    IB_DIRMAP_INIT_PARAM1(
        "RuleTraceFile",
        parse_ruletracefile_params,
        NULL
    ),

    /* signal the end of the list */
    IB_DIRMAP_INIT_LAST
};

/* Initialize the module structure. */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /* Default metadata */
    MODULE_NAME_STR,                     /* Module name */
    IB_MODULE_CONFIG(&c_per_context_initial), /* Global config data */
    NULL,                                /* Configuration field map */
    rules_directive_map,                 /* Config directive map */
    rules_init,                          /* Initialize function */
    NULL,                                /* Callback data */
    NULL,                                /* Finish function */
    NULL,                                /* Callback data */
);
