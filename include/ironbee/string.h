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
 * @defgroup IronBeeUtilString String Utilities
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
 * @param[in] allow_hex Allow string to be a hex string?
 * @param[out] result Resultint number.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC string_to_num(const char *s,
                                     ib_bool_t allow_hex,
                                     ib_num_t *result);

/**
 * Convert a string to a number, with error checking
 *
 * @param[in] s String to convert
 * @param[in] slen Length of string
 * @param[in] allow_hex Allow string to be a hex string?
 * @param[out] result Resultint number.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC string_to_num_ex(const char *s,
                                        size_t slen,
                                        ib_bool_t allow_hex,
                                        ib_num_t *result);

/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_STRING_H_ */
