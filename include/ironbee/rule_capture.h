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

#ifndef __IRONBEE__RULE_CAPTURE_H__
#define __IRONBEE__RULE_CAPTURE_H__

/**
 * @file
 * @brief IronBee --- Rule Capture
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/engine_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Determine of operator results should be captured
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] result Operator result value
 *
 * @returns true if the results should be captured, false otherwise
 */
bool DLL_PUBLIC ib_rule_should_capture(
    const ib_rule_exec_t       *rule_exec,
    ib_num_t                    result);

/**
 * Get the name of a capture item (i.e. "0")
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] num Capture item number
 *
 * @returns Name string
 */
const char *ib_rule_capture_name(
    const ib_rule_exec_t       *rule_exec,
    int                         num);

/**
 * Get the full name of a capture item (i.e. "CAPTURE:0")
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] num Capture item number
 *
 * @returns Full name string
 */
const char *ib_rule_capture_fullname(
    const ib_rule_exec_t       *rule_exec,
    int                         num);

/**
 * Clear data capture fields
 *
 * @param[in] rule_exec Rule execution object
 *
 * @returns IB_OK: All OK
 * @returns Error status from: ib_capture_clear()
 */
ib_status_t DLL_PUBLIC ib_rule_capture_clear(
    const ib_rule_exec_t       *rule_exec);

/**
 * Set a single capture field item
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] num Number of the capture field
 * @param[in] in_field Field to add.
 *
 * @returns IB_OK: All OK
 * @returns Error status from: ib_capture_set_item()
 */
ib_status_t ib_rule_capture_set_item(
    const ib_rule_exec_t *rule_exec,
    int                   num,
    ib_field_t           *in_field);

/**
 * Add a single capture field item.
 *
 * This will add a, possibly additional, item.
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] in_field Field to add.
 *
 * @returns IB_OK: All OK
 * @returns Error status from: ib_capture_set_list()
 *                             ib_capture_init_item()
 *                             ib_data_list_push()
 *                             ib_field_mutable_value()
 */
ib_status_t ib_rule_capture_add_item(
    const ib_rule_exec_t *rule_exec,
    ib_field_t           *in_field);

#ifdef __cplusplus
}
#endif

#endif
