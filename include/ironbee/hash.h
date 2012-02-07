/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyrighash ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     hashtp://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#ifndef _IB_HASH_H_
#define _IB_HASH_H_

/**
 * @file
 * @brief IronBee - Hash Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include <sys/types.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>


#include <ironbee/build.h>
#include <ironbee/release.h>
#include <ironbee/types.h>
#include <ironbee/list.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeHash Hash
 * @ingroup IronBeeUtil
 *
 * @{
 */

/**
 * Function pointer for a hash function.
 *
 * A hash function converts keys (byte sequences) into hash values (unsinged
 * integers).  A good hash function is vital to the performance of a hash.
 *
 * @param[in] key   Key to hash.
 * @param[in] len   Length of @a key.
 * @param[in] flags Flags.  Currently, only flag is IB_HASH_FLAG_NOCASE.
 *
 * @returns Hash value of \a key.
 **/
typedef unsigned int (*ib_hashfunc_t)(
    const void *key,
    size_t      len,
    uint8_t     flags
);

/* Options */

/**
 * Ignore case during lookup.
 *
 * Instructs the hash function to treat upper and lower case letters as the
 * same.  That is, changing the case of any byte of the key should not alter
 * the hash value of the key.
 **/
#define IB_HASH_FLAG_NOCASE    0x01

/**
 * DJB2 Hash Function (Dan Bernstein).
 *
 * This is the default hash function.
 *
 * @code
 * hash = 5381
 * for c in ckey
 *   hash = hash * 33 + c
 * @endcode
 *
 * @param[in] key   The key to hash.
 * @param[in] len   Length of @a key.
 * @param[in] flags If contains IB_HASH_FLAG_NOCASE, will convert upper
 *                  case bytes to lower case.  All other bits are ignored.
 *
 * @returns Hash value of @a key.
 */
unsigned int DLL_PUBLIC ib_hashfunc_djb2(
    const void *key,
    size_t      len,
    uint8_t     flags
);

/**
 * Create a hash table.
 *
 * @sa ib_hash_create()
 *
 * @param[out] hash    The newly created hash table.
 * @param[in]  pool    Memory pool to use.
 * @param[in]  size    The number of slots in the hash table.
 * @param[in]  hash_fn Hash function to use, e.g., ib_hashfunc_djb2().
 * @param[in]  flags   Flags to pass to the hash function, e.g.,
 *                     IB_HASH_FLAG_NOCASE
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_hash_create_ex(
    ib_hash_t     **hash,
    ib_mpool_t     *pool,
    unsigned int    size,
    ib_hashfunc_t   hash_fn,
    uint8_t         flags
);

/**
 * Create a hash table with nocase flag and a default size.
 *
 * @sa ib_hash_create_ex()
 *
 * @param[out] hash   The newly created hash table.
 * @param[in]  pool Memory pool to use.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_hash_create(
    ib_hash_t  **hash,
    ib_mpool_t  *pool
);

/**
 * Access memory pool of @a hash.
 *
 * @param[in] hash Hash table.
 *
 * @returns Memory pool of @a hash.
 **/
ib_mpool_t DLL_PUBLIC *ib_hash_pool(
    ib_hash_t *hash
);

/**
 * Clear hash table @a hash.
 *
 * Removes all entries from @a hash.
 *
 * @param[in,out] hash Hash table to clear.
 */
void DLL_PUBLIC ib_hash_clear(
     ib_hash_t *hash
);

/**
 * Fetch value from @a hash for key @a key.
 *
 * @sa ib_hash_get()
 * @sa ib_hash_get_nocase()
 *
 * @param[out] value Address which value is written.
 * @param[in]  hash  Hash table.
 * @param[in]  key   Key to lookup.
 * @param[in]  len   Length of @a key.
 * @param[in]  flags Flags to use during lookup, e.g., IB_HASH_FLAG_NOCASE.
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a key is not in hash table.
 * - IB_EINVAL if any parameters are invalid.
 */
