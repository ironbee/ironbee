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

#ifndef _IB_STRINGSET_H_
#define _IB_STRINGSET_H_

/**
 * @file
 * @brief IronBee --- String Set Utility Functions
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilStringSet String Set
 * @ingroup IronBeeUtil
 *
 * A library to construct a set of strings and then, given another string,
 * find the longest string in the set that is a prefix of the given string.
 *
 * To use:
 *
 * - Construct an array of @ref ib_stringset_entry_t representing your set.
 * - Allocate a @ref ib_stringset_t using your favorite allocation strategy.
 * - Pass the array and the @ref ib_string_t to ib_stringset_init().  Treat
 *   this as a *move* of the array from you to the stringset.  The array
 *   should not be subsequently used.  The array must live at least as long
 *   as the stringset.
 * - Query the stringset as desired.
 *
 * @{
 */

/**
 * An string set entry.
 */
typedef struct ib_stringset_entry_t ib_stringset_entry_t;
struct ib_stringset_entry_t
{
    /** String. */
    const char *string;

    /** Length of string. */
    size_t length;

    /** User specified data. */
    void *data;
};

/**
 * Set of strings.
 *
 * Treat as opaque.  See ib_stringset_init() for initialization and
 * ib_stringset_query() for access.
 *
 * Note: Although not intended for direct use, the definition is contained in
 * this file, enabling arbitrary allocation including on the stack.
 **/
typedef struct ib_stringset_t ib_stringset_t;

/** @cond internal */

/**
 * Set of strings.
 *
 * See @ref ib_ipset4_t for detailed discussion on approach.
 **/
struct ib_stringset_t
{
    /** Entries.  Sorted by ib_stringset_init(). */
    const ib_stringset_entry_t *entries;
    /** Number of entries. */
    size_t num_entries;
};

/** @endcond */

/**
 * Initialize a String Set.
 *
 * @param[in] set Set to initialized.  Should be allocated by user.
 * @param[in] entries Entries to add to set.  Should not be used after calling
 *                    this function.  Must live at least as long as @a set.
 * @param[in] num_entries Number of entries in @a entries.
 * @return IB_OK
 */
ib_status_t DLL_PUBLIC ib_stringset_init(
    ib_stringset_t       *set,
    ib_stringset_entry_t *entries,
    size_t                num_entries
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Query a String Set.
 *
 * @param[in] set Set to query.
 * @param[in] string String to find longest prefix of.
 * @param[in] string_length Length of @a string.
 * @param[out] out_entry Best matching entry if found.  May be NULL.
 * @return
 * - IB_OK if an entry was found.
 * - IB_ENOENT if no matching prefix.
 **/
ib_status_t DLL_PUBLIC ib_stringset_query(
    const ib_stringset_t        *set,
    const char                  *string,
    size_t                       string_length,
    const ib_stringset_entry_t **out_entry
)
NONNULL_ATTRIBUTE(1, 2);

/** @} IronBeeUtilStringSet */

#ifdef __cplusplus
}
#endif

#endif /* _IB_STRINGSET_H_ */
