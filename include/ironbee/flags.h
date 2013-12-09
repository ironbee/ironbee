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

#ifndef _IB_FLAGS_H_
#define _IB_FLAGS_H_

/**
 * @file
 * @brief IronBee --- Flag Utility Functions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/list.h>
#include <ironbee/mpool.h>
#include <ironbee/strval.h>
#include <ironbee/types.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilFlags Flag Utility Functions and Macros
 * @ingroup IronBeeUtil
 *
 * Code related to flag manipulations
 *
 * @{
 */

/**
 * String key/flag operator
 */
typedef enum {
    FLAG_ADD,           /**< Add option bit to flags */
    FLAG_REMOVE,        /**< Remove option bit from flags */
    FLAG_SET,           /**< Set flags to option bit */
} ib_flags_op_t;

/**
 * String flag operation
 */
struct ib_flags_operation_t {
    ib_flags_op_t  op;       /**< Flag operator */
    ib_flags_t     flags;    /**< Corresponding flags */
};
typedef struct ib_flags_operation_t ib_flags_operation_t;

/**
 * Test if any of a set of flags is set
 *
 * @param[in] flags Flags to test
 * @param[in] check Flag bits to test check in @a flags
 *
 * @returns boolean value
 *
 * @internal
 * Implemented in: (self)
 * Tested in: tests/test_util_flags.cpp
 */
bool ib_flags_any(ib_flags_t flags, ib_flags_t check);
#define ib_flags_any(flags, check) \
    (((flags) & (check)) != 0)

/**
 * Test if all of a set of flags is set
 *
 * @param[in] flags Flags to test
 * @param[in] check Flag bits to test check in @a flags
 *
 * @returns boolean value
 *
 * @internal
 * Implemented in: (self)
 * Tested in: tests/test_util_flags.cpp
 */
bool ib_flags_all(ib_flags_t flags, ib_flags_t check);
#define ib_flags_all(flags, check) \
    (((flags) & (check)) == (check))

/**
 * Set flag bits
 *
 * @param[in] flags Flags to modify
 * @param[in] flags_set Flag bits to set in @a flags
 *
 * @returns updated flags
 *
 * @internal
 * Implemented in: (self)
 * Tested in: tests/test_util_flags.cpp
 */
bool ib_flags_set(ib_flags_t flags, ib_flags_t flags_set);
#define ib_flags_set(flags, flags_set) \
    ( (flags) |= (flags_set) )

/**
 * Clear flag bits
 *
 * @param[in] flags Flags to modify
 * @param[in] flags_clear Flag bits to clear in @a flags
 *
 * @returns updated flags
 *
 * @internal
 * Implemented in: (self)
 * Tested in: tests/test_util_flags.cpp
 */
bool ib_flags_clear(ib_flags_t flags, ib_flags_t flags_clear);
#define ib_flags_clear(flags, flags_clear) \
    ( (flags) &= (~(flags_clear)) )

/**
 * Merge a flag / mask with the previous value.
 *
 * @param[in] inflags Original flags to operate on
 * @param[in] flags Flags to merge in
 * @param[in] mask Mask of @a flags to merge in
 *
 * @returns Merged flags
 */
ib_flags_t ib_flags_merge(
    ib_flags_t  inflags,
    ib_flags_t  flags,
    ib_flags_t  mask);

/**
 * Merge a flag / mask with the previous value (macro version)
 *
 * @param[in] inflags Original flags to operate on
 * @param[in] flags Flags to merge in
 * @param[in] mask Mask of @a flags to merge in
 *
 * @returns Merged flags
 */
ib_flags_t IB_FLAGS_MERGE(
    ib_flags_t  inflags,
    ib_flags_t  flags,
    ib_flags_t  mask);
#define IB_FLAGS_MERGE(inflags, flags, mask) \
    ( ((flags) & (mask)) | ((inflags) & ~(mask)) )

/**
 * Parse and apply a single flag string from a name/value pair mapping.
 *
 * This function will treat @a str as a single item to lookup in @a map, with
 * the resulting flag bit(s) being applied to @a pflags and @a pmask.  If @a
 * num is zero and @a str starts with a '+', the mask @a pmask is reset to all
 * bits set.
 *
 * @param[in] map String / value mapping
 * @param[in] str String to lookup in @a map
 * @param[in] num Operator number
 * @param[in,out] pflags Pointer to flags
 * @param[in,out] pmask Pointer to flag mask
 *
 * @returns Status code:
 *  - IB_OK: All OK,
 *  - IB_ENOENT: @a str not found in @a map
 *  - IB_EINVAL: One or more of the pointer parameters is NULL
 */
