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

#include "ironbee_config_auto.h"


#ifdef ENABLE_LUA
#include "rules_lua_private.h"
#include "lua/ironbee.h"
#endif

#include <ironbee/action.h>
#include <ironbee/cfgmap.h>
#include <ironbee/config.h>
#include <ironbee/core.h>
#include <ironbee/debug.h>
#include <ironbee/engine.h>
#include <ironbee/list.h>
#include <ironbee/lock.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/operator.h>
#include <ironbee/provider.h>
#include <ironbee/rule_engine.h>
#include <ironbee/util.h>

#ifdef ENABLE_LUA
#include <lua.h>
#endif

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

/**
 * Phase lookup table.
 */
typedef struct {
    const char          *str;
    bool                 is_stream;
    ib_rule_phase_num_t  phase;
} phase_lookup_t;
static phase_lookup_t phase_lookup_table[] =
{
    /* Standard phases */
    { "REQUEST_HEADER",          false, PHASE_REQUEST_HEADER },
    { "REQUEST",                 false, PHASE_REQUEST_BODY },
    { "RESPONSE_HEADER",         false, PHASE_RESPONSE_HEADER },
    { "RESPONSE",                false, PHASE_RESPONSE_BODY },
    { "POSTPROCESS",             false, PHASE_POSTPROCESS },
    /* Stream inspection phases */
    { "REQUEST_HEADER_STREAM",   true,  PHASE_STR_REQUEST_HEADER },
    { "REQUEST_BODY_STREAM",     true,  PHASE_STR_REQUEST_BODY },
    { "RESPONSE_HEADER_STREAM",  true,  PHASE_STR_RESPONSE_HEADER },
    { "RESPONSE_BODY_STREAM",    true,  PHASE_STR_RESPONSE_BODY },
    /* List terminator */
    { NULL,                      false, PHASE_INVALID },
};

#ifdef ENABLE_LUA
/**
 * Ironbee's root rule state.
 */
static lua_State *g_ironbee_rules_lua;

/**
 * @brief Semaphore ID used to protect Lua thread creation and destruction.
 */
static ib_lock_t g_lua_lock;
#endif

/**
 * Lookup a phase name in the phase name table.
 *
 * @param[in] str Phase name string to lookup
 * @param[in] is_stream true if this is a stream phase
 * @param[out] phase Phase number
 *
 * @returns Status code
 */
static ib_status_t lookup_phase(const char *str,
                                bool is_stream,
                                ib_rule_phase_num_t *phase)
{
    IB_FTRACE_INIT();
    const phase_lookup_t *item;

    for (item = phase_lookup_table;  item->str != NULL;  ++item) {
         if (strcasecmp(str, item->str) == 0) {
             *phase = item->phase;
             IB_FTRACE_RET_STATUS(IB_OK);
         }
    }
    IB_FTRACE_RET_STATUS(IB_EINVAL);
}

#ifdef ENABLE_LUA
/**
 * @brief Callback type for functions executed protected by g_lua_lock.
 * @details This callback should take a @c ib_engine_t* which is used
 *          for logging, @c a lua_State* which is used to create the
 *          new thread, and a @c lua_State** which will be assigned a
 *          new @c lua_State*.
 */
typedef ib_status_t(*critical_section_fn_t)(ib_engine_t *ib,
                                            lua_State *parent,
                                            lua_State **out_new);
#endif

/**
 * Parse rule's operator.
 *
 * Parses the rule's operator and operand strings (@a operator and @a
 * operand), and stores the results in the rule object @a rule.
 *
 * @param cp IronBee configuration parser
 * @param rule Rule object to update
 * @param operator Operator string
 * @param operand Operand string
 *
 * @returns Status code
 */
