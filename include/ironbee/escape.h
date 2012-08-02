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

#ifndef _IB_ESCAPE_H_
#define _IB_ESCAPE_H_

/**
 * @file
 * @brief IronBee &mdash; String Utility Functions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/string.h>
#include <ironbee/types.h>

#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilStringEscape String Escaping
 * @ingroup IronBeeUtilString
 *
 * Functions to escape strings.
 *
 * @{
 */

/**
 * Convert a bytestring to a json string with escaping
 *
 * @param[in] mp Memory pool to use for allocations
 * @param[in] data_in Input data
 * @param[in] dlen_in Length of data in @a data_in
 * @param[in] nul Save room for and append a nul byte?
 * @param[out] data_out Output data
 * @param[out] dlen_out Length of data in @a data_out (or NULL)
 * @param[out] result Result flags (IB_STRFLAG_xx)
 *
 * @returns IB_OK if successful
 *          IB_EALLOC if allocation errors occur
 */
ib_status_t ib_string_escape_json_ex(ib_mpool_t *mp,
                                     const uint8_t *data_in,
                                     size_t dlen_in,
                                     bool nul,
                                     char **data_out,
                                     size_t *dlen_out,
                                     ib_flags_t *result);

/**
 * Convert a c-string to a json string with escaping
 *
 * @param[in] mp Memory pool to use for allocations
 * @param[in] data_in Input data
 * @param[out] data_out Output data
 * @param[out] result Result flags (IB_STRFLAG_xx)
 *
 * @returns IB_OK if successful
 *          IB_EALLOC if allocation errors occur
 */
ib_status_t ib_string_escape_json(ib_mpool_t *mp,
                                  const char *data_in,
                                  char **data_out,
                                  ib_flags_t *result);

/**
 * @} IronBeeUtilString
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ESCAPE_H_ */
