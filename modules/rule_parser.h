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

#ifndef _IB_RULE_PARSER_H_
#define _IB_RULE_PARSER_H_

/**
 * @file
 * @brief IronBee - Rule parser private defintions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/release.h>

#include <ironbee/types.h>
#include <ironbee/operator.h>
#include <ironbee/mpool.h>
#include <ironbee/rule_engine.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilRuleParser RuleParser
 * @ingroup IronBeeUtil
 *
 * Rule parser routines.
 *
 * Routines to perform parsing of rule strings.
 *
 * @{
 */

/**
 * Parse rule's operator.
 *
 * Parsers the rule's operator string, stores the results in the rule object.
 *
 * @param cp IronBee configuaration parser
 * @param rule Rule object to update
 * @param str Operator string
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_parse_operator(ib_cfgparser_t *cp,
                                              ib_rule_t *rule,
                                              const char *str);

/**
 * Parse a rule's input string.
 *
 * Parsers the rule's input field list string, stores the results in the rule
 * object.
 *
 * @param cp IronBee configuration parser
 * @param rule Rule to operate on
 * @param input_str Input field name.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_parse_inputs(ib_cfgparser_t *cp,
                                            ib_rule_t *rule,
                                            const char *input_str);

/**
 * Parse a rule's modifier string.
 *
 * Parsers the rule's modifier string, stores the results in the rule
 * object.
 *
 * @param cp IronBee configuration parser
 * @param rule Rule to operate on
 * @param modifier_str Input field name.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_rule_parse_modifier(ib_cfgparser_t *cp,
                                              ib_rule_t *rule,
                                              const char *modifier_str);


#ifdef __cplusplus
}
#endif

#endif /* _IB_RULE_ENGINE_H_ */
