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

#ifndef _IB_HASH_H_
#define _IB_HASH_H_

/**
 * @file
 * @brief IronBee --- Hash Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/list.h>
#include <ironbee/mpool.h>
#include <ironbee/types.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeHash Hash
 * @ingroup IronBeeUtil
 *
 * Hash based map of null-string to void*.
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
 * Hash iterator.
 *
 * An external iterator for hashes.
 *
 * @sa ib_hash_iterator()
 * @sa ib_hash_fetch()
 * @sa ib_hash_first()
 * @sa ib_hash_at_end()
 * @sa ib_hash_next()
 **/
typedef struct ib_hash_iterator_t ib_hash_iterator_t;

/**
 * Function pointer for a hash function.
 *
 * A hash function converts keys (byte sequences) into hash values (unsigned
 * integers).  A good hash function is vital to the performance of a hash.
 * The @a randomizer parameter is provided to the hash function to allow the
 * hash function to vary from hash to hash and thus avoid collision attacks.
 * The @a randomizer parameter will always be the same for a given hash.
 *
 * @param[in] key        Key to hash.
 * @param[in] key_length Length of @a key.
 * @param[in] randomizer Value to randomize hash function.
 * @param[in] cbdata     Callback data.
 *
 * @returns Hash value of @a key.
 **/
typedef uint32_t (*ib_hash_function_t)(
    const char *key,
    size_t      key_length,
    uint32_t    randomizer,
    void       *cbdata
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
 * @param[in] cbdata     Callback data.
 *
 * @returns 1 if @a a and @a b are to be considered equal and 0 otherwise.
 **/
typedef int (*ib_hash_equal_t)(
    const char *a,
    size_t      a_length,
    const char *b,
    size_t      b_length,
    void       *cbdata
);

/**
 * @name Hash functions and equality predicates.
 * Functions suitable for use as ib_hash_function_t and ib_hash_equal_t.
 *
 * @sa ib_hash_create_ex()
 */
/*@{*/

/**
 * DJB2 Hash Function (Dan Bernstein) plus randomizer.
 *
 * This is the default hash function for ib_hash_create().
 *
 * @sa ib_hashfunc_djb2_nocase().
 *
 * @code
 * hash = randomizer
 * for c in ckey
 *   hash = hash * 33 + c
 * @endcode
 *
 * @param[in] key        The key to hash.
 * @param[in] key_length Length of @a key.
 * @param[in] randomizer Value to randomize hash function.
 * @param[in] cbdata     Callback data; unused.
 *
 * @returns Hash value of @a key.
 */
uint32_t DLL_PUBLIC ib_hashfunc_djb2(
    const char *key,
    size_t      key_length,
    uint32_t    randomizer,
    void       *cbdata
)
NONNULL_ATTRIBUTE(1);

/**
 * DJB2 Hash Function (Dan Bernstein) plus randomizer.  Case insensitive
 * version.
 *
 * This is the default hash function for ib_hash_create_nocase().
 *
 * @sa ib_hashfunc_djb2().
 *
 * @code
 * hash = randomizer
 * for c in ckey
 *   hash = hash * 33 + tolower(c)
 * @endcode
 *
 * @param[in] key        The key to hash.
 * @param[in] key_length Length of @a key.
 * @param[in] randomizer Value to randomize hash function.
 * @param[in] cbdata     Callback data; unused.
 *
 * @returns Hash value of @a key.
 */
uint32_t DLL_PUBLIC ib_hashfunc_djb2_nocase(
    const char *key,
    size_t      key_length,
    uint32_t    randomizer,
    void       *cbdata
)
NONNULL_ATTRIBUTE(1);

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
 * @param[in] cbdata     Callback data; unused.
 *
 * @returns 1 if @a a and @a b have same length and same bytes and 0
 * otherwise.
 **/
int DLL_PUBLIC ib_hashequal_default(
    const char *a,
    size_t      a_length,
    const char *b,
    size_t      b_length,
    void       *cbdata
)
NONNULL_ATTRIBUTE(1, 3);

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
 * @param[in] cbdata     Callback data; unused.
 *
 * @returns 1 if @a a and @a b have same length and same bytes and 0
 * otherwise.
 **/
int DLL_PUBLIC ib_hashequal_nocase(
    const char *a,
    size_t      a_length,
    const char *b,
    size_t      b_length,
    void       *cbdata
)
NONNULL_ATTRIBUTE(1, 3);

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
 *                             Must be a power of 2.
 * @param[in]  hash_function   Hash function to use, e.g., ib_hashfunc_djb2().
 * @param[in]  hash_cbdata     Callback data for @a hash_function.
 * @param[in]  equal_predicate Predicate to use for key equality.
 * @param[in]  equal_cbdata    Callback data for @a equal_predicate.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 * - IB_EINVAL if @a size is not a power of 2, or pointers are NULL.
 */
ib_status_t DLL_PUBLIC ib_hash_create_ex(
    ib_hash_t          **hash,
    ib_mpool_t          *pool,
    size_t               size,
    ib_hash_function_t   hash_function,
    void                *hash_cbdata,
    ib_hash_equal_t      equal_predicate,
    void                *equal_cbdata
)
NONNULL_ATTRIBUTE(1, 2);

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
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Create a hash table with ib_hashfunc_djb2_nocase(), ib_hashequal_nocase()
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
)
NONNULL_ATTRIBUTE(1, 2);

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
    const ib_hash_t *hash
)
NONNULL_ATTRIBUTE(1);

 /**
  * Number of elements in @a hash.
  *
  * @param[in] hash Hash table.
  *
  * @returns Number of elements in @a hash.
  **/
