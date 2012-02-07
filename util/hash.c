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
 * @brief IronBee - Hash Utility Functions Implementation
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/hash.h>

#include "ironbee_config_auto.h"

#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <ironbee/types.h>
#include <ironbee/debug.h>
#include <ironbee/mpool.h>


/* Internal Declarations */

/**
 * @defgroup IronBeeHashInternal Hash Internal
 * @ingroup IronBeeHash
 * @internal
 *
 * @{
 */

/**
 * Default size to use for ib_hash_create().
 * @internal
 **/
#define IB_HASH_INITIAL_SIZE   15

typedef struct ib_hash_entry_t ib_hash_entry_t;
typedef struct ib_hash_iter_t ib_hash_iter_t;

/**
 * Entry in a ib_hash_t.
 * @internal
 **/
struct ib_hash_entry_t {
    /** Key. */
    const void          *key;
    /** Length of @c key. */
    size_t               len;
    /** Value. */
    const void          *value;
    /** Hash of @c key. */
    unsigned int         hash;
    /** Next entry in slot for @c hash. */
    ib_hash_entry_t     *next;
};

/**
 * External iterator for ib_hash_t.
 * @internal
 **/
struct ib_hash_iter_t {
    /** Hash table we are iterating through. */
    ib_hash_t           *cur_ht;
    /** Current entry. */
    ib_hash_entry_t     *cur_entry;
    /** Next entry. */
    ib_hash_entry_t     *next;
    /** Index in sequence. */
    unsigned int         index;
};

/**
 * See ib_hash_t()
 **/
struct ib_hash_t {
    /** Flags to pass to @c hash_fn. */
    uint8_t              flags;
    /** Hash function. */
    ib_hashfunc_t        hash_fn;
    /** 
     * Slots.
     *  
     * Each slot holds a (possibly empty) linked list of ib_hash_entry_t's, 
     * all of which have the same hash value.
     **/
    ib_hash_entry_t    **slots;
    /** Size of @c slots. */
    unsigned int         size;
    /** @todo Remove. */
    ib_hash_iter_t       iterator;
    /** Memory pool. */
    ib_mpool_t          *pool;        
    /** Linked list of removed hash entries to use for recycling. */
    ib_hash_entry_t     *free;
    /** Number of entries. */
    unsigned int         count;
};


/**
 * @internal
 * Search an entry for the given key and key length
 * The hash used to search the key will be also returned via param
 *
 * @param ib_ht the hash table to search in
 * @param key buffer holding the key
 * @param len number of bytes key length
 * @param hte pointer reference used to store the entry if found
 * @param hash reference to store the calculated hash
 * @param lookup_flags Flags to use during lookup, e.g., (IB_HASH_FLAG_NOCASE)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_hash_find_entry(ib_hash_t *ib_ht,
                                          const void *key,
                                          size_t len,
                                          ib_hash_entry_t **hte,
                                          unsigned int *hash,
                                          uint8_t lookup_flags);


/**
 * @internal
 * Creates an initialized iterator for the hash table entries.
 *
 * @param p Memory pool for the iterator allocation
 * @param ib_ht hash table to iterate
 *
 * @returns Status code
 */
ib_hash_iter_t DLL_PUBLIC *ib_hash_first(ib_mpool_t *p,
                                         ib_hash_t *ib_ht);

/**
 * @internal
 * Move the iterator to the next entry.
 *
 * @param hti hash table iterator
 *
 * @returns Status code
 */
ib_hash_iter_t DLL_PUBLIC *ib_hash_next(ib_hash_iter_t *hti);

/** @} IronBeeUtilHash */

/* End Internal Declarations */

unsigned int ib_hashfunc_djb2(const void *key,
                              size_t len,
                              uint8_t flags)
{
    IB_FTRACE_INIT();
    unsigned int hash = 0;
    const unsigned char *ckey = (const unsigned char *)key;
    size_t i = 0;

    /* This is stored at ib_hash_t flags, however,
     * the lookup function should take care to compare
     * the real keys with or without tolower (so there will
     * be a collision of hashes but the keys still different).
     * See lookup_flags to choose with or without case sensitive */
    if ( (flags & IB_HASH_FLAG_NOCASE)) {
        for (i = 0; i < len ; i++) {
            hash = (hash << 5) + tolower(ckey[i]);
        }
    }
    else {
        for (i = 0; i < len; i++) {
            hash = (hash << 5) + ckey[i];
        }
    }

    IB_FTRACE_RET_UINT(hash);
}

