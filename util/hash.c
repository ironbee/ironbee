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
#define IB_HASH_INITIAL_SIZE 15

typedef struct ib_hash_entry_t    ib_hash_entry_t;
typedef struct ib_hash_iterator_t ib_hash_iterator_t;

/**
 * Entry in a ib_hash_t.
 * @internal
 **/
struct ib_hash_entry_t {
    /** Key. */
    const void          *key;
    /** Length of @c key. */
    size_t               key_length;
    /** Value. */
    const void          *value;
    /** Hash of @c key. */
    unsigned int         hash_value;
    /** Next entry in slot for @c hash. */
    ib_hash_entry_t     *next_entry;
};

/**
 * External iterator for ib_hash_t.
 * @internal
 *
 * The end of the sequence is indicated by \c current_entry being NULL.
 * Any iterator is invalidated by any mutating operation on the hash.
 **/
struct ib_hash_iterator_t {
    /** Hash table we are iterating through. */
    ib_hash_t           *hash;
    /** Current entry. */
    ib_hash_entry_t     *current_entry;
    /** Next entry. */
    ib_hash_entry_t     *next_entry;
    /** Which slot we are in. */
    unsigned int         slot_index;
};

/**
 * See ib_hash_t()
 **/
struct ib_hash_t {
    /** Flags to pass to @c hash_function. */
    uint8_t              flags;
    /** Hash function. */
    ib_hash_function_t   hash_function;
    /**
     * Slots.
     *
     * Each slot holds a (possibly empty) linked list of ib_hash_entry_t's,
     * all of which have the same hash value.
     **/
    ib_hash_entry_t    **slots;
    /** Size of @c slots. */
    unsigned int         size;
    /** Memory pool. */
    ib_mpool_t          *pool;
    /** Linked list of removed hash entries to use for recycling. */
    ib_hash_entry_t     *free;
    /** Number of entries. */
    unsigned int         count;
};


/**
 * Search for an entry in @a hash matching @key.
 * @internal
 *
 * @param[out] hash_entry Hash entry.
 * @param[out] hash_value Hash value of @a key.
 * @param[in]  hash       Hash table.
 * @param[in]  key        Key.
 * @param[in]  key_length Length of @a key.
 * @param[in]  flags      Flags to pass to hash function.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EINVAL on invalid flags.
 * - IB_ENOENT if @a key not found.
 */
static ib_status_t ib_hash_find_entry(
    ib_hash_entry_t **hash_entry,
    unsigned int     *hash_value,
    ib_hash_t        *hash,
    const void       *key,
    size_t            key_length,
    uint8_t           flags
);

/**
 * Search for a hash entry in the list of entries starting at @a first.
 * @internal
 *
 * @param[in] first      Beginning of list to search.
 * @param[in] key        Key to search for.
 * @param[in] key_length Length of @a key.
 * @param[in] hash_value Hash value of @a key.
 * @param[in] flags      Flags.
 *
 * @returns Hash entry if found and NULL otherwise.
 */
static ib_hash_entry_t *ib_hash_find_htentry(
     ib_hash_entry_t *first,
     const void      *key,
     size_t           key_length,
     unsigned int     hash_value,
     uint8_t          flags
);

/**
 * @internal
 * Return iterator pointing to first entry of @a hash.
 *
 * @param[in]  hash Hash table to iterate over.
 *
 * @return Iterator pointing to first entry in @a hash.
 */
static ib_hash_iterator_t ib_hash_first(
     ib_hash_t* hash
);

/**
 * @internal
 * Move \a iterator to the next entry.
 *
 * @param[in,out] iterator Iterator to advance.
 */
static void ib_hash_next(
    ib_hash_iterator_t *iterator
);

#define IB_HASH_LOOP(entry,hash) \
    for ( \
        ib_hash_iterator_t iterator = ib_hash_first(hash); \
        ((entry) = iterator.current_entry) != NULL; \
        ib_hash_next(&iterator) \
    )

/** @} IronBeeUtilHash */

/* End Internal Declarations */

/* Internal Definitions */

static ib_hash_entry_t *ib_hash_find_htentry(
    ib_hash_entry_t *first,
    const void      *key,
    size_t           key_length,
    unsigned int     hash_value,
    uint8_t          flags
)
{
    IB_FTRACE_INIT();

    for (
        ib_hash_entry_t* current_entry = first;
        current_entry != NULL;
        current_entry = current_entry->next_entry
    )
    {
        if (
               current_entry->hash_value == hash_value
            && current_entry->key_length == key_length
        )
        {
            if (flags & IB_HASH_FLAG_NOCASE)
            {
                size_t i = 0;
                for (i = 0; i < key_length; ++i) {
                    char a = tolower(((char*)key)[i]);
                    char b = tolower(((char*)current_entry->key)[i]);
                    if ( a != b ) {
                        break;
                    }
                }
                if (i == key_length) {
                    IB_FTRACE_RET_PTR(ib_hash_entry_t, current_entry);
                }
            }
            else if ( memcmp(current_entry->key, key, key_length) == 0 ) {
                IB_FTRACE_RET_PTR(ib_hash_entry_t, current_entry);
            }
        }
    }
    IB_FTRACE_RET_PTR(ib_hash_entry_t, NULL);
}