static ib_status_t parse_operator(ib_cfgparser_t *cp,
                                  ib_rule_t *rule,
                                  const char *operator,
                                  const char *operand)
{
    IB_FTRACE_INIT();
    assert(cp != NULL);
    assert(rule != NULL);
    assert(operator != NULL);

    ib_status_t rc = IB_OK;
    const char *opname = NULL;
    const char *cptr = operator;
    ib_flags_t flags = IB_OPINST_FLAG_NONE;
    ib_operator_inst_t *opinst;

    /* Leading '!' (invert flag)? */
    if (*cptr == '!') {
        flags |= IB_OPINST_FLAG_INVERT;
        ++cptr;
    }

    /* Better be an '@' next... */
    if ( (*cptr != '@') || (isalpha(*(cptr+1)) == 0) ) {
        ib_cfg_log_error(cp, "Invalid rule syntax \"%s\"", operator);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    opname = cptr + 1;

    /* Create the operator instance */
    rc = ib_operator_inst_create(cp->ib,
                                 cp->cur_ctx,
                                 rule,
                                 ib_rule_required_op_flags(rule),
                                 opname,
                                 operand,
                                 flags,
                                 &opinst);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Failed to create operator instance "
                         "operator=\"%s\" operand=\"%s\": %s",
                         opname,
                         operand == NULL ? "" : operand,
                         ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Set the operator */
    rc = ib_rule_set_operator(cp->ib, rule, opinst);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Failed to set operator for rule: %s",
                         ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_cfg_log_debug3(cp,
                      "Rule: operator=\"%s\" operand=\"%s\" "
                      "flags=0x%04x",
                      operator,
                      (operand == NULL) ? "" : operand,
                      flags);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Rewrite the target string if required
 *
 * Parses the rule's target field list string @a target_str, looking for
 * the '#' and '&' tokens at the start of it.
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
    IB_FTRACE_INIT();
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
        else if (*cur == '#') {
            ops[count] = ".length()";
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
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /**
     * Allocate & build the new string
     */
    new = ib_mpool_alloc(cp->mp, target_len);
    if (new == NULL) {
        ib_cfg_log_error(cp,
                         "Failed to duplicate target field string \"%s\"",
                         target_str);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Add the functions in reverse order */
    strcpy(new, target_str+count);
    for (n = count-1;  n >= 0;  --n) {
        strcat(new, ops[n] );
    }

    /* Log our rewrite */
    ib_cfg_log_debug3(cp, "Rewrote \"%s\" -> \"%s\"", target_str, new);

    /* Done */
    *rewritten = new;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Parse the transformations from a target string
 *
 * @param[in] cp Configuration parser
 * @param[in] str Target field string to parse
 * @param[out] target Target name
 * @param[out] tfns List of transformation names
 *
 * @returns Status code
 */
static ib_status_t parse_target_string(ib_cfgparser_t *cp,
                                       const char *str,
                                       const char **target,
                                       ib_list_t **tfns)
{
    IB_FTRACE_INIT();
    ib_status_t  rc;
    char        *cur;                /* Current position */
    char        *dup_str;            /* Duplicate string */

    assert(cp != NULL);
    assert(str != NULL);
    assert(target != NULL);

    /* Start with a known state */
    *target = NULL;
    *tfns = NULL;

    /* No parens?  Just store the target string as the field name & return. */
    if (strstr(str, "()") == NULL) {
        *target = str;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Make a duplicate of the target string to work on */
    dup_str = ib_mpool_strdup(ib_rule_mpool(cp->ib), str);
    if (dup_str == NULL) {
        ib_cfg_log_error(cp, "Error duplicating target string \"%s\"", str);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Walk through the string */
    cur = dup_str;
    while (cur != NULL) {
        char  *separator;       /* Current separator */
        char  *parens = NULL;   /* Paren pair '()' */
        char  *pdot = NULL;     /* Paren pair + dot '().' */
        char  *tfn = NULL;      /* Transformation name */

        /* First time through the loop? */
        if (cur == dup_str) {
            separator = strchr(cur, '.');
            if (separator == NULL) {
                break;
            }
            *separator = '\0';
            tfn = separator + 1;
        }
        else {
            separator = cur;
            tfn = separator;
        }

        /* Find the next separator and paren set */
        parens = strstr(separator+1, "()");
        pdot = strstr(separator+1, "().");

        /* Parens + dot: intermediate transformation */
        if (pdot != NULL) {
            *pdot = '\0';
            *(pdot+2) = '\0';
            cur = pdot + 3;
        }
        /* Parens but no dot: last transformation */
        else if (parens != NULL) {
            *parens = '\0';
            cur = NULL;
        }
        /* Finally, no parens: done */
        else {
            cur = NULL;
            tfn = NULL;
        }

        /* Skip to top of loop if there's no operator */
        if (tfn == NULL) {
            continue;
        }

        /* Create the transformation list if required. */
        if (*tfns == NULL) {
            rc = ib_list_create(tfns, ib_rule_mpool(cp->ib));
            if (rc != IB_OK) {
                ib_cfg_log_error(cp,
                                 "Error creating transformation list: %s",
                                 ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }
        }

        /* Add the name to the list */
        rc = ib_list_push(*tfns, tfn);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Error adding transformation \"%s\" to list: %s",
                             tfn, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /**
     * The field name is the start of the duplicate string, even after
     * it's been chopped up into pieces.
     */
    *target = dup_str;
    ib_cfg_log_debug3(cp, "Final target field name is \"%s\"", *target);

    IB_FTRACE_RET_STATUS(IB_OK);
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
    IB_FTRACE_INIT();
    ib_status_t rc;
    const char *rewritten_target_str = NULL;
    const char *final_target_str; /* Holder for the final target name. */
    ib_list_t *tfns;              /* Transformations to perform. */
    ib_rule_target_t *ib_rule_target;
    int not_found = 0;
    int rewrites;

    /* First, rewrite cur into rewritten_target_str. */
    rc = rewrite_target_tokens(cp, target_str,
                               &rewritten_target_str, &rewrites);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error rewriting target \"%s\"", target_str);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Parse the rewritten string into the final_target_str. */
    rc = parse_target_string(cp,
                             rewritten_target_str,
                             &final_target_str,
                             &tfns);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error parsing target string \"%s\": %s",
                         target_str, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Create the target object */
    rc = ib_rule_create_target(cp->ib,
                               target_str,
                               final_target_str,
                               tfns,
                               &ib_rule_target,
                               &not_found);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error creating rule target \"%s\": %s",
                         final_target_str, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (not_found != 0) {
        ib_cfg_log_error(cp,
            "Rule target \"%s\": %d transformations not found",
            final_target_str, not_found
        );
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Add the target to the rule */
    rc = ib_rule_add_target(cp->ib, rule, ib_rule_target);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to add rule target \"%s\"", target_str);
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_cfg_log_debug3(cp, "Added rule target \"%s\" to rule", target_str);

    IB_FTRACE_RET_STATUS(IB_OK);
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
    IB_FTRACE_INIT();
    ib_status_t        rc = IB_OK;
    ib_action_inst_t  *action;
    ib_rule_action_t   atype = RULE_ACTION_TRUE;
    if (*name == '!') {
        ++name;
        atype = RULE_ACTION_FALSE;
    }

    /* Create a new action instance */
    rc = ib_action_inst_create(cp->ib,
                               cp->cur_ctx,
                               name,
                               params,
                               IB_ACTINST_FLAG_NONE,
                               &action);
    if (rc == IB_ENOENT) {
        ib_cfg_log_notice(cp, "Ignoring unknown modifier \"%s\"", name);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Failed to create action instance \"%s\": %s",
                         name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Add the action to the rule */
    rc = ib_rule_add_action(cp->ib, rule, action, atype);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Failed to add action \"%s\" to rule \"%s\": %s",
                         name, ib_rule_id(rule), ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
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
    IB_FTRACE_INIT();
    bool child = ib_flags_all(rule->flags, IB_RULE_FLAG_CHCHILD);

    if ( (! child) && (ib_rule_id(rule) == NULL) )
    {
        ib_cfg_log_error(cp, "No rule id specified (flags=0x%04x)",
                         rule->flags);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if ( (! child) &&
         ((rule->meta.phase == PHASE_INVALID) ||
          (rule->meta.phase == PHASE_NONE)) )
    {
        ib_cfg_log_error(cp, "Phase invalid or not specified.");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
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
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    const char *name;
    char *colon;
    char *copy;
    const char *value = NULL;

    assert(cp != NULL);
    assert(rule != NULL);
    assert(modifier_str != NULL);

    /* Copy the string */
    copy = ib_mpool_strdup(ib_rule_mpool(cp->ib), modifier_str);
    if (copy == NULL) {
        ib_cfg_log_error(cp,
                         "Failed to copy rule modifier \"%s\"", modifier_str);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
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
            ib_cfg_log_error(cp, "Modifier ID with no value");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        rc = ib_rule_set_id(cp->ib, rule, value);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Message modifier */
    if (strcasecmp(name, "msg") == 0) {
        bool expand = false;
        rule->meta.msg = value;
        rc = ib_data_expand_test_str(value, &expand);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Expansion test failed: %d", rc);
            IB_FTRACE_RET_STATUS(rc);
        }
        if (expand) {
            rule->meta.flags |= IB_RULEMD_FLAG_EXPAND_MSG;
        }
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* LogData modifier */
    if (strcasecmp(name, "logdata") == 0) {
        bool expand = false;
        rule->meta.data = value;
        rc = ib_data_expand_test_str(value, &expand);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Expansion test failed: %d", rc);
            IB_FTRACE_RET_STATUS(rc);
        }
        if (expand) {
            rule->meta.flags |= IB_RULEMD_FLAG_EXPAND_DATA;
        }
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Tag modifier */
    if (strcasecmp(name, "tag") == 0) {
        rc = ib_list_push(rule->meta.tags, (void *)value);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Severity modifier */
    if (strcasecmp(name, "severity") == 0) {
        int severity = value ? atoi(value) : 0;

        if (severity > UINT8_MAX) {
            ib_cfg_log_error(cp, "Invalid severity: %s", value);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        rule->meta.severity = (uint8_t)severity;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Confidence modifier */
    if (strcasecmp(name, "confidence") == 0) {
        int confidence = value ? atoi(value) : 0;

        if (confidence > UINT8_MAX) {
            ib_cfg_log_error(cp, "Invalid confidence: %s", value);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        rule->meta.confidence = (uint8_t)confidence;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Revision modifier */
    if (strcasecmp(name, "rev") == 0) {
        int rev = value ? atoi(value) : 0;

        if ( (rev < 0) || (rev > UINT16_MAX) ) {
            ib_cfg_log_error(cp, "Invalid revision: %s", value);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        rule->meta.revision = (uint16_t)rev;
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Phase modifiers (Not valid for stream rules) */
    if (! ib_rule_is_stream(rule)) {
        ib_rule_phase_num_t phase = PHASE_NONE;
        if (strcasecmp(name, "phase") == 0) {
            if (value == NULL) {
                ib_cfg_log_error(cp, "Modifier PHASE with no value");
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }
            rc = lookup_phase(value, false, &phase);
            if (rc != IB_OK) {
                ib_cfg_log_error(cp, "Invalid phase: %s", value);
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }
        }
        else {
            ib_rule_phase_num_t tphase;
            rc = lookup_phase(name, false, &tphase);
            if (rc == IB_OK) {
                phase = tphase;
            }
        }

        /* If we encountered a phase modifier, set it */
        if (phase != PHASE_NONE) {
            rc = ib_rule_set_phase(cp->ib, rule, phase);
            if (rc != IB_OK) {
                ib_cfg_log_error(cp, "Error setting rule phase: %s",
                                 ib_status_to_string(rc));
                IB_FTRACE_RET_STATUS(rc);
            }
            IB_FTRACE_RET_STATUS(IB_OK);
        }

        /* Not a phase modifier, so don't return */
    }

    /* Chain modifier */
    if ( (ib_rule_allow_chain(rule)) &&
         (strcasecmp(name, "chain") == 0) )
    {
        rc = ib_rule_set_chain(cp->ib, rule);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Capture modifier */
    if (strcasecmp(name, "capture") == 0) {
        if (ib_flags_any(rule->opinst->op->flags, IB_OP_FLAG_CAPTURE)) {
            rule->flags |= IB_RULE_FLAG_CAPTURE;
            IB_FTRACE_RET_STATUS(IB_OK);
        }
        else {
            ib_cfg_log_error(cp, "Capture not supported by operator %s",
                             rule->opinst->op->name);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    /* Transformation modifiers */
    if (strcasecmp(name, "t") == 0) {
        if (! ib_rule_allow_tfns(rule)) {
            ib_cfg_log_error(cp,
                "Transformations not supported for this rule"
            );
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        if (value == NULL) {
            ib_cfg_log_error(cp, "Modifier transformation with no value");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        rc = ib_rule_add_tfn(cp->ib, rule, value);
        if (rc == IB_ENOENT) {
            ib_cfg_log_error(cp, "Unknown transformation: \"%s\"", value);
            IB_FTRACE_RET_STATUS(rc);
        }
        else if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Error adding transformation \"%s\": %s",
                             value, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Finally, try to match it to an action */
    rc = register_action_modifier(cp, rule, name, value);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error registering action \"%s\": %s",
                         value, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(rc);
}

#ifdef ENABLE_LUA
/**
 * @brief This will use @c g_lua_lock to atomically call @a fn.
 * @details The argument @a fn will be either
 *          ib_lua_new_thread(ib_engine_t*, lua_State**) or
 *          ib_lua_join_thread(ib_engine_t*, lua_State**) which will be called
 *          only if @c g_lua_lock can be locked using @c semop.
 * @param[in] ib IronBee context. Used for logging.
 * @param[in] fn The function to execute. This is passed @a ib and @a fn.
 * @param[in,out] L The Lua State to create or destroy. Passed to @a fn.
 * @returns If any error locking or unlocking the
 *          semaphore is encountered, the error code is returned.
 *          Otherwise the result of @a fn is returned.
 */
static ib_status_t call_in_critical_section(ib_engine_t *ib,
                                            critical_section_fn_t fn,
                                            lua_State **L)
{
    IB_FTRACE_INIT();

    /* Return code from IronBee calls. */
    ib_status_t ib_rc;
    /* Return code form critical call. */
    ib_status_t critical_rc;

    ib_rc  = ib_lock_lock(&g_lua_lock);

    /* Report semop error and return. */
    if (ib_rc != IB_OK) {
        ib_log_error(ib, "Failed to lock Lua context.");
        IB_FTRACE_RET_STATUS(ib_rc);
    }

    /* Execute lua call in critical section. */
    critical_rc = fn(ib, g_ironbee_rules_lua, L);

    ib_rc = ib_lock_unlock(&g_lua_lock);

    if (critical_rc != IB_OK) {
        ib_log_error(ib, "Critical call failed: %s",
                     ib_status_to_string(critical_rc));
    }

    /* Report semop error and return. */
    if (ib_rc != IB_OK) {
        ib_log_error(ib, "Failed to unlock Lua context.");
        IB_FTRACE_RET_STATUS(ib_rc);
    }

    IB_FTRACE_RET_STATUS(critical_rc);
}

/**
 * @brief Call the rule named @a func_name on a new Lua stack.
 * @details This will atomically create and destroy a lua_State*
 *          allowing for concurrent execution of @a func_name
 *          by a ib_lua_func_eval(ib_engine_t*, ib_txt_t*, const char*).
 * @param[in] ib IronBee context.
 * @param[in,out] tx The transaction. The Rule may color this with data.
 * @param[in] func_name The Lua function name to call.
 * @param[out] result The result integer value. This should be set to
 *             1 (true) or 0 (false).
 * @returns IB_OK on success, IB_EUNKNOWN on semaphore locking error, and
 *          IB_EALLOC is returned if a new execution stack cannot be created.
 */
static ib_status_t ib_lua_func_eval_r(ib_engine_t *ib,
                                      ib_tx_t *tx,
                                      const char *func_name,
                                      ib_num_t *result)
{
    IB_FTRACE_INIT();

    int result_int;
    ib_status_t ib_rc;
    lua_State *L;

    /* Atomically create a new Lua stack */
    ib_rc = call_in_critical_section(ib, &ib_lua_new_thread, &L);

    if (ib_rc != IB_OK) {
        IB_FTRACE_RET_STATUS(ib_rc);
    }

    /* Call the rule in isolation. */
    ib_rc = ib_lua_func_eval_int(ib, tx, L, func_name, &result_int);

    /* Convert the passed in integer type to an ib_num_t. */
    *result = result_int;

    if (ib_rc != IB_OK) {
        IB_FTRACE_RET_STATUS(ib_rc);
    }

    /* Atomically destroy the Lua stack */
    ib_rc = call_in_critical_section(ib, &ib_lua_join_thread, &L);

    IB_FTRACE_RET_STATUS(ib_rc);
}

static ib_status_t lua_operator_create(ib_engine_t *ib,
                                       ib_context_t *ctx,
                                       const ib_rule_t *rule,
                                       ib_mpool_t *pool,
                                       const char *parameters,
                                       ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t lua_operator_execute(ib_engine_t *ib,
                                        ib_tx_t *tx,
                                        const ib_rule_t *rule,
                                        void *data,
                                        ib_flags_t flags,
                                        ib_field_t *field,
                                        ib_num_t *result)
{
    IB_FTRACE_INIT();
    ib_status_t ib_rc;
    const char *func_name = (char *) data;

    ib_log_debug3_tx(tx, "Calling lua function %s.", func_name);

    ib_rc = ib_lua_func_eval_r(ib, tx, func_name, result);

    ib_log_debug3_tx(tx, "Lua function %s=%"PRIu64".", func_name, *result);

    IB_FTRACE_RET_STATUS(ib_rc);
}

static ib_status_t lua_operator_destroy(ib_operator_inst_t *op_inst)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_OK);
}
#endif

/**
 * @brief Parse a RuleExt directive.
 * @details Register lua function. RuleExt lua:/path/to/rule.lua phase:REQUEST
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
    IB_FTRACE_INIT();

    ib_status_t rc;
    const ib_list_node_t *targets;
    const ib_list_node_t *mod;
    ib_rule_t *rule;
    const char *file_name;

#ifdef ENABLE_LUA
    ib_operator_inst_t *op_inst;

    /* Check if lua is available. */
    if (g_ironbee_rules_lua == NULL) {
        ib_cfg_log_error(cp, "Lua is not available");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
#endif

    /* Get the targets string */
    targets = ib_list_first_const(vars);

    file_name = (const char *)ib_list_node_data_const(targets);

    if ( file_name == NULL ) {
        ib_cfg_log_error(cp, "No targets for rule");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_cfg_log_debug3(cp, "Processing external rule: %s", file_name);

    /* Allocate a rule */
    rc = ib_rule_create(cp->ib, cp->cur_ctx,
                        cp->cur_file, cp->cur_lineno,
                        false, &rule);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to create rule: %s",
                         ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_flags_set(rule->flags, (IB_RULE_FLAG_EXTERNAL | IB_RULE_FLAG_NO_TGT));

    /* Parse all of the modifiers */
    mod = targets;
    while( (mod = ib_list_node_next_const(mod)) != NULL) {
        ib_cfg_log_debug3(cp, "Parsing modifier %s", (const char *)mod->data);
        rc = parse_modifier(cp, rule, mod->data);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                "Error parsing external rule modifier \"%s\": %s",
                (const char *)mod->data,
                ib_status_to_string(rc)
            );
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Check the rule modifiers. */
    rc = check_rule_modifiers(cp, rule);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Using the rule->meta and file_name, load and stage the ext rule. */
    if (strncasecmp(file_name, "lua:", 4) == 0) {
#ifdef ENABLE_LUA
        rc = ib_lua_load_func(cp->ib,
                              g_ironbee_rules_lua,
                              file_name+4,
                              ib_rule_id(rule));

        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Failed to load lua file %s", file_name+4);
            IB_FTRACE_RET_STATUS(rc);
        }

        ib_cfg_log_debug3(cp, "Loaded lua file %s", file_name+4);

        rc = ib_operator_register(cp->ib,
                                  file_name,
                                  IB_OP_FLAG_PHASE,
                                  &lua_operator_create,
                                  NULL,
                                  &lua_operator_destroy,
                                  NULL,
                                  &lua_operator_execute,
                                  NULL);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Failed to register lua operator: %s",
                             file_name);
            IB_FTRACE_RET_STATUS(rc);
        }

        rc = ib_operator_inst_create(cp->ib,
                                     cp->cur_ctx,
                                     rule,
                                     ib_rule_required_op_flags(rule),
                                     file_name,
                                     NULL,
                                     IB_OPINST_FLAG_NONE,
                                     &op_inst);

        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Failed to instantiate lua operator for rule %s",
                             file_name+4);
            IB_FTRACE_RET_STATUS(rc);
        }

        /* The data is then name of the function. */
        op_inst->data = (void *)ib_rule_id(rule);

        rc = ib_rule_set_operator(cp->ib, rule, op_inst);

        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Failed to associate lua operator "
                             "with rule %s: %s",
                             ib_rule_id(rule), file_name+4);
            IB_FTRACE_RET_STATUS(rc);
        }

        ib_cfg_log_debug3(cp, "Set operator %s for rule %s",
                          file_name,
                          ib_rule_id(rule));
#else
        ib_cfg_log_error(cp, "IronBee built without Lua support.");
#endif
    }
    else {
        ib_cfg_log_error(cp, "RuleExt does not support rule type %s.",
                         file_name);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Finally, register the rule */
    rc = ib_rule_register(cp->ib, cp->cur_ctx, rule);
    if (rc == IB_EEXIST) {
        ib_cfg_log_warning(cp, "Not overwriting existing rule");
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error registering rule: %s",
                         ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Disable the entire chain if this rule is invalid */
    if ( (rule->flags & IB_RULE_FLAG_VALID) == 0) {
        rc = ib_rule_chain_invalidate(cp->ib, cp->cur_ctx, rule);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Error invalidating rule chain: %s",
                             ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    ib_cfg_log_debug(cp,
                     "Registered external rule %s for phase %d context %p",
                     ib_rule_id(rule), rule->meta.phase, cp->cur_ctx);

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @brief Parse a Rule directive.
 * @details Register a Rule directive to the engine.
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
    IB_FTRACE_INIT();
    ib_status_t rc;
    const ib_list_node_t *node;
    const char *nodestr;
    const char *operator;
    const char *operand;
    ib_rule_t *rule = NULL;
    int targets = 0;

    if (cbdata != NULL) {
        IB_FTRACE_MSG("Callback data is not null.");
    }

    /* Allocate a rule */
    rc = ib_rule_create(cp->ib, cp->cur_ctx,
                        cp->cur_file, cp->cur_lineno,
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
    rc = parse_operator(cp, rule, operator, operand);
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
        IB_FTRACE_RET_STATUS(rc);
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
            IB_FTRACE_RET_STATUS(rc);
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
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @brief Parse a StreamInspect directive.
 * @details Register the StreamInspect directive to the engine.
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
    IB_FTRACE_INIT();
    ib_status_t rc;
    const ib_list_node_t *node;
    ib_rule_phase_num_t phase = PHASE_INVALID;
    const char *str;
    const char *operator;
    const char *operand;
    ib_rule_t *rule;

    if (cbdata != NULL) {
        IB_FTRACE_MSG("Callback data is not null.");
    }

    /* Get the targets string */
    node = ib_list_first_const(vars);
    if ( (node == NULL) || (node->data == NULL) ) {
        ib_cfg_log_error(cp, "No stream for StreamInspect");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    str = node->data;

    /* Lookup the phase name */
    rc = lookup_phase(str, true, &phase);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Invalid phase: %s", str);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Get the operator string */
    node = ib_list_node_next_const(node);
    if ( (node == NULL) || (node->data == NULL) ) {
        ib_cfg_log_error(cp, "No operator for rule");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    operator = (const char *)node->data;

    /* Allocate a rule */
    rc = ib_rule_create(cp->ib, cp->cur_ctx,
                        cp->cur_file, cp->cur_lineno,
                        true, &rule);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to create rule: %s",
                         ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_flags_set(rule->flags, IB_RULE_FLAG_NO_TGT);

    /* Set the rule's stream */
    rc = ib_rule_set_phase(cp->ib, rule, phase);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error setting rule phase: %s",
                         ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Verify that we have an operand */
    node = ib_list_node_next_const(node);
    if ( (node == NULL) || (node-> data == NULL) ) {
        ib_cfg_log_error(cp, "No rule operand found");
        IB_FTRACE_RET_STATUS(rc);
    }
    operand = (const char *)node->data;

    /* Parse the operator */
    rc = parse_operator(cp, rule, operator, operand);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error parsing rule operator \"%s\": %s",
                         operator, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Parse all of the modifiers */
    while( (node = ib_list_node_next_const(node)) != NULL) {
        rc = parse_modifier(cp, rule, node->data);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp,
                             "Error parsing stream rule modifier \"%s\": %s",
                             (const char *)node->data,
                             ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Finally, register the rule */
    rc = ib_rule_register(cp->ib, cp->cur_ctx, rule);
    if (rc == IB_EEXIST) {
        ib_cfg_log_warning(cp, "Not overwriting existing rule");
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error registering rule: %s",
                         ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @brief Parse a RuleEnable directive.
 * @details Handle the RuleEnable directive to the engine.
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
    IB_FTRACE_INIT();
    const ib_list_node_t *node;
    ib_status_t rc = IB_OK;

    if (cbdata != NULL) {
        IB_FTRACE_MSG("Callback data is not null.");
    }

    /* Loop through all of the parameters in the list */
    IB_LIST_LOOP_CONST(vars, node) {
        const char *param = (const char *)node->data;

        if (strcasecmp(param, "all") == 0) {
            rc = ib_rule_enable_all(cp->ib, cp->cur_ctx,
                                    cp->cur_file, cp->cur_lineno);
        }
        else if (strncasecmp(param, "id:", 3) == 0) {
            const char *id = param + 3;
            rc = ib_rule_enable_id(cp->ib, cp->cur_ctx,
                                   cp->cur_file, cp->cur_lineno,
                                   id);
        }
        else if (strncasecmp(param, "tag:", 4) == 0) {
            const char *tag = param + 4;
            rc = ib_rule_enable_tag(cp->ib, cp->cur_ctx,
                                    cp->cur_file, cp->cur_lineno,
                                    tag);
        }
        else {
            ib_cfg_log_error(cp, "Invalid %s parameter \"%s\"", name, param);
            rc = IB_EINVAL;
            continue;
        }
    }

    /* Done */
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @brief Parse a RuleDisable directive.
 * @details Handle the RuleDisable directive to the engine.
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
    IB_FTRACE_INIT();
    const ib_list_node_t *node;
    ib_status_t rc = IB_OK;

    if (cbdata != NULL) {
        IB_FTRACE_MSG("Callback data is not null.");
    }

    /* Loop through all of the parameters in the list */
    IB_LIST_LOOP_CONST(vars, node) {
        const char *param = (const char *)node->data;

        if (strcasecmp(param, "all") == 0) {
            rc = ib_rule_disable_all(cp->ib, cp->cur_ctx,
                                     cp->cur_file, cp->cur_lineno);
        }
        else if (strncasecmp(param, "id:", 3) == 0) {
            const char *id = param + 3;
            rc = ib_rule_disable_id(cp->ib, cp->cur_ctx,
                                    cp->cur_file, cp->cur_lineno,
                                    id);
        }
        else if (strncasecmp(param, "tag:", 4) == 0) {
            const char *tag = param + 4;
            rc = ib_rule_disable_tag(cp->ib, cp->cur_ctx,
                                     cp->cur_file, cp->cur_lineno,
                                     tag);
        }
        else {
            ib_cfg_log_error(cp, "Invalid %s parameter \"%s\"", name, param);
            rc = IB_EINVAL;
            continue;
        }
    }

    /* Done */
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @brief Parse a RuleMarker directive.
 * @details Register a RuleMarker directive to the engine.
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
    IB_FTRACE_INIT();
    ib_status_t rc;
    const ib_list_node_t *node;
    ib_rule_t *rule = NULL;

    if (cbdata != NULL) {
        IB_FTRACE_MSG("Callback data is not null.");
    }

    /* Allocate a rule */
    rc = ib_rule_create(cp->ib, cp->cur_ctx,
                        cp->cur_file, cp->cur_lineno,
                        false, &rule);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to allocate rule: %s",
                         ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_flags_set(rule->flags, IB_RULE_FLAG_ACTION);

    /* Force the operator to one that will not execute (negated nop). */
    rc = parse_operator(cp, rule, "!@nop", NULL);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error parsing rule operator \"nop\": %s",
                         ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
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
                IB_FTRACE_RET_STATUS(rc);
            }
        }
        else {
            ib_cfg_log_error(cp, "Invalid %s parameter \"%s\"", name, param);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    /* Force a zero revision so it can always be overridden. */
    rc = parse_modifier(cp, rule, "rev:0");
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error parsing %s modifier \"rev:0\": %s",
                         name,
                         ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Check the rule modifiers. */
    rc = check_rule_modifiers(cp, rule);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Finally, register the rule. */
    rc = ib_rule_register(cp->ib, cp->cur_ctx, rule);
    if (rc == IB_EEXIST) {
        ib_cfg_log_notice(cp, "Not overwriting existing rule");
        rc = IB_OK;
    }
    else if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error registering rule marker: %s",
                         ib_status_to_string(rc));
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @brief Parse an Action directive.
 * @details Register an Action directive to the engine.
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
    IB_FTRACE_INIT();
    ib_status_t rc;
    const ib_list_node_t *node;
    ib_rule_t *rule = NULL;

    if (cbdata != NULL) {
        IB_FTRACE_MSG("Callback data is not null.");
    }

    /* Allocate a rule */
    rc = ib_rule_create(cp->ib, cp->cur_ctx,
                        cp->cur_file, cp->cur_lineno,
                        false, &rule);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to allocate rule: %s",
                         ib_status_to_string(rc));
        goto cleanup;
    }
    ib_flags_set(rule->flags, IB_RULE_FLAG_ACTION);

    /* Parse the operator */
    rc = parse_operator(cp, rule, "@nop", NULL);
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
        IB_FTRACE_RET_STATUS(rc);
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
            IB_FTRACE_RET_STATUS(rc);
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
    IB_FTRACE_RET_STATUS(rc);
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

    /* signal the end of the list */
    IB_DIRMAP_INIT_LAST
};

#ifdef ENABLE_LUA
static void clean_up_ipc_mem(void)
{
    ib_lock_destroy(&g_lua_lock);
}
#endif

static ib_status_t rules_init(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    IB_FTRACE_INIT();
    assert(ib != NULL);
    assert(m != NULL);

#ifdef ENABLE_LUA
    /* Error code from Iron Bee calls. */
    ib_status_t ib_rc;
    ib_core_cfg_t *corecfg = NULL;

    /**
     * This is the search pattern that is appended to each element of
     * lua_search_paths and then added to the Lua runtime package.path
     * global variable. */
    const char *lua_file_pattern = "?.lua";

    /* Null terminated list of search paths. */
    const char *lua_search_paths[3];

    const char *lua_preloads[][2] = { { "ffi", "ffi" },
                                      { "ironbee", "ironbee-ffi" },
                                      { "ibapi", "ironbee-api" },
                                      { NULL, NULL } };

    char *path = NULL;           /**< Tmp string to build a search path. */

    int i = 0; /**< An iterator. */

    ib_rc = ib_lock_init(&g_lua_lock);

    if (ib_rc != IB_OK) {
        ib_log_error(ib, "Failed to initialize lua global lock.");
    }

    atexit(&clean_up_ipc_mem);

    if (m == NULL) {
        IB_FTRACE_MSG("Module is null.");
        clean_up_ipc_mem();
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    g_ironbee_rules_lua = luaL_newstate();

    if (g_ironbee_rules_lua == NULL) {
        ib_log_notice(ib, "Failed to create LuaJIT state.");
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    luaL_openlibs(g_ironbee_rules_lua);

    ib_rc = ib_context_module_config(ib_context_main(ib),
                                     ib_core_module(),
                                     (void *)&corecfg);

    if (ib_rc != IB_OK) {
        ib_log_error(ib, "Could not retrieve core module configuration.");
        IB_FTRACE_RET_STATUS(ib_rc);
    }

    /* Initialize the search paths list. */
    lua_search_paths[0] = corecfg->module_base_path;
    lua_search_paths[1] = corecfg->rule_base_path;
    lua_search_paths[2] = NULL;

    for (i = 0; lua_search_paths[i] != NULL; ++i)
    {
        char *tmp;
        ib_log_debug(ib,
            "Adding \"%s\" to lua search path.", lua_search_paths[i]);

        /* Strlen + 2. One for \0 and 1 for the path separator. */
        tmp = realloc(path,
                      strlen(lua_search_paths[i]) +
                      strlen(lua_file_pattern) + 2);

        if (tmp == NULL) {
            ib_log_error(ib, "Could allocate buffer for string append.");
            free(path);
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
        path = tmp;

        strcpy(path, lua_search_paths[i]);
        strcpy(path + strlen(path), "/");
        strcpy(path + strlen(path), lua_file_pattern);

        ib_lua_add_require_path(ib, g_ironbee_rules_lua, path);

        ib_log_debug(ib, "Added \"%s\" to lua search path.", path);
    }

    /* We are done with path. To be safe, we NULL it as there is more work
     * to be done in this function, and we do not want to touch path again. */
    free(path);
    path = NULL;

    for (i = 0; lua_preloads[i][0] != NULL; ++i)
    {
        ib_rc = ib_lua_require(ib,
                               g_ironbee_rules_lua,
                               lua_preloads[i][0],
                               lua_preloads[i][1]);
        if (ib_rc != IB_OK)
        {
            ib_log_error(ib,
                "Failed to load mode \"%s\" into \"%s\".",
                lua_preloads[i][1],
                lua_preloads[i][0]);
            clean_up_ipc_mem();
            IB_FTRACE_RET_STATUS(ib_rc);
        }
    }

#endif

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t rules_fini(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    IB_FTRACE_INIT();

#ifdef ENABLE_LUA
    ib_lock_destroy(&g_lua_lock);

    if (g_ironbee_rules_lua != NULL) {
        lua_close(g_ironbee_rules_lua);
        g_ironbee_rules_lua = NULL;
    }
#endif

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Initialize the module structure. */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /* Default metadata */
    MODULE_NAME_STR,                     /* Module name */
    IB_MODULE_CONFIG_NULL,               /* Global config data */
    NULL,                                /* Configuration field map */
    rules_directive_map,                 /* Config directive map */
    rules_init,                          /* Initialize function */
    NULL,                                /* Callback data */
    rules_fini,                          /* Finish function */
    NULL,                                /* Callback data */
    NULL,                                /* Context open function */
    NULL,                                /* Callback data */
    NULL,                                /* Context close function */
    NULL,                                /* Callback data */
    NULL,                                /* Context destroy function */
    NULL                                 /* Callback data */
);