ib_status_t ib_hash_create_ex(ib_hash_t **hp,
                              ib_mpool_t *pool,
                              unsigned int size,
                              ib_hashfunc_t   hash_fn,
                              uint8_t flags)
{
    IB_FTRACE_INIT();
    ib_hash_t *ib_ht = NULL;

    if (hp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_ht = (ib_hash_t *)ib_mpool_calloc(pool, 1, sizeof(ib_hash_t));
    if (ib_ht == NULL) {
        *hp = NULL;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    ib_ht->size = size;
    ib_ht->slots = (ib_hash_entry_t **)ib_mpool_calloc(pool, ib_ht->size + 1,
                                sizeof(ib_hash_entry_t *));
    if (ib_ht->slots == NULL) {
        *hp = NULL;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    ib_ht->hash_fn = hash_fn;
    ib_ht->count = 0;
    ib_ht->flags = flags;
    ib_ht->free = NULL;
    ib_ht->pool = pool;

    *hp = ib_ht;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_hash_create(
    ib_hash_t **ph,
    ib_mpool_t *pool
)
{
    return ib_hash_create_ex(
        ph,
        pool,
        IB_HASH_INITIAL_SIZE,
        ib_hashfunc_djb2,
        IB_HASH_FLAG_NOCASE
    );
}

ib_mpool_t DLL_PUBLIC *ib_hash_pool(ib_hash_t *hash)
{
    assert(hash != NULL);

    return hash->pool;
}

ib_hash_iter_t *ib_hash_next(ib_hash_iter_t *hti)
{
    IB_FTRACE_INIT();
    hti->cur_entry = hti->next;
    while (!hti->cur_entry) {
        if (hti->index > hti->cur_ht->size) {
            IB_FTRACE_RET_PTR(ib_hash_iter_t, NULL);
        }
        hti->cur_entry = hti->cur_ht->slots[hti->index++];
    }
    hti->next = hti->cur_entry->next;
    IB_FTRACE_RET_PTR(ib_hash_iter_t, hti);
}

ib_hash_iter_t *ib_hash_first(ib_mpool_t *p,
                              ib_hash_t *ib_ht)
{
    IB_FTRACE_INIT();
    ib_hash_iter_t *hti = NULL;

    if (p != NULL) {
        hti = (ib_hash_iter_t *)ib_mpool_calloc(p, 1, sizeof(ib_hash_iter_t));
        if (hti == NULL) {
            IB_FTRACE_RET_PTR(ib_hash_iter_t, NULL);
        }
    }
    else {
        hti = &ib_ht->iterator;
    }

    memset(hti, 0, sizeof(ib_hash_iter_t));
    hti->cur_ht = ib_ht;

    IB_FTRACE_RET_PTR(ib_hash_iter_t, ib_hash_next(hti));
}

/**
 * @internal
 * Search an entry in the given list
 *
 * @returns ib_hash_entry_t
 */
static ib_hash_entry_t *ib_hash_find_htentry(ib_hash_entry_t *hte,
                                              const void *key,
                                              size_t len,
                                              unsigned int hash,
                                              uint8_t flags)
{
    IB_FTRACE_INIT();

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
                        break;
                    }
                }
                if (i == len) {
                    IB_FTRACE_RET_PTR(ib_hash_entry_t, hte);
                }
             }
            else if (memcmp(hte->key, key, len) == 0) {
                IB_FTRACE_RET_PTR(ib_hash_entry_t, hte);
            }
        }
    }
    IB_FTRACE_RET_PTR(ib_hash_entry_t, NULL);
}