ib_status_t ib_flags_string(
    const ib_strval_t *map,
    const char        *str,
    int                num,
    ib_flags_t        *pflags,
    ib_flags_t        *pmask);

/**
 * Parse and apply each node in a flag string list (list of flag strings).
 *
 * This is equivalent to calling @a ib_flags_string() for each
 * element of @a strlist with an incrementing num (starting at zero).
 *
 * @param[in] map String / value mapping
 * @param[in] strlist List of strings to lookup in @a map
 * @param[in,out] pflags Pointer to flags
 * @param[in,out] pmask Pointer to flag mask
 * @param[out] perror Pointer to offending string in case of an error (or NULL).
 *
 * @returns Status code:
 *  - IB_OK: All OK,
 *  - IB_EINVAL: One or more of the pointer parameters is NULL
 */
ib_status_t DLL_PUBLIC ib_flags_strlist(
    const ib_strval_t *map,
    const ib_list_t   *strlist,
    ib_flags_t        *pflags,
    ib_flags_t        *pmask,
    const char       **perror);

/**
 * Parse and apply a tokenized string as flags from a name/value pair mapping.
 *
 * This function will make a copy of @a str (using @a mp for allocation), and
 * then use strtok() to tokenize the copy (using @a sep as the delimiter
 * passed to strtok().  Each individual item will then be looked up in @a map,
 * and then applied.
 *
 * This is similar to tokenizing @a str into a list, and then invoking
 * ib_flags_strlist() on the list (except that no list is ever constructed).
 *
 * @param[in] map String / value mapping
 * @param[in] mp Memory pool to use for (temporary) allocations
 * @param[in] str @a sep separated string to lookup in @a map
 * @param[in] sep Separator character string (passed to strtok())
 * @param[in,out] pflags Pointer to flags
 * @param[in,out] pmask Pointer to flag mask
 *
 * @returns Status code:
 *  - IB_OK: All OK,
 *  - IB_ENOENT: @a str not found in @a map
 *  - IB_EINVAL: One or more of the pointer parameters is NULL
 */
ib_status_t DLL_PUBLIC ib_flags_strtok(
    const ib_strval_t *map,
    ib_mpool_t        *mp,
    const char        *str,
    const char        *sep,
    ib_flags_t        *pflags,
    ib_flags_t        *pmask);

/**
 * Parse a @a sep separated string from a name/value pair mapping into
 * a list of flag operations.
 *
 * This is similar to ib_flags_strtok() except that this function can be used
 * to parse a list of operations at configuration time, and later apply this
 * list at runtime.  Use ib_flags_oplist_apply() to apply this list of flag
 * operations at run-time.
 *
 * @param[in] map String / value mapping
 * @param[in] mp Memory pool to use for allocations
 * @param[in] str @a sep separated string to lookup in @a map
 * @param[in] sep Separator character string (passed to strtok())
 * @param[out] oplist List of (ib_flags_operation_t *)
 *
 * @returns Status code:
 *  - IB_OK: All OK,
 *  - IB_ENOENT: @a str not found in @a map
 *  - IB_EINVAL: One or more of the pointer parameters is NULL
 */
ib_status_t DLL_PUBLIC ib_flags_oplist_parse(
    const ib_strval_t *map,
    ib_mpool_t        *mp,
    const char        *str,
    const char        *sep,
    ib_list_t         *oplist);

/**
 * Apply a list of flag operations.  Use ib_flags_oplist_parse()
 * to parse a (tokenized) string into a list of operations.
 *
 * @param[out] oplist List of (ib_flags_operation_t *)
 * @param[in,out] pflags Pointer to flags
 * @param[in,out] pmask Pointer to flag mask
 *
 * @returns Status code:
 *  - IB_OK: All OK,
 *  - IB_EINVAL: One or more of the pointer parameters is NULL
 */
ib_status_t DLL_PUBLIC ib_flags_oplist_apply(
    const ib_list_t   *oplist,
    ib_flags_t        *pflags,
    ib_flags_t        *pmask);


/**
 * @} IronBeeUtilFlags
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_FLAGS_H_ */
