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
 ****************************************************************************/

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
 * Hash table.
 *
 * A map of keys (byte sequences or strings) to values (@c void*).
 *
 * @warning The @c void* value type works well for pointers but can cause
 * problems if other data is stored in there.  If you store non-pointer
 * types, make sure they are as wide as your pointers are.
 *
 * @sa IronBeeHash
 * @sa hash.h
 **/
typedef struct ib_hash_t ib_hash_t;

/**
 * Function pointer for a hash function.
 *
 * A hash function converts keys (byte sequences) into hash values (unsinged
 * integers).  A good hash function is vital to the performance of a hash.
 *
 * @param[in] key        Key to hash.
 * @param[in] key_length Length of @a key.
 *
 * @returns Hash value of @a key.
 **/
typedef unsigned int (*ib_hash_function_t)(
    const void *key,
    size_t      key_length
);

/**
 * Function pointer for a key equality function.
 *
 * Should return 1 if @a a and @a b are to be considered equal keys and 0
 * otherwise.
 *
 * @param[in] a        First key.
 * @param[in] a_length Length of @a a.
 * @param[in] b        Second key.
 * @param[in] b_length Length of @a b.
 *
 * @returns 1 if @a a and @a b are to be considered equal and 0 otherwise.
 **/
typedef int (*ib_hash_equal_t)(
    const void *a,
    size_t a_length,
    const void *b,
    size_t b_length
);

/**
 * @name Hash functions and equality predicates.
 * Functions suitable for use as ib_hash_function_t and ib_hash_equal_t.
 *
 * @sa ib_hash_create_ex()
 */
/*@{*/

/**
 * DJB2 Hash Function (Dan Bernstein).
 *
 * This is the default hash function for ib_hash_create().
 *
 * @sa ib_hashfunc_djb2_nocase().
 *
 * @code
 * hash = 5381
 * for c in ckey
 *   hash = hash * 33 + c
 * @endcode
 *
 * @param[in] key        The key to hash.
 * @param[in] key_length Length of @a key.
 *
 * @returns Hash value of @a key.
 */
unsigned int DLL_PUBLIC ib_hashfunc_djb2(
    const void *key,
    size_t      key_length
);

/**
 * DJB2 Hash Function (Dan Bernstein).  Case insensitive version.
 *
 * This is the default hash function for ib_hash_create_nocase().
 *
 * @sa ib_hashfunc_djb2().
 *
 * @code
 * hash = 5381
 * for c in ckey
 *   hash = hash * 33 + tolower(c)
 * @endcode
 *
 * @param[in] key        The key to hash.
 * @param[in] key_length Length of @a key.
 *
 * @returns Hash value of @a key.
 */
unsigned int DLL_PUBLIC ib_hashfunc_djb2_nocase(
    const void *key,
    size_t      key_length
);

/**
 * Byte for byte equality predicate.
 *
 * This is the default equality predicate for ib_hash_create().
 *
 * @sa ib_hashequal_nocase().
 *
 * @param[in] a        First key.
 * @param[in] a_length Length of @a a.
 * @param[in] b        Second key.
 * @param[in] b_length Length of @a b.
 *
 * @returns 1 if @a a and @a b have same length and same bytes and 0
 * otherwise.
 **/
int DLL_PUBLIC ib_hashequal_default(
    const void *a,
    size_t a_length,
    const void *b,
    size_t b_length
);

/**
 * Byte for byte equality predicate.
 *
 * This is the default equality predicate for ib_hash_create_nocase().
 *
 * @sa ib_hashequal_default().
 *
 * @param[in] a        First key.
 * @param[in] a_length Length of @a a.
 * @param[in] b        Second key.
 * @param[in] b_length Length of @a b.
 *
 * @returns 1 if @a a and @a b have same length and same bytes and 0
 * otherwise.
 **/
int DLL_PUBLIC ib_hashequal_nocase(
    const void *a,
    size_t a_length,
    const void *b,
    size_t b_length
);

/*@}*/

/**
 * @name Creation
 *
 * Functions to create hashes.
 */
/*@{*/

