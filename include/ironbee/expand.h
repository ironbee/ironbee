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

#ifndef _IB_EXPAND_H_
#define _IB_EXPAND_H_

/**
 * @file
 * @brief IronBee - String Expansion Utility Functions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>
#include <ironbee/mpool.h>
#include <ironbee/hash.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilExpand Expand
 * @ingroup IronBeeUtil
 *
 * String expansion routines.
 *
 * @{
 */


/**
 * Expand a NUL-terminated string using the given hash.
 *
 * This function looks through @a str for instances of
 * @a startpat + _name_ + @a endpat (i.e. "%{FOO}"), then attempts to look up
 * each of such name found in @a hash.  If the name is not found in @a hash,
 * the "%{_name_}" sub-string is replaced with an empty string.  If the
 * name is found, the associated field value is used to replace "%{_name_}"
 * sub-string for string and numeric types (numbers are converted to strings);
 * for others, the replacement is an empty string.
 *
 * @param[in] mp Memory pool
 * @param[in] str String to expand
 * @param[in] prefix Prefix string (i.e. "%{")
 * @param[in] postfix Postfix string (i.e. "}")
 * @param[in] hash Hash from which to lookup and expand names in @a str
 * @param[out] result Resulting string
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC expand_str(ib_mpool_t *mp,
                                  const char *str,
                                  const char *prefix,
                                  const char *postfix,
                                  ib_hash_t *hash,
                                  char **result);

/**
 * Determine if a string would be expanded by @sa expand_str().
 *
 * This function looks through @a str for instances of
 * @a startpat + _name_ + @a endpat (i.e. "%{FOO}").
 *
 * @param[in] str String to check for expansion
 * @param[in] prefix Prefix string (i.e. "%{")
 * @param[in] postfix Postfix string (i.e. "}")
 * @param[out] result IB_TRUE if @a str would be expanded by @sa expand_str().
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC expand_test_str(const char *str,
                                       const char *prefix,
                                       const char *postfix,
                                       ib_bool_t *result);


/** @} IronBeeUtilExpand */


#ifdef __cplusplus
}
#endif

#endif /* _IB_EXPAND_H_ */