ib_status_t ib_hash_find_entry(
     ib_hash_entry_t **hash_entry,
     unsigned int     *hash_value,
     ib_hash_t        *hash,
     const void       *key,
     size_t            key_length,
     uint8_t           flags
)
{
    IB_FTRACE_INIT();

    ib_hash_entry_t *current_slot  = NULL;
    ib_hash_entry_t *current_entry = NULL;

    /* Ensure that NOCASE lookups are allowed at ib_hash_t flags */
    if (hash_entry == NULL || hash == NULL ||
        ( (flags & IB_HASH_FLAG_NOCASE) &&
         !(hash->flags & IB_HASH_FLAG_NOCASE)))
    {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    *hash_value = hash->hash_function(key, key_length, hash->flags);

    slot = hash->slots[*hash_value & hash->size];
    he = ib_hash_find_htentry(slot, key, key_length, *hash_value, flags);
    if (he == NULL) {
        *hash_entry = NULL;
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }
    *hash_entry = he;

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
    ib_hash_entry_t **new_slots = NULL;
    ib_hash_entry_t *current_entry = NULL;
    unsigned int new_max = 0;

    new_max = (ib_ht->size * 2) + 1;
    new_slots = (ib_hash_entry_t **)ib_mpool_calloc(ib_ht->pool, new_max + 1,
                                                    sizeof(ib_hash_entry_t *));
    if (new_slots == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    IB_HASH_LOOP(current_entry, ib_ht) {
        unsigned int i = current_entry->hash_value & new_max;
        current_entry->next_entry = new_slots[i];
        new_slots[i] = current_entry;
    }
    ib_ht->size = new_max;
    ib_ht->slots = new_slots;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* End Internal Definitions */

unsigned int ib_hashfunc_djb2(
    const void *key,
    size_t      key_length,
    uint8_t     flags
)
{
    IB_FTRACE_INIT();

    unsigned int         hash = 0;
    const unsigned char *ckey = (const unsigned char *)key;

    if ( flags & IB_HASH_FLAG_NOCASE ) {
        for (size_t i = 0; i < key_length; ++i) {
            hash = (hash << 5) + tolower(ckey[i]);
        }
    } else {
        for (size_t i = 0; i < key_length; ++i) {
            hash = (hash << 5) + ckey[i];
        }
    }

    IB_FTRACE_RET_UINT(hash);
}

ib_status_t ib_hash_create_ex(ib_hash_t **hp,
                              ib_mpool_t *pool,
                              unsigned int size,
                              ib_hash_function_t   hash_function,
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

    ib_ht->hash_function = hash_function;
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

static ib_hash_iterator_t ib_hash_first(
    ib_hash_t *hash
)
{
    // There is no ftace return macro for custom types.
    ib_hash_iterator_t iterator;

    memset(&iterator, 0, sizeof(ib_hash_iterator_t));
    iterator.hash = hash;
    ib_hash_next(&iterator);

    return iterator;
}

void ib_hash_next(
    ib_hash_iterator_t *iterator
)
{
    IB_FTRACE_INIT();

    iterator->current_entry = iterator->next_entry;
    while (! iterator->current_entry) {
        if (iterator->slot_index > iterator->hash->size) {
            IB_FTRACE_RET_VOID();
        }
        iterator->current_entry = iterator->hash->slots[iterator->slot_index];
        ++iterator->slot_index;
    }
    iterator->next_entry = iterator->current_entry->next_entry;
    IB_FTRACE_RET_VOID();
}

void ib_hash_clear(ib_hash_t *hash)
{
    IB_FTRACE_INIT();

    ib_hash_entry_t *current_entry = NULL;
    IB_HASH_LOOP(current_entry, hash) {
        ib_hash_set_ex(hash, current_entry->key, current_entry->key_length, NULL);
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

    rc = ib_hash_find_entry(&he, &hash, ib_ht, key, len, lookup_flags);
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
    ib_hash_entry_t* current_entry = NULL;

    IB_HASH_LOOP(current_entry, hash) {
        ib_list_push(list, &current_entry->value);
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

    hash = ib_ht->hash_function(key, len, ib_ht->flags);

    hte_prev = &ib_ht->slots[hash & ib_ht->size];
    hte = *hte_prev;

    for (; *hte_prev != NULL; hte_prev = &hte->next_entry) {
        hte = *hte_prev;
        if (hte->hash_value == hash
            && hte->key_length == len)
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
            *hte_prev = (*hte_prev)->next_entry;
            entry->next_entry = ib_ht->free;
            ib_ht->free = entry;
        }
    }
    else {
        /* it's not in the list. Add it if value != NULL */
        if (value != NULL) {
            ib_hash_entry_t *entry = NULL;

            /* add a new entry for non-NULL values */
            if ((entry = ib_ht->free) != NULL) {
                ib_ht->free = entry->next_entry;
            }
            else {
                entry = (ib_hash_entry_t *)ib_mpool_calloc(ib_ht->pool, 1,
                                                       sizeof(ib_hash_entry_t));
                if (entry == NULL) {
                    IB_FTRACE_RET_STATUS(IB_EALLOC);
                }
            }
            entry->hash_value = hash;
            entry->key  = key;
            entry->key_length = len;
            entry->value = value;

            *hte_prev = entry;
            entry->next_entry = NULL;

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
