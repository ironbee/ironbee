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

/**
 * @file
 * @brief IronBee - Utility Hash Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/util.h>

#include "ironbee_util_private.h"

/**
 * Default hash function
 *
 * @param key buffer holding the key to hash
 * @param len size of the key to hash in bytes
 * @param flags bit flag options for the key
 *              (currently IB_HASH_FLAG_NOCASE)
 *
 * @returns Status code
 */
unsigned int ib_hashfunc_default(const void *ckey,
                                 size_t len,
                                 uint8_t flags)
{
    IB_FTRACE_INIT(ib_hashfunc_default);
    unsigned int hash = 0;
    const unsigned char *key = (const unsigned char *)ckey;
    const unsigned char *p = NULL; 
    size_t i = 0;

    /* This stored at ib_hash_t flags, however,
     * the lookup function should take care to compare
     * the real keys with or without tolower (so there will
     * be a collision of hashes but the keys still different).
     * See lookup_flags to choose with or without case sensitive */
    if ( (flags & IB_HASH_FLAG_NOCASE)) {
        for (i = 0; i < len ; i++) {
            hash = hash * 33 + tolower(key[i]);
        }
        len = p - key;
    }
    else {
        for (i = 0; i < len; i++) {
            hash = hash * 33 + key[i];
        }
    }

    IB_FTRACE_RET_UINT(hash);
}

/**
 * Create a hash table with custom options.
 * The size must be always equal to (2^n - 1) Ex: 7, 15, 31, 63 and so on
 * IB_HASH_INITIAL_SIZE is 15 by default
 *
 * @param ht Address which new hash table is written
 * @param pool Memory pool to use
 * @param slots number of initial slots at the array (size of the array)
 * @param flags custom bit flags (IB_HASH_FLAG_STRLEN, IB_HASH_FLAG_NOCASE)
 *
 * @returns Status code
 */