ib_status_t DLL_PUBLIC ib_hash_get_ex(
    void      *value,
    ib_hash_t *hash,
    void      *key,
    size_t     len,
    uint8_t    flags
);

/**
 * Get value for @a key (NULL terminated char string) from @a hash.
 *
 * @sa ib_hash_get_ex()
 * @sa ib_hash_get_nocase()
 *
 * @param[out] value Address which value is written.
 * @param[in]  hash  Hash table.
 * @param[in]  key   Key to lookup
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a key is not in hash table.
 * - IB_EINVAL if any parameters are invalid.
 */
ib_status_t DLL_PUBLIC ib_hash_get(
    void       *value,
    ib_hash_t  *hash,
    const char *key
);

/**
 * Get value for @a key (NULL terminated char string) from @a hash, ignoring
 * case.
 *
 * @sa ib_hash_get_ex()
 * @sa ib_hash_get()
 *
 * @param[out] value Address which value is written.
 * @param[in]  hash  Hash table.
 * @param[in]  key   Key to lookup.
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a key is not in hash table.
 * - IB_EINVAL if any parameters are invalid.
 */
ib_status_t DLL_PUBLIC ib_hash_get_nocase(
    void       *value,
    ib_hash_t  *hash,
    const char *key
);


/**
 * Push every entry from @a hash onto @a list.
 *
 * Order is undefined.
 *
 * @param[in,out] list List to push values.
 * @param[in]     hash   Hash table to take values from.
 *
 * @returns
 * - IB_OK if any elements are pushed.
 * - IB_ENOENT if @a hash is empty.
 */
ib_status_t DLL_PUBLIC ib_hash_get_all(
    ib_list_t *list,
    ib_hash_t *hash
);

/**
 * Set value of @a key in @a hash to @a data.
 *
 * @sa ib_hash_set()
 *
 * @param[in,out] hash  Hash table.
 * @param[in]     key   Key.
 * @param[in]     len   Length of @a key
 * @param[in]     value Value.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC if @a hash attempted to grow and failed.
 */
ib_status_t DLL_PUBLIC ib_hash_set_ex(
    ib_hash_t  *hash,
    const void *key,
    size_t      len,
    const void *value
);

/**
 * Set value of @a key (NULL terminated char string) in @a hash to @a value.
 *
 * @sa ib_hash_set_ex()
 *
 * @param[in,out] hash  Hash table.
 * @param[in]     key   Key.
 * @param[in]     value Value.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC if @a hash attempted to grow and failed.
 */
ib_status_t DLL_PUBLIC ib_hash_set(
    ib_hash_t  *hash,
    const char *key,
    void       *value
);

/**
 * Remove value for @a key from @a data.
 *
 * @sa ib_hash_remove()
 *
 * @param[in,out] value If non-NULL, removed value will be stored here.
 * @param[in,out] hash  Hash table.
 * @param[in]     key   Key.
 * @param[in]     len   Length of @a key.
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a key is not in hash table.
 * - IB_EINVAL if any parameters are invalid.
 */
ib_status_t DLL_PUBLIC ib_hash_remove_ex(
    void      *value,
    ib_hash_t *hash,
    void      *key,
    size_t     len
);

/**
 * Remove value for @a key (NULL terminated char string) from @a data.
 *
 * @sa ib_hash_remove_ex()
 *
 * @param[in,out] value If non-NULL, removed value will be stored here.
 * @param[in,out] hash  Hash table.
 * @param[in]     key   Key.
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a key is not in hash table.
 * - IB_EINVAL if any parameters are invalid.
 */
ib_status_t DLL_PUBLIC ib_hash_remove(
    void       *value,
    ib_hash_t  *hash,
    const char *key
);

/** @} IronBeeUtilHash */

#ifdef __cplusplus
}
#endif

#endif /* _IB_HASH_H_ */
