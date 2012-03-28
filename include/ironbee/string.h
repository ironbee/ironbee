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
 *****************************************************************************/

#ifndef _IB_STRING_H_
#define _IB_STRING_H_

/**
 * @file
 * @brief IronBee - String Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilString String
 * @ingroup IronBeeUtil
 * @{
 */

/**
 * Convert string const to string and length parameters.
 *
 * Allows using a NUL terminated string in place of two parameters
 * (char*, len) by calling strlen().
 *
 * @param s String
 */
#define IB_S2SL(s)  (s), strlen(s)

/**
 * Convert string const to unsigned string and length parameters.
 *
 * Allows using a NUL terminated string in place of two parameters
 * (uint8_t*, len) by calling strlen().
 *
 * @param s String
 */
#define IB_S2USL(s)  ((uint8_t *)(s)), strlen(s)

/**
 * strchr() equivalent that operates on a string buffer with a length
 * which can have embedded NUL characters in it.
 *
 * @param[in] s String
 * @param[in] l Length
 * @param[in] c Character to search for
 */
char DLL_PUBLIC *ib_strchr(const char *s, size_t l, int c);

/**
 * Convert a string to a number, with error checking
 *
 * @param[in] s String to convert
 * @param[in] slen Length of string
 * @param[in] base Base passed to strtol() -- see strtol() documentation
 * for details.
 * @param[out] result Resulting number.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_string_to_num_ex(const char *s,
                                           size_t slen,
                                           int base,
                                           ib_num_t *result);

/**
 * Convert a string to a number, with error checking
 *
 * @param[in] s String to convert
 * @param[in] base Base passed to strtol() -- see strtol() documentation
 * for details.
 * @param[out] result Resulting number.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_string_to_num(const char *s,
                                        int base,
                                        ib_num_t *result);

/**
 * strstr() clone that works with non-NUL terminated strings.
 *
 * @param[in] haystack String to search.
 * @param[in] haystack_len Length of @a haystack.
 * @param[in] needle String to search for.
 * @param[in] needle_len Length of @a needle.
 *
 * @returns Pointer to the first match in @a haystack, or NULL if no match
 * found.
 */
const char DLL_PUBLIC *ib_strstr_ex(const char *haystack,
                                    size_t      haystack_len,
                                    const char *needle,
                                    size_t      needle_len);

/**
 * Simple ASCII lowercase function.
 *
 * @param[in] data Data to convert to lower case
 * @param[in] dlen Length of @a data
 * @param[out] modified IB_TRUE if the string was modified, else IB_FALSE.
 *
 * @note For non-ASCII (utf8, etc) you should use case folding.
 */
ib_status_t ib_strlower_ex(uint8_t *data,
                           size_t dlen,
                           ib_bool_t *modified);

/**
 * Simple ASCII lowercase function.
 *
 * @param[in] data Data to convert to lower case
 * @param[out] modified IB_TRUE if the string was modified, else IB_FALSE.
 *
 * @note For non-ASCII (utf8, etc) you should use case folding.
 */
ib_status_t ib_strlower(char *data,
                        ib_bool_t *modified);

/**
 * Simple ASCII trim left function.
 *
 * @param[in] data_in Pointer to input data
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Pointer to output data
 * @param[in] dlen_out Length of @a data_out
 * @param[out] modified IB_TRUE if the string was modified, else IB_FALSE.
 *
 * @note: This is an in-place operation which may change the data length.
 */
ib_status_t ib_strtrim_left_ex(uint8_t *data_in,
                               size_t dlen_in,
                               uint8_t **data_out,
                               size_t *dlen_out,
                               ib_bool_t *modified);

/**
 * Simple ASCII trim left function.
 *
 * @param[in] data_in Pointer to input data
 * @param[out] data_out Pointer to output data
 * @param[out] modified IB_TRUE if the string was modified, else IB_FALSE.
 *
 * @note: This is an in-place operation which may change the data length.
 */
ib_status_t ib_strtrim_left(char *data_in,
                            char **data_out,
                            ib_bool_t *modified);

