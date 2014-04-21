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

#ifndef _IB_DECODE_H_
#define _IB_DECODE_H_

/**
 * @file
 * @brief IronBee --- Utility Functions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup IronBeeUtilString
 *
 * @{
 */

/**
 * Decode a URL.
 *
 * @param[in] data_in URL data.
 * @param[in] dlen_in Length of @a data_in.
 * @param[in] data_out Where to write output.
 * @param[out] dlen_out Bytes written to @a data_out.
 *
 * @returns IB_OK
 */
ib_status_t DLL_PUBLIC ib_util_decode_url(
    const uint8_t  *data_in,
    size_t          dlen_in,
    uint8_t        *data_out,
    size_t         *dlen_out
)
NONNULL_ATTRIBUTE(1, 3, 4);

/**
 * Decode HTML entity.
 *
 * @param[in] data_in HTML entity data.
 * @param[in] dlen_in Length of @a data_in.
 * @param[in] data_out Where to write output.
 * @param[out] dlen_out Bytes written to @a data_out.
 *
 * @returns Status code
 * - IB_OK: Success
 * - IB_EALLOC: Allocation error
 */
ib_status_t DLL_PUBLIC ib_util_decode_html_entity(
    const uint8_t  *data_in,
    size_t          dlen_in,
    uint8_t        *data_out,
    size_t         *dlen_out
)
NONNULL_ATTRIBUTE(1, 3, 4);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_DECODE_H_ */
