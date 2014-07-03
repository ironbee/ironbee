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
 * @brief IronBee --- String Escape Functions
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/list.h>
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
 * Convert a bytestring to a json string with escaping
 *
 * @param[in] data_in Input data.
 * @param[in] dlen_in Length of data in @a data_in.
 * @param[out] data_out Where to write result to.
 * @param[in] dsize_out Maximum number of bytes that @a data_out can hold.
 * @param[out] dlen_out Length of data in @a data_out not including NUL.
 *
 * @returns IB_OK if successful.
 *          IB_ETRUNC if output buffer is not large enough.
 *
 * @note: @a dlen_out will be nul terminated and enclosed in quotes.
 */
ib_status_t DLL_PUBLIC ib_string_escape_json_buf(
    const uint8_t *data_in,
    size_t         dlen_in,
    char          *data_out,
    size_t         dsize_out,
    size_t        *dlen_out
)
NONNULL_ATTRIBUTE(3);

/**
 * Allocate a @c char* and escape @a src into it and return that @c char*.
 *
 * @param[in] mm Memory manager to use for allocations
 * @param[in] src The source string.
 * @param[in] src_len The length of @a src not including the final NUL.
 *
 * @returns
 * - NULL on error.
 * - Escape string otherwise.
 */
char DLL_PUBLIC *ib_util_hex_escape(
    ib_mm_t        mm,
    const uint8_t *src,
    size_t         src_len
);

/**
 * Unescape a Javascript-escaped string into the @a dst string buffer.
 *
 * The resultant buffer @a dst should also not be treated as a typical string
 * because a `\0` character could appear in the middle of the buffer.
 *
 * If IB_OK is not returned then @a dst and @a dst_len are left in an
 * inconsistent state.
 *
 * @param[out] dst string buffer that should be at least as long as
 *             @a src_len.
 * @param[out] dst_len the length of the decoded byte array. This will be
 *             equal to or shorter than @a src_len. Note that `strlen(dst)`
 *             could result in a smaller value than @a dst_len because of
 *             a `\0` character showing up in the middle of the array.
 * @param[in] src source string that is encoded.
 * @param[in] src_len the length of @a src.
 *
 * @returns
 * - IB_OK if successful
 * - IB_EINVAL if the string cannot be unescaped because of short escape codes
 *             or non-hex values being passed to escape codes.
 *             On a failure @a dst_len are left in an inconsistent state.
 */
ib_status_t DLL_PUBLIC ib_util_unescape_string(
    char       *dst,
    size_t     *dst_len,
    const char *src,
    size_t      src_len
);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ESCAPE_H_ */