size_t DLL_PUBLIC ib_hash_size(
    const ib_hash_t* hash
)
NONNULL_ATTRIBUTE(1);

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
 *
 * @param[in]  hash       Hash table.
 * @param[out] value      Address which value is written.  May be NULL.
 * @param[in]  key        Key to lookup.
 * @param[in]  key_length Length of @a key.
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a key is not in hash table.
 * - IB_EINVAL if any parameters are invalid.
 */
ib_status_t DLL_PUBLIC ib_hash_get_ex(
    const ib_hash_t  *hash,
    void             *value,
    const char       *key,
    size_t            key_length
)
NONNULL_ATTRIBUTE(1, 3);

/**
 * Get value for @a key (NULL terminated char string) from @a hash.
 *
 * @sa ib_hash_get_ex()
 *
 * @param[in]  hash  Hash table.
 * @param[out] value Address which value is written.  May be NULL.
 * @param[in]  key   Key to lookup
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a key is not in hash table.
 * - IB_EINVAL if any parameters are invalid.
 */
ib_status_t DLL_PUBLIC ib_hash_get(
    const ib_hash_t   *hash,
    void              *value,
    const char        *key
)
NONNULL_ATTRIBUTE(1, 3);

/**
 * Push every entry from @a hash onto @a list.
 *
 * Order is undefined.  The values pushed to the list are the entry values
 * (@c void @c *).
 *
 * @param[in]     hash Hash table to take values from.
 * @param[in,out] list List to push values.
 *
 * @returns
 * - IB_OK if any elements are pushed.
 * - IB_ENOENT if @a hash is empty.
 */
ib_status_t DLL_PUBLIC ib_hash_get_all(
    const ib_hash_t *hash,
    ib_list_t       *list
)
NONNULL_ATTRIBUTE(1, 2);

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
    const char *key,
    size_t      key_length,
    void       *value
)
NONNULL_ATTRIBUTE(1, 2);

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
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Clear hash table @a hash.
 *
 * Removes all entries from @a hash.
 *
 * @param[in,out] hash Hash table to clear.
 */
void DLL_PUBLIC ib_hash_clear(
     ib_hash_t *hash
)
NONNULL_ATTRIBUTE(1);

/**
 * Remove value for @a key from @a data.
 *
 * @sa ib_hash_remove()
 *
 * @param[in,out] hash       Hash table.
 * @param[in,out] value      If non-NULL, removed value will be stored here.
 * @param[in]     key        Key.
 * @param[in]     key_length Length of @a key.
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a key is not in hash table.
 * - IB_EINVAL if any parameters are invalid.
 */
