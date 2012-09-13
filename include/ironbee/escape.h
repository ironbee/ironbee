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
 * @brief IronBee --- String Utility Functions
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
 * @defgroup IronBeeUtilStringEscape String Escaping and Un-escaping
 * @ingroup IronBeeUtilString
 *
 * Functions to escape and un-escape strings.
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
 *
 * @internal
 * Implemented in: util/escape.c
 * Tested in: tests/test_util_escape.cc
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
 *
 * @internal
 * Implemented in: util/escape.c
 * Tested in: tests/test_util_escape.cc
 */
ib_status_t ib_string_escape_json(ib_mpool_t *mp,
                                  const char *data_in,
                                  char **data_out,
                                  ib_flags_t *result);

/**
 * Malloc a @c char* and escape @a src into it and return that @c char*.
 *
 * Escaping is done by finding ASCII non-printable characters
 * and replacing them with @c 0xhh where @c hh is the hexadecimal value
 * of the character.
 *
 * This utility is intended to assist in logging otherwise unprintable
 * strings for information purposes. There is no way to distinguish
 * between the string "hi0x00" and "hi" where the last byte is a zero once
 * the two strings have passed through this function.
 *
 * The returned string must be free'ed.
 *
 * @param[in] src The source string.
 * @param[in] src_len The length of @a src not including the final NUL.
 *
 * @returns
 * - NULL on error;
 * - a NUL-terminated string that must be free'ed on success.
 *
 * @internal
 * Implemented in: util/escape.c
 * Tested in: tests/test_util_hex_escape.cc
 */
char DLL_PUBLIC * ib_util_hex_escape(const char *src,
                                     size_t src_len);

/**
 * Malloc a @c char* and escape @a src into it and return that @c char*.
 *
 * Escaping is done by finding ASCII non-printable characters
 * and replacing them with @c 0xhh where @c hh is the hexadecimal value
 * of the character.
 *
 * This utility is intended to assist in logging otherwise unprintable
 * strings for information purposes. There is no way to distinguish
 * between the string "hi0x00" and "hi" where the last byte is a zero once
 * the two strings have passed through this function.
 *
 * The returned string must be free'ed.
 *
 * @param[in] src The source string.
 * @param[in] src_len The length of @a src not including the final NUL.
 *
 * @returns
 * - NULL on error;
 * - a NUL-terminated string that must be free'ed on success.
 *
 * @internal
 * Implemented in: util/escape.c
 * Tested in: tests/test_util_hex_escape.cc
 */
char DLL_PUBLIC * ib_util_hex_escape(const char *src,
                                     size_t src_len);

/**
 * When passed to @ref ib_util_unescape_string an escaped null character will
 * results in the string not being parsed and IB_EINVAL being returned.
 */
#define IB_UTIL_UNESCAPE_NONULL    (1U << 0)
#define IB_UTIL_UNESCAPE_NULTERMINATE (1U << 1)

/**
 * Unescape a Javascript-escaped string into the @a dst string buffer.
 *
 * Decode the contents of @a str into @a dst. Then terminate @a dst with \0
 * if @a flags includes IB_UTIL_UNESCAPE_NULTERMINATE. In this case,
 * @a dst must be @a src_len+1 in size.
 *
 * Because @a src may be a segment in a larger character buffer,
 * @a src is not treated as a \0 terminated string, but is
 * processed using the given @a src_len.
 *
 * The resultant buffer @a dst should also not be treated as a typical string
 * because a \0 character could appear in the middle of the buffer unless
 * IB_UTIL_UNESCAPE_NONULL is set in @a flags.
 *
 * If IB_OK is not returned then @a dst and @a dst_len are left in an
 * inconsistent state.
 *
 * @param[out] dst string buffer that should be at least as long as
 *             @a src_len or @a src_len+1 if IB_UTIL_UNESCAPE_NULTERMINATE
 *             is set.
 * @param[out] dst_len the length of the decoded byte array. This will be
 *             equal to or shorter than @a src_len. Note that srclen(dst)
 *             could result in a smaller value than @a dst_len because of
 *             a \0 character showing up in the middle of the array.
 * @param[in] src source string that is encoded.
 * @param[in] src_len the length of @a src.
 * @param[in] flags Flags that affect how the string is processed.
 *
 * @returns IB_OK if successful. IB_EINVAL if the string cannot be unescaped
 *          because of short escape codes or non-hex values being passed
 *          to escape codes.
 *
 *          IB_EBADVAL is returned if a flag is set and the string cannot
 *          be decoded because of the flag settings.
 *
 *          On a failure @a dst_len are left in an inconsistent state.
 *
 * @internal
 * Implemented in: util/util.c
 * Tested in: tests/test_util_unescape_string.cc
 */
ib_status_t DLL_PUBLIC ib_util_unescape_string(char *dst,
                                               size_t *dst_len,
                                               const char *src,
                                               size_t src_len,
                                               uint32_t flags);

/**
 * @} IronBeeUtilString
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ESCAPE_H_ */