/**
 * Create a hash table.
 *
 * @sa ib_hash_create()
 *
 * @param[out] hash            The newly created hash table.
 * @param[in]  pool            Memory pool to use.
 * @param[in]  size            The number of slots in the hash table.
 * @param[in]  hash_function   Hash function to use, e.g., ib_hashfunc_djb2().
 * @param[in]  equal_predicate Predicate to use for key equality.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_hash_create_ex(
    ib_hash_t          **hash,
    ib_mpool_t          *pool,
    unsigned int         size,
    ib_hash_function_t   hash_function,
    ib_hash_equal_t      equal_predicate
);

/**
 * Create a hash table with ib_hashfunc_djb2(), ib_hashequal_default(), and a
 * default size.
 *
 * @sa ib_hash_create_ex()
 *
 * @param[out] hash The newly created hash table.
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
 * Create a hash table with ib_hashfunc_djb2_nocase(), ib_hashequal_noacse()
 * and a default size.
 *
 * @sa ib_hash_create_ex()
 *
 * @param[out] hash The newly created hash table.
 * @param[in]  pool Memory pool to use.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
ib_status_t DLL_PUBLIC ib_hash_create_nocase(
    ib_hash_t  **hash,
    ib_mpool_t  *pool
);

/*@}*/

/**
 * @name Accessors
 *
 * Functions to access hash properties.
 */
/*@{*/

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
  * Number of elements in @a hash.
  *
  * @param[in] hash Hash table.
  *
  * @returns Number of elements in @a hash.
  **/
size_t DLL_PUBLIC ib_hash_size(
    ib_hash_t* hash
);

/*@}*/

/**
 * @name Non-mutating
 *
 * These functions do not alter the hash.
 */
/*@{*/

/**
 * Fetch value from @a hash for key @a key.
 *
 * @sa ib_hash_get()
 * @sa ib_hash_get_nocase()
 *
 * @param[out] value      Address which value is written.
 * @param[in]  hash       Hash table.
 * @param[in]  key        Key to lookup.
 * @param[in]  key_length Length of @a key.
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a key is not in hash table.
 * - IB_EINVAL if any parameters are invalid.
 */
ib_status_t DLL_PUBLIC ib_hash_get_ex(
    void             *value,
    const ib_hash_t  *hash,
    void             *key,
    size_t            key_length
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
    void              *value,
    const ib_hash_t   *hash,
    const char        *key
);

/**
 * Push every entry from @a hash onto @a list.
 *
 * Order is undefined.  The values pushed to the list are the entry values
 * (@c void @c *).
 *
 * @param[in,out] list List to push values.
 * @param[in]     hash Hash table to take values from.
 *
 * @returns
 * - IB_OK if any elements are pushed.
 * - IB_ENOENT if @a hash is empty.
 */
ib_status_t DLL_PUBLIC ib_hash_get_all(
    ib_list_t       *list,
    const ib_hash_t *hash
);

/*@}*/

/**
 * @name Mutating
 *
 * These functions alter the hash.
 */
/*@{*/

/**
 * Set value of @a key in @a hash to @a data.
 *
 * @sa ib_hash_set()
 *
 * @param[in,out] hash       Hash table.
 * @param[in]     key        Key.
 * @param[in]     key_length Length of @a key
 * @param[in]     value      Value.
 *
 * If @a value is NULL, removes element.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC if @a hash attempted to grow and failed.
 */
ib_status_t DLL_PUBLIC ib_hash_set_ex(
    ib_hash_t  *hash,
    const void *key,
    size_t      key_length,
    void       *value
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
 * If @a value is NULL, removes element.
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
 * Remove value for @a key from @a data.
 *
 * @sa ib_hash_remove()
 *
 * @param[in,out] value      If non-NULL, removed value will be stored here.
 * @param[in,out] hash       Hash table.
 * @param[in]     key        Key.
 * @param[in]     key_length Length of @a key.
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a key is not in hash table.
 * - IB_EINVAL if any parameters are invalid.
 */
ib_status_t DLL_PUBLIC ib_hash_remove_ex(
    void       *value,
    ib_hash_t  *hash,
    void       *key,
    size_t      key_length
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
    void        *value,
    ib_hash_t   *hash,
    const char  *key
);

/*@}*/

/** @} IronBeeUtilHash */

#ifdef __cplusplus
}
#endif

#endif /* _IB_HASH_H_ */