ib_status_t ib_hash_create_ex(ib_hash_t **hp,
                              ib_mpool_t *pool,
                              int size,
                              uint8_t flags)
{
    IB_FTRACE_INIT(ib_hash_create_ex);
    ib_hash_t *ib_ht = NULL;

    if (hp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_ht = (ib_hash_t *)ib_mpool_calloc(pool, 1, sizeof(ib_hash_t));
    if (ib_ht == NULL) {
        *hp = NULL;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    ib_ht->slots = (ib_hash_entry_t **)ib_mpool_calloc(pool, size * 2 + 1,
                                sizeof(ib_hash_entry_t *));
    if (ib_ht->slots == NULL) {
        *hp = NULL;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    ib_ht->hash_fn = ib_hashfunc_default;
    ib_ht->size = size * 2 ;
    ib_ht->cnt = 0;
    ib_ht->flags = flags;
    ib_ht->free = NULL;
    ib_ht->mp = pool;

    *hp = ib_ht;

    IB_FTRACE_RET_STATUS(IB_OK); 
}

/**
 * Create a hash table with nocase option by default.
 * If you dont need it, use ib_hash_create_ex
 *
 * @param ph Address which new hash table is written
 * @param pool Memory pool to use
 *
 * @returns Status code
 */
ib_status_t ib_hash_create(ib_hash_t **hp,
                           ib_mpool_t *pool)
{
    IB_FTRACE_INIT(ib_hash_create);
    IB_FTRACE_RET_STATUS(ib_hash_create_ex(hp, pool, IB_HASH_INITIAL_SIZE,
                             IB_HASH_FLAG_NOCASE));
}

/**
 * move the iterator to the next entry
 *
 * @param hi hash table iterator
 *
 * @returns Status code
 */
ib_hash_iter_t *ib_hash_next(ib_hash_iter_t *hti)
{
    IB_FTRACE_INIT(ib_hash_next);
    hti->cur_entry = hti->next;
    while (!hti->cur_entry) {
        if (hti->index > hti->cur_ht->size) {
            IB_FTRACE_RET_PTR(ib_hash_iter_t *, NULL);
        }
        hti->cur_entry = hti->cur_ht->slots[hti->index++];
    }   
    hti->next = hti->cur_entry->next;
    IB_FTRACE_RET_PTR(ib_hash_iter_t *, hti);
}

/**
 * Creates an initialized iterator for the hash table entries
 *
 * @param mp Memory pool for the iterator allocation
 * @param ib_ht hash table to iterate
 *
 * @returns Status code
 */
ib_hash_iter_t *ib_hash_first(ib_mpool_t *p, ib_hash_t *ib_ht)
{
    IB_FTRACE_INIT(ib_hash_first);
    ib_hash_iter_t *hti = NULL;
    if (p != NULL) {
        hti = (ib_hash_iter_t *)ib_mpool_calloc(p, 1, sizeof(ib_hash_iter_t));
    }
    else {
        hti = &ib_ht->iterator;
    }

    hti->cur_ht = ib_ht; 

    IB_FTRACE_RET_PTR(ib_hash_iter_t *, ib_hash_next(hti));
}

/**
 * @internal
 * Seach an entry in the given list
 *
 * @returns ib_hash_entry_t
 */
static ib_hash_entry_t *ib_hash_find_htentry(ib_hash_entry_t *hte,
                                              const void *key,
                                              size_t len,
                                              unsigned int hash,
                                              uint8_t flags)
{
    IB_FTRACE_INIT(ib_hash_find_htentry);

    for (; hte != NULL; hte = hte->next) {
        if (hte->hash == hash
            && hte->len == len)
        {
            if (flags & IB_HASH_FLAG_NOCASE) 
            {
                size_t i = 0;
                const char *k = (const char *)key;
                for (; i < len; i++) {
                    if (tolower((char)k[i]) != tolower(((char *)hte->key)[i])) {
                        continue;
                    }
                }
                IB_FTRACE_RET_PTR(ib_hash_entry_t *, hte);
             }
            else if (memcmp(hte->key, key, len) == 0) {
                IB_FTRACE_RET_PTR(ib_hash_entry_t *, hte);
            }
        }
    }
    IB_FTRACE_RET_PTR(ib_hash_entry_t *, NULL);
}

/**
 * @internal
 * Seach an entry for the given key and key length
 * The hash used to search the key will be also returned via param
 *
 * @param ib_ht the hash table to search in
 * @param key buffer holding the key
 * @param len number of bytes key length
 * @param hte pointer reference used to store the entry if found
 * @param hash reference to store the calculated hash
 *
 * @returns Status code
 */
ib_status_t ib_hash_find_entry(ib_hash_t *ib_ht,
                               const void *key,
                               size_t len,
                               ib_hash_entry_t **hte,
                               unsigned int *hash,
                               uint8_t lookup_flags)
{
    IB_FTRACE_INIT(ib_hash_find_entry);

    ib_hash_entry_t *slot = NULL;
    ib_hash_entry_t *he = NULL;

    /* Ensure that NOCASE lookups are allowed at ib_hash_t flags */
    if (hte == NULL || hash == NULL ||
        ( (lookup_flags & IB_HASH_FLAG_NOCASE) && 
         !(ib_ht->flags & IB_HASH_FLAG_NOCASE)))
    {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    *hash = ib_ht->hash_fn(key, len, ib_ht->flags);

    slot = ib_ht->slots[*hash & ib_ht->size];
    he = ib_hash_find_htentry(slot, key, len, *hash, lookup_flags);
    if (he == NULL) {
        *hte = NULL;
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }
    *hte = he;
    
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Resize the number of slots holding the entry lists
 *
 * @returns ib_hash_entry_t
 */
static void ib_hash_resize_slots(ib_hash_t *ib_ht)
{
    IB_FTRACE_INIT(ib_hash_resize_slots);
    ib_hash_iter_t *hti = NULL;
    ib_hash_entry_t **new_slots = NULL;
    unsigned int new_max = 0;

    new_max = ib_ht->size * 2;
    new_slots = (ib_hash_entry_t **)ib_mpool_calloc(ib_ht->mp, new_max + 1,
                                                    sizeof(ib_hash_entry_t *));
    for (hti = ib_hash_first(NULL, ib_ht);
         hti != NULL;
         hti = ib_hash_next(hti))
    {
        unsigned int i = hti->cur_entry->hash & new_max;
        hti->cur_entry->next = new_slots[i];
        new_slots[i] = hti->cur_entry;
    }
    ib_ht->slots = new_slots;
    ib_ht->size = new_max;
    IB_FTRACE_RET_VOID();
}

/**
 * Clear a hash table.
 *
 * @param h Hash table
 */
void ib_hash_clear(ib_hash_t *h)
{
    IB_FTRACE_INIT(ib_hash_clear);
    ib_hash_iter_t *hti = NULL;
    for (hti = ib_hash_first(NULL, h);
         hti;
         hti = ib_hash_next(hti))
    {
        ib_hash_set_ex(h, hti->cur_entry->key, hti->cur_entry->len, NULL);
    }
    IB_FTRACE_RET_VOID();
}

/**
 * Get data from a hash table via key
 * (warning! must be NULL terminated string! Use ib_hash_get_ex to
 * specify custom flags)
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param pdata Address which data is written
 *
 * @returns Status code
 */
ib_status_t ib_hash_get(ib_hash_t *h,
                        const char *key,
                        void *pdata)
{
    IB_FTRACE_INIT(ib_hash_get);
    if (key == NULL) {
        *(void **)pdata = NULL;
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(ib_hash_get_ex(h, (void *)key, strlen(key), pdata,
                                        IB_HASH_FLAG_CASE));
}

/**
 * Get data from a hash table via key with ignore case option set
 * (warning! must be NULL terminated string! Use ib_hash_get_ex to
 * specify custom flags)
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param pdata Address which data is written
 *
 * @returns Status code
 */
ib_status_t ib_hash_get_nocase(ib_hash_t *h,
                               const char *key,
                               void *pdata)
{
    IB_FTRACE_INIT(ib_hash_get);
    if (key == NULL) {
        *(void **)pdata = NULL;
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(ib_hash_get_ex(h, (void *)key, strlen(key), pdata,
                                        IB_HASH_FLAG_NOCASE));
}


/**
 * Get data from a hash table via key and key length.
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param len Length of key
 * @param pdata Address which data is written
 *
 * @returns Status code
 */
ib_status_t ib_hash_get_ex(ib_hash_t *ib_ht,
                           void *key, size_t len,
                           void *pdata,
                           uint8_t lookup_flags)
{
    IB_FTRACE_INIT(ib_hash_get_ex);
    ib_hash_entry_t *he = NULL;
    ib_status_t rc = IB_EINVAL;
    unsigned int hash = 0;

    if (key == NULL) {
        *(void **)pdata = NULL;
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_hash_find_entry(ib_ht, key, len, &he, &hash, lookup_flags);
    if (rc == IB_OK) {
        *(void **)pdata = (void *)he->data;
    }
    else {
        *(void **)pdata = NULL;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Get all data from a hash table and push onto the supplied list.
 *
 * @param h Hash table
 * @param list List to push values
 *
 * @returns Status code
 */
ib_status_t ib_hash_get_all(ib_hash_t *h, ib_list_t *list)
{
    IB_FTRACE_INIT(ib_hash_get_all);
    ib_hash_iter_t *hti = NULL;

    for (hti = ib_hash_first(list->mp, h);
         hti;
         hti = ib_hash_next(hti))
    {
        ib_list_push(list, &hti->cur_entry->data);
    }

    if (ib_list_elements(list) <= 0) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

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
ib_status_t ib_hash_set_ex(ib_hash_t *ib_ht,
                           const void *key,
                           size_t len,
                           const void *pdata)
{
    IB_FTRACE_INIT(ib_hash_set_ex);
    unsigned int hash = 0;

    ib_hash_entry_t *hte = NULL;
    ib_hash_entry_t **hte_prev = NULL;

    hash = ib_ht->hash_fn(key, len, ib_ht->flags);

    hte_prev = &ib_ht->slots[hash & ib_ht->size];
    hte = *hte_prev;
    
    for (; *hte_prev != NULL; hte_prev = &hte->next) {
        hte = *hte_prev;
        if (hte->hash == hash
            && hte->len == len)
        {
            if (ib_ht->flags & IB_HASH_FLAG_NOCASE) 
            {
                size_t i = 0;
                const char *k = (const char *)key;
                for (; i < len; i++) {
                    if (tolower((char)k[i]) != tolower(((char *)hte->key)[i])) {
                        continue;
                    }
                }
                break;
             }
            else if (memcmp(hte->key, key, len) == 0) {
                break;
            }
        }
    }

    /* it's in the list, update or delete if pdata == NULL*/
    if (*hte_prev != NULL) {
        if (pdata != NULL) {
            /* Update */
            hte->data = pdata;
        } else {
            /* Delete */
            hte->data = NULL;
            ib_ht->cnt--;
            ib_hash_entry_t *entry = *hte_prev;
            *hte_prev = (*hte_prev)->next;
            entry->next = ib_ht->free;
            ib_ht->free = entry;
        }
    }
    else {
        /* it's not in the list. Add it if pdata != NULL */
        if (pdata != NULL) {
            ib_hash_entry_t *entry = NULL;
        
            /* add a new entry for non-NULL datas */
            if ((entry = ib_ht->free) != NULL) {
                ib_ht->free = entry->next;
            }
            else {
                entry = (ib_hash_entry_t *)ib_mpool_calloc(ib_ht->mp, 1,
                                                       sizeof(ib_hash_entry_t));
                if (entry == NULL) {
                    IB_FTRACE_RET_STATUS(IB_EALLOC);
                }
            }
            entry->hash = hash;
            entry->key  = key;
            entry->len = len;
            entry->data = pdata;
        
            *hte_prev = entry;
            entry->next = NULL;

            ib_ht->cnt++;

            /* Change this to accept more */
            if (ib_ht->cnt > ib_ht->size) {
                ib_hash_resize_slots(ib_ht);
            }
        }
        /* else, no changes needed */
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Set data in a hash table via key (string).
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param data Data
 *
 * @returns Status code
 */
ib_status_t ib_hash_set(ib_hash_t *h,
                        const char *key,
                        void *data)
{
    IB_FTRACE_INIT(ib_hash_set);
    /* Cannot be a NULL value (this means delete). */
    if (data == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    IB_FTRACE_RET_STATUS(ib_hash_set_ex(h, (void *)key, strlen(key), data));
}

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
ib_status_t ib_hash_remove_ex(ib_hash_t *h,
                              void *key, size_t len,
                              void *pdata)
{
    IB_FTRACE_INIT(ib_hash_remove_ex);
    ib_status_t rc = IB_ENOENT;
    void *data = NULL;

    rc = ib_hash_get_ex(h, key, (size_t)len, &data, h->flags);
    if (rc == IB_ENOENT) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (data != NULL) {
        *(void **)pdata = data;
    }
    rc = ib_hash_set_ex(h, key, (size_t)len, NULL);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Remove data from a hash table via key (string).
 *
 * @param h Hash table
 * @param key Key to lookup
 * @param pdata Address which data is written (or NULL if not required)
 *
 * @returns Status code
 */
ib_status_t ib_hash_remove(ib_hash_t *h,
                           const char *key,
                           void *pdata)
{
    IB_FTRACE_INIT(ib_hash_remove);
    IB_FTRACE_RET_STATUS(ib_hash_remove_ex(h, (void *)key, strlen(key), pdata));
}



