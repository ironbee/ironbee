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
 * @author Nick LeRoy <nleroy@qualys.com>
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
const char *ib_capture_name(int num);

/**
 * Get the full name of a capture item (i.e. "CAPTURE:0")
 *
 * @param[in] tx Transaction (used for allocations if collection_name
 *            is not NULL)
 * @param[in] collection_name The name of the capture collection or NULL
 * @param[in] num Capture item number
 *
 * @returns Full name string
 */
const char *ib_capture_fullname(
    const ib_tx_t *tx,
    const char    *collection_name,
    int            num
);

/**
 * Fetch appropriate data field given a name, creating if necessary.
 *
 * @param[in] tx Transaction
 * @param[in] collection_name The name of the capture collection or NULL
 *                            for default name.
 * @param[out] field Where to write result to.
 * @returns
 * - IB_OK: All ok
 * - IB_EALLOC: Allocation error.
 * - IB_EINVAL: Unexpected error replacing existing non-list field.
 * - Other on unexpected error.
 **/
ib_status_t DLL_PUBLIC ib_capture_acquire(
    const ib_tx_t  *tx,
    const char     *collection_name,
    ib_field_t    **field
);

/**
 * Clear data capture fields
 *
 * @param[in] capture Capture collection to clear.
 *
 * @returns
 * - IB_OK: All OK
 * - Error status from: ib_capture_init_item()
 */
ib_status_t DLL_PUBLIC ib_capture_clear(ib_field_t *capture);

/**
 * Set a single capture field item
 *
 * @param[in] capture Capture collection to clear.
 * @param[in] num Number of the capture field
 * @param[in] mm Memory manager to use.
 * @param[in] in_field Field to add.
 *
 * @returns
 * - IB_OK: All OK
 * - IB_EINVAL: @a num is too large
 * - Error status from: ib_capture_set_list()
 *                      ib_capture_init_item()
 *                      ib_var_source_set()
 *                      ib_field_mutable_value()
 */
ib_status_t DLL_PUBLIC ib_capture_set_item(
    ib_field_t       *capture,
    int               num,
    ib_mm_t           mm,
    const ib_field_t *in_field
);

/**
 * Add a single capture field item.
 *
 * This will add an, possibly additional, item.
 *
 * @param[in] capture Capture collection to clear.
 * @param[in] in_field Field to add.
 *
 * @returns
 * - IB_OK: All OK
 * - Error status from: ib_capture_set_list()
 *                      ib_capture_init_item()
 *                      ib_var_source_set()
 *                      ib_field_mutable_value()
 */
ib_status_t DLL_PUBLIC ib_capture_add_item(
    ib_field_t *capture,
    ib_field_t *in_field
);

#ifdef __cplusplus
}
#endif

#endif
