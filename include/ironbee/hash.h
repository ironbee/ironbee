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
 * @defgroup IronBeeHash Hashtable
 * @ingroup IronBeeUtil
 *
 * @{
 */

typedef unsigned int (*ib_hashfunc_t)(const void *key, size_t len,
                                      uint8_t flags);

/* Options */
#define IB_HASH_FLAG_NOCASE    0x01 /**< Ignore case lookup */

/**
 * DJB2 Hash Function (Dan Bernstein).
 *
 * This is the default hash function.
 *
 * @param ckey buffer holding the key to hash
 * @param len size of the key to hash in bytes
 * @param flags bit flag options for the key
 *              (currently IB_HASH_FLAG_NOCASE)
 *
 * @returns Status code
 */
unsigned int DLL_PUBLIC ib_hashfunc_djb2(const void *ckey,
                                         size_t len,
                                         uint8_t flags);

/**
 * Create a hash table.
 *
 * @param ht Address which new hash table is written
 * @param pool Memory pool to use
 * @param slots Number of slots
 * @param flags Flags to apply to every lookup.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_create_ex(ib_hash_t **ht,
                                         ib_mpool_t *pool,
                                         int slots,
                                         uint8_t flags);

/**
 * Create a hash table with nocase option by default.
 *
 * If you do not need key case insensitivity, use ib_hash_create_ex()
 *
 * @param ph Address which new hash table is written
 * @param pool Memory pool to use
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_create(ib_hash_t **ph, ib_mpool_t *pool);

/**
 * Access hash mpool.
 *
 * @param[in] ht Hash table.
 * @return Memory pool of \a ht.
 **/
ib_mpool_t DLL_PUBLIC *ib_hash_mpool(ib_hash_t *ht);

/**
 * Clear a hash table.
 *
 * @param h Hash table
 */
void DLL_PUBLIC ib_hash_clear(ib_hash_t *h);

/**
 * Get data from a hash table via key and key length.
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param len Length of key
 * @param pdata Address which data is written
 * @param lookup_flags Flags to use during lookup, e.g., (IB_HASH_FLAG_NOCASE)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_get_ex(ib_hash_t *h,
                                      void *key, size_t len,
                                      void *pdata,
                                      uint8_t lookup_flags);

/**
 * Get data from a hash table via NUL terminated key.
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param pdata Address which data is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_get(ib_hash_t *h,
                                   const char *key,
                                   void *pdata);

/**
 * Get data from a hash table via NUL terminated key with ignore
 * case option set.
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param pdata Address which data is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_get_nocase(ib_hash_t *h,
                                   const char *key,
                                   void *pdata);

/**
 * Get all data from a hash table and push onto the supplied list.
 *
 * @param h Hash table
 * @param list List to push values
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_get_all(ib_hash_t *h, ib_list_t *list);

/**
 * Set data in a hash table for key and key length.
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param len Length of key
 * @param data Data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_set_ex(ib_hash_t *h,
                                      const void *key,
                                      size_t len,
                                      const void *data);

/**
 * Set data in a hash table via key (string).
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param data Data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_set(ib_hash_t *h,
                                   const char *key,
                                   void *data);

/**
 * Remove data from a hash table via key and key length.
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param len Length of key
 * @param pdata Address which data is written (or NULL if not required)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_remove_ex(ib_hash_t *h,
                                         void *key,
                                         size_t len,
                                         void *pdata);

/**
 * Remove data from a hash table via key (string).
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param pdata Address which data is written (or NULL if not required)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_remove(ib_hash_t *h,
                                      const char *key,
                                      void *pdata);


/** @} IronBeeUtilHash */


#ifdef __cplusplus
}
#endif

#endif /* _IB_HASH_H_ */
