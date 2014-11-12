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
 * Join strings in @a list using @a join_string into a single string.
 *
 * @param[in] join_string NUL-terminated string which will be printed into
 *            @a pout before each element of @a list except the first one.
 * @param[in] list A list of NUL-terminated strings which will be printed
 *            into @a pout separated by @a join_string.
 * @param[in] mm Memory manager that @a pout will be allocated from.
 * @param[out] pout This is pointed at the final NUL-terminated assembled
 *                  string.
 * @param[out] pout_len While @a pout is NUL-terminated, its length is made
 *              available to the caller.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC If a memory allocation fails. On failure @a pout and
 *             @a pout_len are untouched.
 */
ib_status_t ib_string_join(
    const char  *join_string,
    ib_list_t   *list,
    ib_mm_t      mm,
    const char **pout,
    size_t      *pout_len
)
NONNULL_ATTRIBUTE(1, 2, 4, 5);

/**
 * @} IronBeeUtil
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_STRING_H_ */
