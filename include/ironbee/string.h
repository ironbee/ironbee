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
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/field.h>
#include <ironbee/mm.h>

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
 * Convert string const to string and length parameters.
 *
 * Allows using a NUL terminated string in place of two parameters
 * (char*, len) by calling strlen().
 *
 * @param s String
 */
#define IB_S2SL(s)  (s), strlen(s)

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
ib_status_t DLL_PUBLIC ib_string_to_num_ex(
    const char *s,
    size_t      slen,
    int         base,
    ib_num_t   *result
)
NONNULL_ATTRIBUTE(4);

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
ib_status_t DLL_PUBLIC ib_string_to_num(
    const char *s,
    int         base,
    ib_num_t   *result
)
NONNULL_ATTRIBUTE(3);

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
ib_status_t DLL_PUBLIC ib_string_to_time_ex(
    const char *s,
    size_t      slen,
    ib_time_t  *result
)
NONNULL_ATTRIBUTE(3);

/**
 * Convert a string to a time type, with error checking.
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
ib_status_t DLL_PUBLIC ib_string_to_time(
    const char *s,
    ib_time_t  *result
)
NONNULL_ATTRIBUTE(2);

/**
 * Convert a string to an @ref ib_float_t with error checking.
 *
 * Avoid using this function because it requires that a copy of the
 * input string be made to pass to strtold(). Prefer ib_string_to_float().
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
ib_status_t DLL_PUBLIC ib_string_to_float_ex(
    const char *s,
    size_t      slen,
    ib_float_t *result
)
NONNULL_ATTRIBUTE(3);

/**
 * Convert a string to an @ref ib_float_t with error checking.
 *
 * @param[in] s The string to convert.
 * @param[in] result The result.
 *
 * @returns
 *   - IB_OK on success
 *   - IB_EINVAL if no conversion could be performed, including because
 *               of a NULL or zero-length string.
 */
ib_status_t DLL_PUBLIC ib_string_to_float(
    const char *s,
    ib_float_t *result
)
NONNULL_ATTRIBUTE(2);

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
const char DLL_PUBLIC *ib_strstr(
    const char *haystack,
    size_t      haystack_len,
    const char *needle,
    size_t      needle_len
)
NONNULL_ATTRIBUTE(1, 3);

/**
 * Get a string representation of a number
 *
 * @param[in] mm The memory manager to use for allocations
 * @param[in] value The number to operate on
 *
 * @returns The buffer or NULL if allocation fails
 */
const char DLL_PUBLIC *ib_num_to_string(
    ib_mm_t mm,
    int64_t value
);

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
const char DLL_PUBLIC *ib_time_to_string(
    ib_mm_t   mm,
    ib_time_t value
);

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
const char DLL_PUBLIC *ib_float_to_string(
    ib_mm_t     mm,
    long double value
);

/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_STRING_H_ */