/**
 * @internal
 * Search an entry for the given key and key length
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
    IB_FTRACE_INIT();

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
static ib_status_t ib_hash_resize_slots(ib_hash_t *ib_ht)
{
    IB_FTRACE_INIT();
    ib_hash_iter_t *hti = NULL;
    ib_hash_entry_t **new_slots = NULL;
    unsigned int new_max = 0;

    new_max = (ib_ht->size * 2) + 1;
    new_slots = (ib_hash_entry_t **)ib_mpool_calloc(ib_ht->pool, new_max + 1,
                                                    sizeof(ib_hash_entry_t *));
    if (new_slots == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    for (hti = ib_hash_first(NULL, ib_ht);
         hti != NULL;
         hti = ib_hash_next(hti))
    {
        unsigned int i = hti->cur_entry->hash & new_max;
        hti->cur_entry->next = new_slots[i];
        new_slots[i] = hti->cur_entry;
    }
    ib_ht->size = new_max;
    ib_ht->slots = new_slots;
    IB_FTRACE_RET_STATUS(IB_OK);
}

void ib_hash_clear(ib_hash_t *hash)
{
    IB_FTRACE_INIT();
    ib_hash_iter_t *hti = NULL;
    for (hti = ib_hash_first(NULL, hash);
         hti;
         hti = ib_hash_next(hti))
    {
        ib_hash_set_ex(hash, hti->cur_entry->key, hti->cur_entry->len, NULL);
    }
    IB_FTRACE_RET_VOID();
}

ib_status_t ib_hash_get(
    void* value,
    ib_hash_t *hash,
    const char *key
)
{
    IB_FTRACE_INIT();

    if (key == NULL) {
        *(void **)value = NULL;
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(ib_hash_get_ex(value, hash, (void *)key, strlen(key), 0));
}

ib_status_t ib_hash_get_nocase(
    void *value,
    ib_hash_t *hash,
    const char *key
)
{
    IB_FTRACE_INIT();
    if (key == NULL) {
        *(void **)value = NULL;
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(ib_hash_get_ex(value, hash, (void *)key, strlen(key),
                                        IB_HASH_FLAG_NOCASE));
}


ib_status_t ib_hash_get_ex(
    void *value,
    ib_hash_t *ib_ht,
    void *key,
    size_t len,
    uint8_t lookup_flags
)
{
    IB_FTRACE_INIT();
    ib_hash_entry_t *he = NULL;
    ib_status_t rc = IB_EINVAL;
    unsigned int hash = 0;

    if (key == NULL) {
        *(void **)value = NULL;
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_hash_find_entry(ib_ht, key, len, &he, &hash, lookup_flags);
    if (rc == IB_OK) {
        *(void **)value = (void *)he->value;
    }
    else {
        *(void **)value = NULL;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_hash_get_all(
    ib_list_t *list,
    ib_hash_t *hash
)
{
    IB_FTRACE_INIT();
    ib_hash_iter_t *hti = NULL;

    for (hti = ib_hash_first(list->mp, hash);
         hti;
         hti = ib_hash_next(hti))
    {
        ib_list_push(list, &hti->cur_entry->value);
    }

    if (ib_list_elements(list) <= 0) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_hash_set_ex(ib_hash_t *ib_ht,
                           const void *key,
                           size_t len,
                           const void *value)
{
    IB_FTRACE_INIT();
    unsigned int hash = 0;
    uint8_t found = 0;

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
                        break;
                    }
                }
                if (i == len) {
                    /* Found */
                    found = 1;
                    break;
                }
             }
            else if (memcmp(hte->key, key, len) == 0) {
                found = 1;
                break;
            }
        }
    }

    /* it's in the list, update or delete if value == NULL*/
    if (*hte_prev != NULL) {
        if (value != NULL && found) {
            /* Update */
            hte->value = value;
        } else {
            /* Delete */
            hte->value = NULL;
            ib_ht->count--;
            ib_hash_entry_t *entry = *hte_prev;
            *hte_prev = (*hte_prev)->next;
            entry->next = ib_ht->free;
            ib_ht->free = entry;
        }
    }
    else {
        /* it's not in the list. Add it if value != NULL */
        if (value != NULL) {
            ib_hash_entry_t *entry = NULL;

            /* add a new entry for non-NULL values */
            if ((entry = ib_ht->free) != NULL) {
                ib_ht->free = entry->next;
            }
            else {
                entry = (ib_hash_entry_t *)ib_mpool_calloc(ib_ht->pool, 1,
                                                       sizeof(ib_hash_entry_t));
                if (entry == NULL) {
                    IB_FTRACE_RET_STATUS(IB_EALLOC);
                }
            }
            entry->hash = hash;
            entry->key  = key;
            entry->len = len;
            entry->value = value;

            *hte_prev = entry;
            entry->next = NULL;

            ib_ht->count++;

            /* Change this to accept more */
            if (ib_ht->count > ib_ht->size) {
                IB_FTRACE_RET_STATUS(ib_hash_resize_slots(ib_ht));
            }
        }
        /* else, no changes needed */
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_hash_set(
    ib_hash_t *hash,
    const char *key,
    void *value
)
{
    IB_FTRACE_INIT();
    /* Cannot be a NULL value (this means delete). */
    if (value == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    IB_FTRACE_RET_STATUS(ib_hash_set_ex(hash, (void *)key, strlen(key), value));
}

ib_status_t ib_hash_remove_ex(
    void *value,
    ib_hash_t *hash,
    void *key,
    size_t len
)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_ENOENT;
    void *local_value = NULL;

    rc = ib_hash_get_ex(&local_value, hash, key, (size_t)len, hash->flags);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if ((value != NULL) && (local_value != NULL)) {
        *(void **)value = local_value;
    }
    rc = ib_hash_set_ex(hash, key, (size_t)len, NULL);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_hash_remove(
    void *value,
    ib_hash_t *hash,
    const char *key
)
{
    return ib_hash_remove_ex(value, hash, (void*)key, strlen(key));
}
