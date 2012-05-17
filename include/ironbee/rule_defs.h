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

#ifndef _IB_RULE_DEFS_H_
#define _IB_RULE_DEFS_H_

/**
 * @file
 * @brief IronBee &mdash; Rule engine definitions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup IronBeeRule
 * @{
 */

/**
 * Rule phase number.
 */
typedef enum {
    PHASE_INVALID = -1,                 /**< Invalid; used to terminate list */
    PHASE_NONE,                         /**< No phase */
    PHASE_REQUEST_HEADER,               /**< Request header available. */
    PHASE_REQUEST_BODY,                 /**< Request body available. */
    PHASE_RESPONSE_HEADER,              /**< Response header available. */
    PHASE_RESPONSE_BODY,                /**< Response body available. */
    PHASE_POSTPROCESS,                  /**< Post-processing phase. */
    PHASE_STR_REQUEST_HEADER,           /**< Stream: Req. header available. */
    PHASE_STR_REQUEST_BODY,             /**< Stream: Req. body available. */
    PHASE_STR_RESPONSE_HEADER,          /**< Stream: Resp. header available. */
    PHASE_STR_RESPONSE_BODY,            /**< Stream: Resp. body available. */
    IB_RULE_PHASE_COUNT,                /**< Size of rule phase lists */
} ib_rule_phase_t;

/**
 * Rule flags
 *
 * If the external flag is set, the rule engine will always execute the
 * operator, passing NULL in as the field pointer.  The external rule is
 * expected to extract whatever fields, etc. it requires itself.
 */
#define IB_RULE_FLAG_NONE        (0x0)     /**< No flags */
#define IB_RULE_FLAG_ENABLED     (1 << 0)  /**< Rule is enabled */
#define IB_RULE_FLAG_VALID       (1 << 1)  /**< Rule is valid */
#define IB_RULE_FLAG_EXTERNAL    (1 << 2)  /**< External rule */
#define IB_RULE_FLAG_CHAIN       (1 << 3)  /**< Rule is parent in a chain */
#define IB_RULE_FLAG_IN_CHAIN    (1 << 4)  /**< Rule is part of a chain */

/**
 * Rule meta-flags:
 * RUNABLE: Only run the rule if it's enabled and valid
 * PHASE: Flags set for all phase rules
 * STREAM: Flags set for all stream rules
 * CHAIN: Rule is any part of a chain
 */
#define IB_RULE_FLAGS_RUNABLE (IB_RULE_FLAG_ENABLED | IB_RULE_FLAG_VALID)
#define IB_RULE_FLAGS_PHASE   \
    (IB_RULE_FLAG_ALLOW_TFNS | IB_RULE_FLAG_ALLOW_CHAIN)
#define IB_RULE_FLAGS_STREAM  (IB_RULE_FLAG_NONE)
#define IB_RULE_FLAGS_CHAIN   (IB_RULE_FLAG_CHAIN | IB_RULE_FLAG_IN_CHAIN)

/**
 * Rule meta-data flags
 */
#define IB_RULEMD_FLAG_NONE        (0x0)     /**< No flags */
#define IB_RULEMD_FLAG_EXPAND_MSG  (1 << 0)  /**< Expand rule message */
#define IB_RULEMD_FLAG_EXPAND_DATA (1 << 1)  /**< Expand rule logdata */

/**
 * Rule engine: Basic rule type
 */
typedef struct ib_rule_t ib_rule_t;
typedef struct ib_rule_target_t ib_rule_target_t;


/**
 * @} IronBeeRuleDefs
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_RULE_DEFS_H_ */
