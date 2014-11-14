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

#ifndef _IB_TYPE_CONVERT_H_
#define _IB_TYPE_CONVERT_H_

/**
 * @file
 * @brief IronBee --- General Type Definitions
 *
 * These are types that are used throughout IronBee.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>

#include <ironbee/types.h>
#include <ironbee/field.h>
#include <ironbee/mm.h>

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilTypeConvert Type Conversion
 * @ingroup IronBeeUtilTypes
 *
 * Type converstion definitions.
 *
 * @{
 */


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
ib_status_t DLL_PUBLIC ib_type_atoi_ex(
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
ib_status_t DLL_PUBLIC ib_type_atoi(
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
ib_status_t DLL_PUBLIC ib_type_atot_ex(
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
ib_status_t DLL_PUBLIC ib_type_atot(
    const char *s,
    ib_time_t  *result
)
NONNULL_ATTRIBUTE(2);

/**
 * Convert a string to an @ref ib_float_t with error checking.
 *
 * Avoid using this function because it requires that a copy of the
 * input string be made to pass to strtold(). Prefer ib_type_atof().
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
ib_status_t DLL_PUBLIC ib_type_atof_ex(
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
ib_status_t DLL_PUBLIC ib_type_atof(
    const char *s,
    ib_float_t *result
)
NONNULL_ATTRIBUTE(2);

/**
 * Get a string representation of a number
 *
 * @param[in] mm The memory manager to use for allocations
 * @param[in] value The number to operate on
 *
 * @returns The buffer or NULL if allocation fails
 */
const char DLL_PUBLIC *ib_type_itoa(
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
const char DLL_PUBLIC *ib_type_ttoa(
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
const char DLL_PUBLIC *ib_type_ftoa(
    ib_mm_t     mm,
    long double value
);

/**
 * Take two hex characters and convert them into a single byte.
 *
 * @param[in] high high order byte.
 * @param[in] low low order byte.
 *
 * @returns high and low combined into a single byte.
 */
int DLL_PUBLIC ib_type_htoa(char high, char low);


/**
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif /* _IB_TYPE_CONVERT_H_ */
