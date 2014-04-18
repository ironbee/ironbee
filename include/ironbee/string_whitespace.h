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

#ifndef _IB_STRING_WHITESPACE_H_
#define _IB_STRING_WHITESPACE_H_

/**
 * @file
 * @brief IronBee --- String Trim Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/mm.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup IronBeeUtilString
 *
 * @{
 */

/**
 * Delete all whitespace from a string.
 *
 * @param[in] mm Memory manager.
 * @param[in] data_in Pointer to input data.
 * @param[in] dlen_in Length of @a data_in.
 * @param[out] data_out Pointer to output data.
 * @param[in] dlen_out Length of @a data_out.
 *
 * @result Status code
 */
ib_status_t DLL_PUBLIC ib_str_whitespace_remove(
    ib_mm_t         mm,
    const uint8_t  *data_in,
    size_t          dlen_in,
    uint8_t       **data_out,
    size_t         *dlen_out
)
NONNULL_ATTRIBUTE(2, 4, 5);

/**
 * Compress whitespace in a string.
 *
 * @param[in] mm Memory manager
 * @param[in] data_in Pointer to input data
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Pointer to output data
 * @param[in] dlen_out Length of @a data_out
 *
 * @result Status code
 */
ib_status_t DLL_PUBLIC ib_str_whitespace_compress(
    ib_mm_t         mm,
    const uint8_t  *data_in,
    size_t          dlen_in,
    uint8_t       **data_out,
    size_t         *dlen_out
)
NONNULL_ATTRIBUTE(2, 4, 5);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_STRING_WHITESPACE_H_ */
