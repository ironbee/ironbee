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

#ifndef _IB_RULE_LOGGER_H_
#define _IB_RULE_LOGGER_H_

/**
 * @file
 * @brief IronBee --- Rule logger definitions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/config.h>
#include <ironbee/rule_defs.h>
#include <ironbee/rule_engine.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
* @defgroup IronBeeRuleLogger Rule Logger
* @ingroup IronBeeRule
*
* This defines rule logging functions that may be used by external
* modules.
*
* @{
*/

/**
 * Return the configured rule logging level.
 *
 * This is used to determine if optional complex processing should be
 * performed to log possibly option information.
 *
 * @param[in] ctx The context that we're looking the level up for
 *
 * @return The log level configured.
 */
ib_logger_level_t ib_rule_log_level(
    const ib_context_t *ctx
);

/**
 * Return the configured rule debug logging level.
 *
 * This is used to determine if optional complex processing should be
 * performed to log possibly option information.
 *
 * @param[in] ctx The context that we're looking the level up for
 *
 * @return The log level configured.
 */
ib_rule_dlog_level_t ib_rule_dlog_level(
    const ib_context_t *ctx
);

/**
 * Add an event to a rule execution logging object
 *
 * @param[in,out] exec_log The rule execution log object
 * @param[in] event The event to log
 *
 * @returns IB_OK on success,
 *          IB_EALLOC if an allocation failed
 *          Error status returned by ib_list_push()
 */
ib_status_t ib_rule_log_exec_add_event(
    ib_rule_log_exec_t  *exec_log,
    const ib_logevent_t *event
);

/**
 * Log an audit log file for the rule logger
 *
 * @param[in] rule_exec Rule execution logging object
 * @param[in] audit_log Full path of the audit log file
 * @param[in] failed If true, this logs that writing the audit log file
 *            @a audit_log, failed. If false this function logs
 *            that writing @a audit_log succeeded.
 */
void ib_rule_log_add_audit(
    const ib_rule_exec_t *rule_exec,
    const char           *audit_log,
    bool                  failed
);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_RULE_LOGGER_H_ */
