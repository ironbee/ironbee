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
 * @param[in] data_in Input data
 * @param[in] dlen_in Length of data in @a data_in
 * @param[in] add_nul Append a nul byte?
 * @param[in] quote Add surrounding quotes?
 * @param[out] data_out Output buffer
 * @param[in] dsize_out Size of @a data_out
 * @param[out] dlen_out Length of data in @a data_out (or NULL)
 * @param[out] result Result flags (IB_STRFLAG_xx) (or NULL)
 *
 * @returns IB_OK if successful
 *          IB_EALLOC if allocation errors occur
 *
 * @note: @a dlen_out will include the nul byte if requested by @a add_nul
 *
 * @internal
 * Implemented in: util/escape.c
 * Tested in: tests/test_util_escape.cpp
 */
ib_status_t DLL_PUBLIC ib_string_escape_json_buf_ex(
    const uint8_t *data_in,
    size_t         dlen_in,
    bool           add_nul,
    bool           quote,
    char          *data_out,
    size_t         dsize_out,
    size_t        *dlen_out,
    ib_flags_t    *result
);

/**
 * Convert a bytestring to a json string with escaping
 *
 * @param[in] data_in Input data
 * @param[in] quote Quote the string?
 * @param[out] data_out Output buffer
 * @param[in] dsize_out Size of @a data_out
 * @param[out] dlen_out Length of data in @a data_out (or NULL)
 * @param[out] result Result flags (IB_STRFLAG_xx) (or NULL)
 *
 * @returns IB_OK if successful
 *          IB_EALLOC if allocation errors occur
 *
 * @note: @a dlen_out will NOT include the nul byte
 *
 * @internal
 * Implemented in: util/escape.c
 * Tested in: tests/test_util_escape.cpp
 */
ib_status_t ib_string_escape_json_buf(
    const char *data_in,
    bool        quote,
    char       *data_out,
    size_t      dsize_out,
    size_t     *dlen_out,
    ib_flags_t *result
);

/**
 * Convert a list of NUL strings to a json string with escaping
 *
 * @param[in] items List of strings to escape
 * @param[in] quote Quote the strings?
 * @param[in] join String to use for joining items in @a items
 * @param[out] data_out Output buffer
 * @param[in] dsize_out Size of @a data_out
 * @param[out] dlen_out Length of data in @a data_out (or NULL)
 * @param[out] result Result flags (IB_STRFLAG_xx) (or NULL)
 *
 * @returns IB_OK if successful
 *          IB_ETRUNC if @a data_out is truncated
 *
 * @note: @a dlen_out will NOT include the nul byte
 *
 * @internal
 * Implemented in: util/escape.c
 * Tested in: tests/test_util_escape.cpp
 */
ib_status_t DLL_PUBLIC ib_strlist_escape_json_buf(
    const ib_list_t *items,
    bool             quote,
    const char      *join,
    char            *data_out,
    size_t           dsize_out,
    size_t          *dlen_out,
    ib_flags_t      *result
);

/**
 * Convert a bytestring to a json string with escaping
 *
 * @param[in] mp Memory pool to use for allocations
 * @param[in] data_in Input data
 * @param[in] dlen_in Length of data in @a data_in
 * @param[in] nul Save room for and append a nul byte?
 * @param[in] quote Save room for and add quotes?
 * @param[out] data_out Output data
 * @param[out] dlen_out Length of data in @a data_out (or NULL)
 * @param[out] result Result flags (IB_STRFLAG_xx)
 *
 * @returns IB_OK if successful
 *          IB_EALLOC if allocation errors occur
 *
 * @internal
 * Implemented in: util/escape.c
 * Tested in: tests/test_util_escape.cpp
 */
ib_status_t ib_string_escape_json_ex(ib_mpool_t *mp,
                                     const uint8_t *data_in,
                                     size_t dlen_in,
                                     bool nul,
                                     bool quote,
                                     char **data_out,
                                     size_t *dlen_out,
                                     ib_flags_t *result);

