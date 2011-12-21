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
 * @brief IronBee - User Agent Extraction Module
 *
 * This module extracts the user agent information
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <user_agent_private.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>

#include <ironbee/types.h>
#include <ironbee/debug.h>

/* The actual rules */
static modua_match_ruleset_t modua_rules =
{
    0,
    {
        {
            "bot",
            {
                { PRODUCT, CONTAINS, "bot", YES },
            },
        },
        {
            "Firefox 8.0",
            {
                { PRODUCT, STARTSWITH, "Mozilla", YES },
                { PLATFORM, CONTAINS, "compatible", YES },
                { EXTRA, ENDSWITH, "Firefox/8.0", YES },
            },
        },
        {
            "Pure Mozilla",
            {
                { PRODUCT, STARTSWITH, "Mozilla", YES },
                { PLATFORM, CONTAINS, "compatible", NO },
                { EXTRA, EXISTS, "", NO },
            },
        },
        {
            "Mozilla",
            {
                { PRODUCT, STARTSWITH, "Mozilla", YES },
                { PLATFORM, CONTAINS, "compatible", NO },
            },
        },
        {
            "browser",
            {
                { PRODUCT, STARTSWITH, "Mozilla", YES },
            },
        },
        {
            "PDA",
            {
                { PRODUCT, STARTSWITH, "BlackBerry", YES },
            },
        },
        {
            "curl",
            {
                { PRODUCT, CONTAINS, "curl", YES },
            },
        },
        {
            "curl",
            {
                { PRODUCT, STARTSWITH, "Wget", YES },
            },
        },
        {
            NULL,
            {
                { INVALID, EXISTS, "", NO },
            },
        }
    }
};

/**
 * @internal
 * Initialize the specified rule.
 *
 * Initialize a field match rule by storing of the length of it's string.
 *
 * @param[in,out] rule Match rule to initialize
 *
 * @returns status
 */
static ib_status_t modua_field_rule_init(modua_field_rule_t *rule)
{
    IB_FTRACE_INIT(modua_category_match_init);
    if (rule->string != NULL) {
        rule->slen = strlen(rule->string);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Initialize the static rules */
ib_status_t modua_rules_init(unsigned *failed)
{
    IB_FTRACE_INIT(modua_rules_init);
    modua_match_rule_t  *rule;
    ib_status_t          rc;
    modua_field_rule_t  *field_rule;

    for (rule = modua_rules.rules; rule->category != NULL; rule++) {
        unsigned ruleno;
        for (ruleno = 0, field_rule = rule->rules;
             field_rule->string != NULL;
             ++ruleno, ++field_rule) {
            rc = modua_field_rule_init(field_rule);
            if (rc != IB_OK) {
                *failed = ruleno;
                IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
            }
            ++rule->num_rules;
        }
        ++modua_rules.num_rules;
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Get list of rules */
const modua_match_ruleset_t *modua_rules_get( void )
{
    IB_FTRACE_INIT(modua_get_rules);
    if (modua_rules.num_rules == 0) {
        IB_FTRACE_RET_PTR(modua_match_rule_t, NULL);
    }
    IB_FTRACE_RET_PTR(modua_match_ruleset_t, &modua_rules);
}
