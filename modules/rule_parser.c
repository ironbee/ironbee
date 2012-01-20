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
 * @brief IronBee - Rule parsing logic for the rules module.
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <string.h>

#include <ironbee/types.h>
#include <ironbee/util.h>
#include <ironbee/config.h>
#include <ironbee/debug.h>
#include <ironbee/rule_engine.h>
#include <rule_parser.h>

/* Parse rule operator */
ib_status_t ib_rule_parse_operator(ib_cfgparser_t *cp,
                                   ib_rule_t *rule,
                                   const char *str)
{
    IB_FTRACE_INIT(ib_rule_parse_operator);
    ib_status_t         rc = IB_OK;
    const char         *at;
    const char         *bang;
    const char         *op;
    ib_num_t            invert = 0;
    char               *copy;
    char               *space;
    char               *args = NULL;
    ib_operator_inst_t *operator;
    

    /* Find the '@' that starts an operator */
    at = strchr(str, '@');
    if (at == NULL) {
        ib_log_error(cp->ib, 4, "No operator in rule '%s'", str);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Do we have a leading '!'? */
    bang = strchr(str, '!');
    if ( (bang != NULL) && (bang < at) ) {
        invert = 1;
    }

    /* Make a copy of the string to operate on */
    copy = ib_mpool_strdup(ib_rule_mpool(cp->ib), at);
    if (copy == NULL) {
        ib_log_error(cp->ib, 4, "Failed to copy rule operator string '%s'", at);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    op = copy;

    /* Find first space */
    space = strchr(copy, ' ');
    if (space != NULL) {
        size_t  oplen = strspn(space, " ");
        char   *end;
        size_t  alen;

        *space = '\0';
        args = space + oplen;

        /* Strip off trailing whitespace from args */
        alen = strlen(args);
        if (alen > 0) {
            end = args+alen-1;
            while( (end > args) && ( *end == ' ') ) {
                *end = '\0';
                --end;
            }
        }

        /* Is args an empty string? */
        if (*args == '\0') {
            args = NULL;
        }
    }

    /* Create the operator instance */
    rc = ib_operator_inst_create(cp->ib, op, args, &operator);
    if (rc != IB_OK) {
        ib_log_error(cp->ib, 4,
                     "Failed to create operator instance '%s': %d", op, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Set the operator */
    rc = ib_rule_set_operator(cp->ib, rule, operator, invert);
    if (rc != IB_OK) {
        ib_log_error(cp->ib, 4,
                     "Failed to set operator for rule: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_log_debug(cp->ib, 9,
                 "Rule: op='%s'; invert=%d args='%s'",
                 op, invert, args);

    IB_FTRACE_RET_STATUS(rc);
}

/* Parse rule inputs */
ib_status_t ib_rule_parse_inputs(ib_cfgparser_t *cp,
                                 ib_rule_t *rule,
                                 const char *input_str)
                                 
{
    IB_FTRACE_INIT(ib_rule_parse_inputs);
    ib_status_t  rc = IB_OK;
    size_t       len;
    const char  *start;
    const char  *cur;
    char        *copy;
    char        *save;
    
    /* Copy the input string */
    len = strspn(input_str, " ");
    start = input_str+len;
    if (*start == '\0') {
        ib_log_error(cp->ib, 4, "Rule inputs is empty");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    copy = ib_mpool_strdup(ib_rule_mpool(cp->ib), start);

    /* Split it up */
    ib_log_debug(cp->ib, 9, "Splitting rule input string '%s'", copy);
    for (cur = strtok_r(copy, "|,", &save);
         cur != NULL;
         cur = strtok_r(NULL, "|,", &save) ) {
        rc = ib_rule_add_input(cp->ib, rule, cur);
        if (rc != IB_OK) {
            ib_log_error(cp->ib, 4, "Failed to add rule input '%s'", cur);
            IB_FTRACE_RET_STATUS(rc);
        }
        ib_log_debug(cp->ib, 4, "Added rule input '%s'", cur);
    }
    
    IB_FTRACE_RET_STATUS(rc);
}

/* Parse rule modfier */
ib_status_t ib_rule_parse_modifier(ib_cfgparser_t *cp,
                                   ib_rule_t *rule,
                                   const char *modifier_str)
                                   
{
    IB_FTRACE_INIT(ib_rule_parse_modifier);
    ib_status_t rc = IB_OK;

    /* @todo */

    IB_FTRACE_RET_STATUS(rc);
}