/**
 * Simple ASCII trim right function.
 *
 * @param[in] data_in Pointer to input data
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Pointer to output data
 * @param[in] dlen_out Length of @a data_out
 * @param[out] modified IB_TRUE if the string was modified, else IB_FALSE.
 *
 * @note: This is an in-place operation which may change the data length.
 */
ib_status_t ib_strtrim_right_ex(uint8_t *data_in,
                                size_t dlen_in,
                                uint8_t **data_out,
                                size_t *dlen_out,
                                ib_bool_t *modified);

/**
 * Simple ASCII trim right function.
 *
 * @param[in] data_in Pointer to input data
 * @param[out] data_out Pointer to output data
 * @param[out] modified IB_TRUE if the string was modified, else IB_FALSE.
 *
 * @note: This is an in-place operation which may change the data length.
 */
ib_status_t ib_strtrim_right(char *data_in,
                             char **data_out,
                             ib_bool_t *modified);


/**
 * Simple ASCII trim left+right function.
 *
 * @param[in] data_in Pointer to input data
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Pointer to output data
 * @param[in] dlen_out Length of @a data_out
 * @param[out] modified IB_TRUE if the string was modified, else IB_FALSE.
 *
 * @note: This is an in-place operation which may change the data length.
 */
ib_status_t ib_strtrim_lr_ex(uint8_t *data_in,
                             size_t dlen_in,
                             uint8_t **data_out,
                             size_t *dlen_out,
                             ib_bool_t *modified);

/**
 * Simple ASCII trim left+right function.
 *
 * @param[in] data_in Pointer to input data
 * @param[out] data_out Pointer to output data
 * @param[out] modified IB_TRUE if the string was modified, else IB_FALSE.
 *
 * @note: This is an in-place operation which may change the data length.
 */
ib_status_t ib_strtrim_lr(char *data_in,
                          char **data_out,
                          ib_bool_t *modified);

/**
 * Delete all whitespace from a string (extended version)
 *
 * @param[in] mp Memory pool
 * @param[in] data_in Pointer to input data
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Pointer to output data
 * @param[in] dlen_out Length of @a data_out
 * @param[out] modified IB_TRUE if the string was modified, else IB_FALSE.
 */
ib_status_t ib_str_wspc_remove_ex(ib_mpool_t *mp,
                                  const uint8_t *data_in,
                                  size_t dlen_in,
                                  uint8_t **data_out,
                                  size_t *dlen_out,
                                  ib_bool_t *modified);

/**
 * Delete all whitespace from a string (NUL terminated string version)
 *
 * @param[in] mp Memory pool
 * @param[in] data_in Pointer to input data
 * @param[out] data_out Pointer to output data
 * @param[out] modified IB_TRUE if the string was modified, else IB_FALSE.
 *
 * @note: This is an in-place operation which may change the data length.
 */
ib_status_t ib_str_wspc_remove(ib_mpool_t *mp,
                               const char *data_in,
                               char **data_out,
                               ib_bool_t *modified);

/**
 * Compress whitespace in a string (extended version)
 *
 * @param[in] mp Memory pool
 * @param[in] data_in Pointer to input data
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Pointer to output data
 * @param[in] dlen_out Length of @a data_out
 * @param[out] modified IB_TRUE if the string was modified, else IB_FALSE.
 */
ib_status_t ib_str_wspc_compress_ex(ib_mpool_t *mp,
                                    const uint8_t *data_in,
                                    size_t dlen_in,
                                    uint8_t **data_out,
                                    size_t *dlen_out,
                                    ib_bool_t *modified);

/**
 * Compress whitespace in a string (NUL terminated string version)
 *
 * @param[in] mp Memory pool
 * @param[in] data_in Pointer to input data
 * @param[out] data_out Pointer to output data
 * @param[out] modified IB_TRUE if the string was modified, else IB_FALSE.
 *
 * @note: This is an in-place operation which may change the data length.
 */
ib_status_t ib_str_wspc_compress(ib_mpool_t *mp,
                                 const char *data_in,
                                 char **data_out,
                                 ib_bool_t *modified);

/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_STRING_H_ */
