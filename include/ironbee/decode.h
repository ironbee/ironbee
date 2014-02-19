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
 */

#include <ironbee/build.h>
#include <ironbee/mm.h>
#include <ironbee/types.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilDecode Utility Decode Functions
 * @ingroup IronBeeUtil
 *
 * Code to decode URLs and HTML
 *
 * @{
 */

/**
 * In-place decode a URL (NUL-string version)
 *
 * @param[in,out] data Buffer to operate on
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status code:
 * - IB_OK: Success
 *
 * @internal
 * Implemented in: util/decode.c
 * Tested in: tests/test_util_decode.cpp
 */
ib_status_t DLL_PUBLIC ib_util_decode_url(
    char         *data,
    ib_flags_t   *result);

/**
 * In-place decode a URL (ex version)
 *
 * @param[in,out] data_in Buffer to operate on
 * @param[in] dlen_in Length of @a data_in
 * @param[out] dlen_out Output length
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status code:
 * - IB_OK: Success
 *
 * @internal
 * Implemented in: util/decode.c
 * Tested in: tests/test_util_decode.cpp
 */
ib_status_t DLL_PUBLIC ib_util_decode_url_ex(
    uint8_t      *data_in,
    size_t        dlen_in,
    size_t       *dlen_out,
    ib_flags_t   *result);

/**
 * Copy-on-write decode a URL (NUL-string version)
 *
 * @param[in] mm Memory Manager for allocations
 * @param[in] data_in Buffer to operate on
 * @param[out] data_out Output data
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status code:
 * - IB_OK: Success
 * - IB_EALLOC: allocation error
 *
 * @internal
 * Implemented in: util/decode.c
 * Tested in: tests/test_util_decode.cpp
 */
ib_status_t DLL_PUBLIC ib_util_decode_url_cow(
    ib_mm_t       mm,
    const char   *data_in,
    char        **data_out,
    ib_flags_t   *result);

/**
 * Copy-on-write decode a URL (ex version)
 *
 * @param[in] mm Memory Manager for allocations
 * @param[in] data_in Buffer to operate on
 * @param[in] dlen_in Length of @a data_in
 * @param[in] nul_byte Reserve extra byte for NUL character?
 * @param[out] data_out Output data
 * @param[out] dlen_out Length of @a data_out
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status code:
 * - IB_OK: Success
 * - IB_EALLOC: allocation error
 *
 * @internal
 * Implemented in: util/decode.c
 * Tested in: tests/test_util_decode.cpp
 */
ib_status_t DLL_PUBLIC ib_util_decode_url_cow_ex(
    ib_mm_t         mm,
    const uint8_t  *data_in,
    size_t          dlen_in,
    bool            nul_byte,
    uint8_t       **data_out,
    size_t         *dlen_out,
    ib_flags_t     *result);

/**
 * In-place decode HTML entities (NUL-string version)
 *
 * @param[in,out] data Buffer to operate on
 * @param[out] result Result flags
 *
 * @returns Status code:
 * - IB_OK: Success
 *
 * @internal
 * Implemented in: util/decode.c
 * Tested in: tests/test_util_decode.cpp
 */
ib_status_t DLL_PUBLIC ib_util_decode_html_entity(
    char           *data,
    ib_flags_t     *result);

/**
 * In-place decode HTML entities (ex version)
 *
 * @param[in,out] data Buffer to operate on
 * @param[in] dlen_in Length of @a data
 * @param[out] dlen_out Output length
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status (IB_OK)
 *
 * @internal
 * Implemented in: util/decode.c
 * Tested in: tests/test_util_decode.cpp
 */
ib_status_t DLL_PUBLIC ib_util_decode_html_entity_ex(
    uint8_t        *data,
    size_t          dlen_in,
    size_t         *dlen_out,
    ib_flags_t     *result);

/**
 * Copy-on-write decode HTML entity (NUL-string version)
 *
 * @param[in] mm Memory Manager for allocations
 * @param[in] data_in Buffer to operate on
 * @param[out] data_out Output data
 * @param[out] result Result flags (IB_STRFLAG_xxx)
 *
 * @returns Status: IB_OK
 *                  IB_EALLOC for allocation errors
 *
 * @internal
 * Implemented in: util/decode.c
 * Tested in: tests/test_util_decode.cpp
 */
ib_status_t DLL_PUBLIC ib_util_decode_html_entity_cow(
    ib_mm_t         mm,
    const char     *data_in,
    char          **data_out,
    ib_flags_t     *result);

/**
 * Copy-on-write decode HTML entity (ex version)
 *
 * @param[in] mm Memory Manager for allocations
 * @param[in] data_in Buffer to operate on
 * @param[in] dlen_in Length of @a buf
 * @param[out] data_out Output data
 * @param[out] dlen_out Length of @a data_out
 * @param[out] result Result flags
 *
 * @returns Status code
 * - IB_OK: Success
 * - IB_EALLOC: Allocation error
 *
 * @internal
 * Implemented in: util/decode.c
 * Tested in: tests/test_util_decode.cpp
 */
ib_status_t DLL_PUBLIC ib_util_decode_html_entity_cow_ex(
    ib_mm_t         mm,
    const uint8_t  *data_in,
    size_t          dlen_in,
    uint8_t       **data_out,
    size_t         *dlen_out,
    ib_flags_t     *result);


/**
 * @} IronBeeUtilDecode
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_DECODE_H_ */