ib_status_t DLL_PUBLIC ib_hash_remove_ex(
    ib_hash_t  *hash,
    void       *value,
    const char *key,
    size_t      key_length
)
NONNULL_ATTRIBUTE(1, 3);

/**
 * Remove value for @a key (NULL terminated char string) from @a data.
 *
 * @sa ib_hash_remove_ex()
 *
 * @param[in,out] hash  Hash table.
 * @param[in,out] value If non-NULL, removed value will be stored here.
 * @param[in]     key   Key.
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a key is not in hash table.
 * - IB_EINVAL if any parameters are invalid.
 */
ib_status_t DLL_PUBLIC ib_hash_remove(
    ib_hash_t   *hash,
    void        *value,
    const char  *key
)
NONNULL_ATTRIBUTE(1, 3);

/*@}*/


/**
 * @name Iterators
 *
 * These functions relate to hash iterators.
 */
/*@{*/

/**
 * Create a hash iterator.
 *
 * @warning Return iterator is singular and all behavior is undefined except
 *          for calling ib_hash_iterator_first().
 *
 * @param[in] mp Memory pool to use.
 * @return New iterator or NULL on allocation error.
 **/
ib_hash_iterator_t DLL_PUBLIC *ib_hash_iterator_create(
    ib_mpool_t *mp
)
NONNULL_ATTRIBUTE(1);

/**
 * Create a hash iterator with malloc.
 *
 * Caller is responsible for freeing.
 *
 * @return New iterator or NULL on allocation error.
 **/
ib_hash_iterator_t DLL_PUBLIC *ib_hash_iterator_create_malloc();

/**
 * Is iterator at end of hash.
 *
 * @warning Behavior is undefined for singular iterators.
 *
 * @param[in] iterator Iterator to check.
 * @return true iff iterator is at end of hash.
 **/
bool DLL_PUBLIC ib_hash_iterator_at_end(
    const ib_hash_iterator_t *iterator
)
NONNULL_ATTRIBUTE(1);

/**
 * Fetch value of hash.
 *
 * @warning Behavior is undefined for singular iterators or iterators at
 *          end of hash.
 *
 * Any out variable may be NULL.
 *
 * @param[out] key        Key.
 * @param[out] key_length Length of @a key.
 * @param[out] value      Value.
 * @param[in]  iterator   Iterator to fetch value of.
 **/
void DLL_PUBLIC ib_hash_iterator_fetch(
    const char               **key,
    size_t                    *key_length,
    void                      *value,
    const ib_hash_iterator_t  *iterator
)
NONNULL_ATTRIBUTE(4);

/**
 * Return iterator pointing to first entry of @a hash.
 *
 * @param[out] iterator Iterator to set.
 * @param[in]  hash     Hash table to iterate over.
 */
void DLL_PUBLIC ib_hash_iterator_first(
    ib_hash_iterator_t *iterator,
    const ib_hash_t    *hash
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Move @a iterator to the next entry.
 *
 * @warning Behavior is undefined for singular iterators or iterators at
 *          end of hash.
 *
 * @param[in,out] iterator Iterator to advance.
 */
void DLL_PUBLIC ib_hash_iterator_next(
    ib_hash_iterator_t *iterator
)
NONNULL_ATTRIBUTE(1);

/**
 * Copy @a from iterator to @a to iterator.
 *
 * @param[in] to   Iterator to copy to.
 * @param[in] from Iterator to copy from.
 */
void DLL_PUBLIC ib_hash_iterator_copy(
    ib_hash_iterator_t       *to,
    const ib_hash_iterator_t *from
)
NONNULL_ATTRIBUTE(1, 2);

/**
 * Compare two iterators.
 *
 * @param[in] a First iterator.
 * @param[in] b Second iterator.
 * @return true iff @a a and @a b refer to the same hash entry.
 **/
bool DLL_PUBLIC ib_hash_iterator_equal(
    const ib_hash_iterator_t *a,
    const ib_hash_iterator_t *b
)
NONNULL_ATTRIBUTE(1, 2);

/*@}*/

/** @} IronBeeUtilHash */

#ifdef __cplusplus
}
#endif

#endif /* _IB_HASH_H_ */
