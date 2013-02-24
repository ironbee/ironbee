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

#ifndef __IRONBEE__CAPTURE_H__
#define __IRONBEE__CAPTURE_H__

/**
 * @file
 * @brief IronBee --- Capture
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@calfeld.net>
 */

#include <ironbee/engine_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the name of a capture item (i.e. "0")
 *
 * @param[in] num Capture item number
 *
 * @returns Name string
 */
const char *ib_capture_fullname(int num);

/**
 * Get the full name of a capture item (i.e. "CAPTURE:0")
 *
 * @param[in] num Capture item number
 *
 * @returns Full name string
 */
const char *ib_capture_name(int num);

/**
 * Clear data capture fields
 *
 * @param[in] tx Transaction
 *
 * @returns IB_OK: All OK
 *          Error status from: ib_capture_init_item()
 */
ib_status_t DLL_PUBLIC ib_capture_clear(ib_tx_t *tx);

/**
 * Set a single capture field item
 *
 * @param[in] tx Transaction
 * @param[in] num Number of the capture field
 * @param[in] in_field Field to add.
 *
 * @returns IB_OK: All OK
 *          IB_EINVAL: @a num is too large
 *          Error status from: ib_capture_set_list()
 *                             ib_capture_init_item()
 *                             ib_data_list_push()
 *                             ib_field_mutable_value()
 */
ib_status_t ib_capture_set_item(
    ib_tx_t    *tx,
    int         num,
    ib_field_t *in_field
);

/**
 * Add a single capture field item.
 *
 * This will add a, possibly additional, item.
 *
 * @param[in] tx Transaction
 * @param[in] in_field Field to add.
 *
 * @returns IB_OK: All OK
 *          IB_EINVAL: @a num is too large
 *          Error status from: ib_capture_set_list()
 *                             ib_capture_init_item()
 *                             ib_data_list_push()
 *                             ib_field_mutable_value()
 */
ib_status_t ib_capture_add_item(
    ib_tx_t    *tx,
    ib_field_t *in_field
);

#endif
