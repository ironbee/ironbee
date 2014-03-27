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

#ifndef _IB_STRING_H_
#define _IB_STRING_H_

/**
 * @file
 * @brief IronBee --- String Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/field.h>     /* For ib_num_t */
#include <ironbee/mm.h>

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilString String
 * @ingroup IronBeeUtil
 *
 * Functions to manipulate strings.
 *
 * @{
 */


/**
 * Operations for functions that modify strings
 */
typedef enum {
    IB_STROP_INPLACE,     /**< Perform the operation in-place */
    IB_STROP_COPY,        /**< Always copy the input */
    IB_STROP_COW,         /**< Use copy-on-write semantics */
    IB_STROP_BUF,         /**< Use pre-allocated buffer */
} ib_strop_t;

/**
 * @name String operator result flags
 *
 * @{
 */
#define IB_STRFLAG_NONE          (0x0)   /**< No flags */
#define IB_STRFLAG_MODIFIED   (1 << 0)   /**< Output is different from input */
#define IB_STRFLAG_NEWBUF     (1 << 1)   /**< Output is a new buffer */
#define IB_STRFLAG_ALIAS      (1 << 2)   /**< Output is an alias into input */
/** @} */

/**
 * Generic string modification function, ex version
 *
 * @param[in] op String modify operation
 * @param[in] mm Memory manager to use for allocations
 * @param[in] data_in Pointer to input data
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Pointer to output data
 * @param[out] dlen_out Length of @a data_out
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
typedef ib_status_t (* ib_strmod_ex_fn_t) (ib_strop_t op,
                                           ib_mm_t mm,
                                           uint8_t *data_in,
                                           size_t dlen_in,
                                           uint8_t **data_out,
                                           size_t *dlen_out,
                                           ib_flags_t *result);

/**
 * Generic string modification function, string version
 *
 * @param[in] op String modify operation
 * @param[in] mm Memory manager for allocations
 * @param[in] str_in Data to operate on
 * @param[out] str_out Output data
 * @param[out] result Result flags (@c IB_STRFLAG_xxx)
 *
 * @returns Status code.
 */
typedef ib_status_t (* ib_strmod_fn_t) (ib_strop_t op,
                                        ib_mm_t mm,
                                        char *str_in,
                                        char **str_out,
                                        ib_flags_t *result);

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
 * Look for a character in a string that can have embedded NUL characters
 * in it.  This version will ignore NUL characters.
 *
 * @param[in] str String to search
 * @param[in] len length of the str
 * @param[in] c The character to search for
 * @param[out] offset Offset of the character; -1 if not found
 *
 * @return Status code
 */
ib_status_t DLL_PUBLIC ib_strchr_nul_ignore(const char *str,
                                            size_t len,
                                            int c,
                                            ssize_t *offset);

/**
 * Look for a character in a string that can have embedded NUL characters
 * in it.  This version returns an error if a NUL character is encountered
 * before len chars.
 *
 * @param[in] str String to search
 * @param[in] len length of the str
 * @param[in] c The character to search for
 * @param[out] offset Offset of the character; -1 if not found
 *
 * @return Status code
 */
ib_status_t DLL_PUBLIC ib_strchr_nul_error(const char *str,
                                           size_t len,
                                           int c,
                                           ssize_t *offset);

/**
 * Convert a string to a number, with error checking.
 *
 * @param[in] s String to convert.
 * @param[in] slen Length of string.
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
 * Convert a string to a time type, with error checking.
 *
 * The time string is an integer representing the number of microseconds
 * since the Epoch.
 *
 * @param[in] s String of integers representing microseconds since the epoch.
 * @param[in] slen Length of string.
 * @param[out] result Resulting time.
 *
 * @returns
 *   - IB_OK
 *   - IB_EINVAL
 */
ib_status_t DLL_PUBLIC ib_string_to_time_ex(const char *s,
                                            size_t slen,
                                            ib_time_t *result);

/**
 * Convert a string to a time type, with error checking
 *
 * The time string is an integer representing the number of microseconds
 * since the Epoch.
 *
 * @param[in] s String of integers representing microseconds since the epoch.
 * @param[out] result Resulting time.
 *
 * @returns
 *   - IB_OK
 *   - IB_EINVAL
 */
ib_status_t DLL_PUBLIC ib_string_to_time(const char *s,
                                         ib_time_t *result);

/**
 * Convert a string to an ib_float_t with error checking.
 *
 * Avoid using this function because it requires that a copy of the
 * input string be made to pass to strtold. Prefer @ref ib_string_to_float.
 *
 * @param[in] s The string to convert.
 * @param[in] slen The string length.
 * @param[in] result The result.
 *
 * @returns
 *   - IB_OK on success
 *   - IB_EALLOC on string dup failure.
 *   - IB_EINVAL if no conversion could be performed, including because
 *               of a NULL or zero-length string.
 */
ib_status_t ib_string_to_float_ex(const char *s,
                                  size_t slen,
                                  ib_float_t *result);

/**
 * Convert a string to an ib_float_t with error checking.
 *
 * @param[in] s The string to convert.
 * @param[in] result The result.
 *
 * @returns
 *   - IB_OK on success
 *   - IB_EINVAL if no conversion could be performed, including because
 *               of a NULL or zero-length string.
 */
ib_status_t ib_string_to_float(const char *s, ib_float_t *result);

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
 * Reverse strstr() clone that works with non-NUL terminated strings.
 *
 * @param[in] haystack String to search.
 * @param[in] haystack_len Length of @a haystack.
 * @param[in] needle String to search for.
 * @param[in] needle_len Length of @a needle.
 *
 * @returns Pointer to the last match in @a haystack, or NULL if no match
 * found.
 */
const char DLL_PUBLIC *ib_strrstr_ex(const char *haystack,
                                     size_t      haystack_len,
                                     const char *needle,
                                     size_t      needle_len);

/**
 * Simple ASCII lowercase function.
 *
 * @param[in] op String modify operation
 * @param[in] mm Memory manager for allocations
 * @param[in] data_in Data to convert to lower case
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Output data
 * @param[out] dlen_out Length of @a data_out
 * @param[out] result Result flags (@c @c IB_STRFLAG_xxx)
 *
 * @returns Status code.
 *
 * @note For non-ASCII (utf8, etc) you should use case folding.
 */
ib_status_t ib_strlower_ex(ib_strop_t op,
                           ib_mm_t mm,
                           uint8_t *data_in,
                           size_t dlen_in,
                           uint8_t **data_out,
                           size_t *dlen_out,
                           ib_flags_t *result);

/**
 * Simple ASCII lowercase function.
 *
 * @param[in] op String modify operation
 * @param[in] mm Memory manager for allocations
 * @param[in] str_in Data to convert to lower case
 * @param[out] str_out Output data
 * @param[out] result Result flags (@c IB_STRFLAG_xxx)
 *
 * @returns Status code.
 *
 * @note For non-ASCII (utf8, etc) you should use case folding.
 */
ib_status_t ib_strlower(ib_strop_t op,
                        ib_mm_t mm,
                        char *str_in,
                        char **str_out,
                        ib_flags_t *result);

/**
 * Simple ASCII trim left function.
 *
 * @param[in] op String modify operation
 * @param[in] mm Memory manager to use for allocations
 * @param[in] data_in Pointer to input data
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Pointer to output data
 * @param[out] dlen_out Length of @a data_out
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
ib_status_t ib_strtrim_left_ex(ib_strop_t op,
                               ib_mm_t mm,
                               uint8_t *data_in,
                               size_t dlen_in,
                               uint8_t **data_out,
                               size_t *dlen_out,
                               ib_flags_t *result);

/**
 * Simple ASCII trim left function.
 *
 * @param[in] op String trim operation
 * @param[in] mm Memory manager to use for allocations
 * @param[in] str_in Pointer to input data
 * @param[out] str_out Pointer to output data
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @returns Status code.
 */
ib_status_t ib_strtrim_left(ib_strop_t op,
                            ib_mm_t mm,
                            char *str_in,
                            char **str_out,
                            ib_flags_t *result);

/**
 * Simple ASCII trim right function.
 *
 * @param[in] op String trim operation
 * @param[in] mm Memory manager to use for allocations
 * @param[in] data_in Pointer to input data
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Pointer to output data
 * @param[out] dlen_out Length of @a data_out
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
ib_status_t ib_strtrim_right_ex(ib_strop_t op,
                                ib_mm_t mm,
                                uint8_t *data_in,
                                size_t dlen_in,
                                uint8_t **data_out,
                                size_t *dlen_out,
                                ib_flags_t *result);

/**
 * Simple ASCII trim right function.
 *
 * @param[in] op String trim operation
 * @param[in] mm Memory manager to use for allocations
 * @param[in] str_in Pointer to input data
 * @param[out] str_out Pointer to output data
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
ib_status_t ib_strtrim_right(ib_strop_t op,
                             ib_mm_t mm,
                             char *str_in,
                             char **str_out,
                             ib_flags_t *result);

/**
 * Simple ASCII trim left+right function.
 *
 * @param[in] op String trim operation
 * @param[in] mm Memory manager to use for allocations
 * @param[in] data_in Pointer to input data
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Pointer to output data
 * @param[out] dlen_out Length of @a data_out
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
ib_status_t ib_strtrim_lr_ex(ib_strop_t op,
                             ib_mm_t mm,
                             uint8_t *data_in,
                             size_t dlen_in,
                             uint8_t **data_out,
                             size_t *dlen_out,
                             ib_flags_t *result);

/**
 * Simple ASCII trim left+right function.
 *
 * @param[in] op String trim operation
 * @param[in] mm Memory manager to use for allocations
 * @param[in] str_in Pointer to input data
 * @param[out] str_out Pointer to output data
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
ib_status_t ib_strtrim_lr(ib_strop_t op,
                          ib_mm_t mm,
                          char *str_in,
                          char **str_out,
                          ib_flags_t *result);

/**
 * Delete all whitespace from a string (extended version)
 *
 * @param[in] op String trim operation
 * @param[in] mm Memory manager
 * @param[in] data_in Pointer to input data
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Pointer to output data
 * @param[in] dlen_out Length of @a data_out
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
ib_status_t ib_str_wspc_remove_ex(ib_strop_t op,
                                  ib_mm_t mm,
                                  uint8_t *data_in,
                                  size_t dlen_in,
                                  uint8_t **data_out,
                                  size_t *dlen_out,
                                  ib_flags_t *result);

/**
 * Delete all whitespace from a string (NUL terminated string version)
 *
 * @param[in] op String trim operation
 * @param[in] mm Memory manager
 * @param[in] str_in Pointer to input data
 * @param[out] str_out Pointer to output data
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
ib_status_t ib_str_wspc_remove(ib_strop_t op,
                               ib_mm_t mm,
                               char *str_in,
                               char **str_out,
                               ib_flags_t *result);

/**
 * Compress whitespace in a string (extended version)
 *
 * @param[in] op String trim operation
 * @param[in] mm Memory manager
 * @param[in] data_in Pointer to input data
 * @param[in] dlen_in Length of @a data_in
 * @param[out] data_out Pointer to output data
 * @param[in] dlen_out Length of @a data_out
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
ib_status_t ib_str_wspc_compress_ex(ib_strop_t op,
                                    ib_mm_t mm,
                                    uint8_t *data_in,
                                    size_t dlen_in,
                                    uint8_t **data_out,
                                    size_t *dlen_out,
                                    ib_flags_t *result);

/**
 * Compress whitespace in a string (NUL terminated string version)
 *
 * @param[in] op String trim operation
 * @param[in] mm Memory manager
 * @param[in] str_in Pointer to input data
 * @param[out] str_out Pointer to output data
 * @param[out] result Flags detailing the result (@c IB_STRFLAG_xx)
 *
 * @result Status code
 */
ib_status_t ib_str_wspc_compress(ib_strop_t op,
                                 ib_mm_t mm,
                                 char *str_in,
                                 char **str_out,
                                 ib_flags_t *result);

/**
 * Get the number of digits in a number
 *
 * @param[in] num The number to operate on
 *
 * @returns Number of digits (including '-')
 */
size_t ib_num_digits(int64_t num);

/**
 * Get the number of digits in a number
 *
 * @param[in] num The number to operate on
 *
 * @returns Number of digits
 */
size_t ib_unum_digits(uint64_t num);

/**
 * Get the size of a string buffer required to store a number
 *
 * @param[in] num The number to operate on
 *
 * @returns Required string length
 */
size_t ib_num_buf_size(int64_t num);

/**
 * Get the size of a string buffer required to store a number
 *
 * @param[in] num The number to operate on
 *
 * @returns Required string length
 */
size_t ib_unum_buf_size(uint64_t num);

/**
 * Get a string representation of a number
 *
 * @param[in] mm The memory manager to use for allocations
 * @param[in] value The number to operate on
 *
 * @returns The buffer or NULL if allocation fails
 */
const char *ib_num_to_string(ib_mm_t mm,
                             int64_t value);

/**
 * Get a string representation of a time.
 *
 * The string is the integer representing the number of milliseconds
 * since the epoch.
 *
 * @param[in] mm The memory manager to use for allocations
 * @param[in] value The number to operate on
 *
 * @returns The buffer or NULL if allocation fails
 */
const char *ib_time_to_string(ib_mm_t mm, ib_time_t value);
/**
 * Get a string representation of a number
 *
 * @param[in] mm The memory manager to use for allocations
 * @param[in] value The number to operate on
 *
 * @returns The buffer or NULL if allocation fails
 */
const char *ib_unum_to_string(ib_mm_t mm,
                              uint64_t value);

/**
 * Get a string representation of a floating point number.
 *
 * This currently uses a fixed length of 10.
 *
 * @param[in] mm The memory manager to use for allocations.
 * @param[in] value The floating point to print.
 *
 * @returns The buffer or NULL if allocation fails
 */
const char *ib_float_to_string(ib_mm_t mm,
                               long double value);

/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_STRING_H_ */
