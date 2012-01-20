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
 * @brief IronBee - Rule engine definitions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/release.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeRuleEngine Rule Engine
 * @ingroup IronBee
 * @{
 */

/**
 * Rule phase number.
 */
typedef enum {
    PHASE_INVALID = -1,             /**< Invalid; used to terminate list */
    PHASE_NONE,                     /**< No phase */
    PHASE_REQUEST_HEADER,           /**< Request header available. */
    PHASE_REQUEST_BODY,             /**< Request body available. */
    PHASE_RESPONSE_HEADER,          /**< Response header available. */
    PHASE_RESPONSE_BODY,            /**< Response body available. */
    PHASE_POSTPROCESS,              /**< Post-processing phase. */
    PHASE_MAX = PHASE_POSTPROCESS,  /**< Max phase number. */
    IB_RULE_PHASE_COUNT,
} ib_rule_phase_type_t;

/**
 * Rule engine: Rule
 */
typedef struct ib_rule_t ib_rule_t;

/**
 * TODO: in Craig's code
 */
typedef struct ib_action_t ib_action_t;
typedef struct ib_action_inst_t ib_action_inst_t;

#ifdef __cplusplus
}
#endif

#endif /* _IB_RULE_DEFS_H_ */
