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

#ifndef _IB_MODULE_USER_AGENT_PRIVATE_H_
#define _IB_MODULE_USER_AGENT_PRIVATE_H_

/**
 * @file
 * @brief IronBee - Private user agent module definitions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/types.h>


/* Category rules
 * If all of the expressions in the 'match rule' match, set the
 * transactions's user agent category to the rule's category.
 */
#define MODUA_MAX_MATCH_RULES   32  /* Max # of match rules */
#define MODUA_MAX_FIELD_RULES    8  /* Max # of field rules / match rule */

/* Match against what field? */
typedef enum {
    NONE  = -1,            /**< Invalid match, used to terminate rule list */
    PRODUCT  = 0,          /**< Match against product field */
    PLATFORM = 1,          /**< Match against platform field */
    EXTRA    = 2,          /**< Match against extra field */
} modua_matchfield_t;

/* Type of match */
typedef enum {
    TERMINATE = -1,        /**< Invalid field */
    EXISTS,                /**< Field exists in user agent */
    MATCHES,               /**< Field exactly matches string */
    STARTSWITH,            /**< Field starts with string */
    CONTAINS,              /**< Field contains string */
    ENDSWITH,              /**< Field ends with string */
} modua_matchtype_t;

/* Expected result */
typedef enum {
    NO = 0,                /**< Expect a negative result */
    YES = 1                /**< Expect a positive result */
} modua_matchresult_t;

/* Match a string to a field */
typedef struct modua_field_rule_s {
    modua_matchfield_t  match_field;  /**< Field to match agaist */
    modua_matchtype_t   match_type;   /**< Type of the match */
    const char         *string;       /**< String to match field with */
    modua_matchresult_t match_result; /**< Match result */
    size_t              slen;         /**< Length of the pattern string */
} modua_field_rule_t;

/* Match category rule list */
typedef struct modua_match_rule_s {
    const char        *category;      /**< Category string */
    modua_field_rule_t rules[MODUA_MAX_FIELD_RULES]; /**< Field match rules*/
    unsigned           num_rules;     /**< Number of actual rules */
} modua_match_rule_t;

/* Match category rule set */
typedef struct modua_match_ruleset_s {
    unsigned           num_rules;     /**< Actual number of match rules */
    modua_match_rule_t rules[MODUA_MAX_MATCH_RULES]; /**< The match rules */
} modua_match_ruleset_t;

/**
 * @internal
 * Initialize the user agent category rules.
 *
 * Initializes the rules used to categorize user agent strings.
 *
 * @param[out] failed Number of rule that failed to initialize
 *
 * @returns status
 */
ib_status_t modua_rules_init(unsigned *failed);

/**
 * @internal
 * Get the match rules.
 *
 * Returns the match rules.  Rules must be previously initialized via
 * modua_rules_init( ).
 *
 * @param[out] count Total number of rules
 *
 * @returns Pointer to the rule array
 */
const modua_match_ruleset_t *modua_rules_get(void);

#endif /* _IB_MODULE_USER_AGENT_PRIVATE_H_ */