/**
 * Convert a c-string to a json string with escaping
 *
 * @param[in] mp Memory pool to use for allocations
 * @param[in] data_in Input data
 * @param[in] quote Save room for and add quotes?
 * @param[out] data_out Output data
 * @param[out] result Result flags (IB_STRFLAG_xx)
 *
 * @returns IB_OK if successful
 *          IB_EALLOC if allocation errors occur
 *
 * @internal
 * Implemented in: util/escape.c
 * Tested in: tests/test_util_escape.cpp
 */
ib_status_t DLL_PUBLIC ib_string_escape_json(
    ib_mpool_t  *mp,
    const char  *data_in,
    bool         quote,
    char       **data_out,
    ib_flags_t  *result);

/**
 * Allocate a buffer large enough to escape a string of length @a src_len
 * with optional padding of @a pad characters
 *
 * @param[in] mp Memory pool to use for allocations or NULL to use malloc()
 * @param[in] src_len Source string length
 * @param[in] pad Padding size (can be zero)
 * @param[out] pbuf Pointer to newly allocated buffer
 * @param[out] psize Pointer to size of @a pbuf
 *
 * @returns Status code:
 *    - IB_OK All OK
 *    - IB_EALLOC Allocation error
 */
ib_status_t DLL_PUBLIC ib_util_hex_escape_alloc(
    ib_mpool_t    *mp,
    size_t         src_len,
    size_t         pad,
    char         **pbuf,
    size_t        *psize);

/**
 * Hex-escape a string into a pre-allocated buffer.
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
 * @param[in] src Source buffer to escape
 * @param[in] src_len Length of @a src
 * @param[in] buf Destination buffer
 * @param[in] buf_size Size of @a buf (or NULL)
 *
 * @returns Length of the final string in @a buf
 */
size_t DLL_PUBLIC ib_util_hex_escape_buf(
    const uint8_t   *src,
    size_t           src_len,
    char            *buf,
    size_t           buf_size);

/**
 * Allocate a @c char* and escape @a src into it and return that @c char*.
 *
 * The returned string must be released via free() if @a mp is NULL.
 *
 * @param[in] mp Memory pool to use for allocations (NULL: use malloc())
 * @param[in] src The source string.
 * @param[in] src_len The length of @a src not including the final NUL.
 *
 * @returns
 * - NULL on error;
 * - a NUL-terminated string that must be free'ed on success.
 *
 * @internal
 * Implemented in: util/escape.c
 * Tested in: tests/test_util_hex_escape.cpp
 */
char DLL_PUBLIC *ib_util_hex_escape(
    ib_mpool_t      *mp,
    const uint8_t   *src,
    size_t           src_len);

/**
 * Unescape a Javascript-escaped string into the @a dst string buffer.
 *
 * The resultant buffer @a dst should also not be treated as a typical string
 * because a \0 character could appear in the middle of the buffer.
 *
 * If IB_OK is not returned then @a dst and @a dst_len are left in an
 * inconsistent state.
 *
 * @param[out] dst string buffer that should be at least as long as
 *             @a src_len.
 * @param[out] dst_len the length of the decoded byte array. This will be
 *             equal to or shorter than @a src_len. Note that srclen(dst)
 *             could result in a smaller value than @a dst_len because of
 *             a \0 character showing up in the middle of the array.
 * @param[in] src source string that is encoded.
 * @param[in] src_len the length of @a src.
 *
 * @returns
 * - IB_OK if successful
 * - IB_EINVAL if the string cannot be unescaped because of short escape codes
 *             or non-hex values being passed to escape codes.
 *             On a failure @a dst_len are left in an inconsistent state.
 *
 * @internal
 * Implemented in: util/util.c
 * Tested in: tests/test_util_unescape_string.cpp
 */
ib_status_t DLL_PUBLIC ib_util_unescape_string(
    char       *dst,
    size_t     *dst_len,
    const char *src,
    size_t      src_len
);

/**
 * @} IronBeeUtilString
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ESCAPE_H_ */
