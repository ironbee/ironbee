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

/**
 * @file
 * @brief IronBee --- Hash Utility Functions Implementation
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/hash.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Internal Declarations */

/**
 * @defgroup IronBeeHashInternal Hash Internal
 * @ingroup IronBeeHash
 *
 * @{
 */

/**
 * Default size to use for ib_hash_create().
 *
 * Must be a power of 2.
 **/
#define IB_HASH_INITIAL_SIZE 16

/**
 * See ib_hash_entry_t()
 */
typedef struct ib_hash_entry_t ib_hash_entry_t;

/**
 * Entry in a ib_hash_t.
 **/
struct ib_hash_entry_t {
    /** Key. */
    const char          *key;
    /** Length of @c key. */
    size_t               key_length;
    /** Value. */
    void                *value;
    /** Hash of @c key. */
    uint32_t             hash_value;
    /** Next entry in slot for @c hash. */
    ib_hash_entry_t     *next_entry;
};

/**
 * External iterator for ib_hash_t.
 *
 * The end of the sequence is indicated by @c current_entry being NULL.
 * Any iterator is invalidated by any mutating operation on the hash.
 **/
struct ib_hash_iterator_t {
    /** Hash table we are iterating through. */
    const ib_hash_t     *hash;
    /** Current entry. */
    ib_hash_entry_t     *current_entry;
    /** Next entry. */
    ib_hash_entry_t     *next_entry;
    /** Which slot to look in next. */
    size_t               slot_index;
};

/**
 * See ib_hash_t()
 **/
struct ib_hash_t {
    /** Hash function. */
    ib_hash_function_t   hash_function;
    /** Hash function callback data. */
    void                *hash_cbdata;
    /** Key equality predicate. */
    ib_hash_equal_t      equal_predicate;
    /** Key equality callback data. */
    void                *equal_cbdata;

    /**
     * Slots.
     *
     * Each slot holds a (possibly empty) linked list of ib_hash_entry_t's,
     * all of which have the same hash value.
     **/
    ib_hash_entry_t    **slots;
    /** Maximum slot index. */
    size_t               max_slot;
    /** Memory manager. */
    ib_mm_t              mm;
    /** Linked list of removed hash entries to use for recycling. */
    ib_hash_entry_t     *free;
    /** Number of entries. */
    size_t               size;
    /** Randomizer value. */
    uint32_t             randomizer;
};

/**
 * Search for an entry in @a hash matching @a key.
 *
 * @param[in]  hash       Hash table.
 * @param[out] hash_entry Hash entry.
 * @param[in]  key        Key.
 * @param[in]  key_length Length of @a key.
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a key not found.
 */
static ib_status_t ib_hash_find_entry(
    const ib_hash_t  *hash,
    ib_hash_entry_t **hash_entry,
    const char       *key,
    size_t            key_length
);

/**
 * Search for a hash entry in the list of entries starting at @a first.
 *
 * @param[in] hash       Hash table.
 * @param[in] first      Beginning of list to search.
 * @param[in] key        Key to search for.
 * @param[in] key_length Length of @a key.
 * @param[in] hash_value Hash value of @a key.
 *
 * @returns Hash entry if found and NULL otherwise.
 */
static ib_hash_entry_t *ib_hash_find_htentry(
     const ib_hash_t *hash,
     ib_hash_entry_t *first,
     const char      *key,
     size_t           key_length,
     uint32_t         hash_value
);

/**
 * Set @a entry to every entry in @a hash in sequence.
 *
 * @code
 * ib_hash_iterator_t iterator;
 * IB_HASH_LOOP(iterator, hash) {
 *   ...
 * }
 * @endcode
 *
 * @param[in,out] iterator Iterator to use.
 * @param[in]     hash     Hash table to iterate through.
 **/
#define IB_HASH_LOOP(iterator, hash) \
    for ( \
        ib_hash_iterator_first(&(iterator), (hash)); \
        ! ib_hash_iterator_at_end(&(iterator)); \
        ib_hash_iterator_next(&(iterator)) \
    )

/**
 * Resize the number of slots in @a hash.
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
static ib_status_t ib_hash_resize_slots(
    ib_hash_t *hash
);

/**
 * Fast downcase.
 *
 * @param[in] c Character to downcase.
 * @return Downcased version of @a c.
 */
inline
static char ib_hash_tolower(
    char c
);

/* End Internal Declarations */

/* Internal Definitions */

ib_hash_entry_t *ib_hash_find_htentry(
    const ib_hash_t *hash,
    ib_hash_entry_t *first,
    const char      *key,
    size_t           key_length,
    uint32_t         hash_value
) {
    assert(hash != NULL);
    assert(key  != NULL);

    for (
        ib_hash_entry_t* current_entry = first;
        current_entry != NULL;
        current_entry = current_entry->next_entry
    )
    {
        if (
            current_entry->hash_value == hash_value &&
            hash->equal_predicate(
                key,                key_length,
                current_entry->key, current_entry->key_length,
                hash->equal_cbdata
            )
        ) {
            return current_entry;
        }
    }
    return NULL;
}

ib_status_t ib_hash_find_entry(
     const ib_hash_t  *hash,
     ib_hash_entry_t **hash_entry,
     const char       *key,
     size_t            key_length
) {
    assert(hash_entry != NULL);
    assert(hash       != NULL);
    assert(key        != NULL);

    ib_hash_entry_t *current_slot  = NULL;
    ib_hash_entry_t *current_entry = NULL;
    uint32_t         hash_value    = 0;
    if (hash_entry == NULL || hash == NULL) {
        return IB_EINVAL;
    }

    hash_value = hash->hash_function(
        key, key_length,
        hash->randomizer,
        hash->hash_cbdata
    );

    /* hash->max_slot+1 is a power of 2 */
    current_slot = hash->slots[hash_value & hash->max_slot];
    current_entry = ib_hash_find_htentry(
        hash,
        current_slot,
        key,
        key_length,
        hash_value
    );
    if (current_entry == NULL) {
        *hash_entry = NULL;
        return IB_ENOENT;
    }
    *hash_entry = current_entry;

    return IB_OK;
}

ib_hash_iterator_t *ib_hash_iterator_create(ib_mm_t mm)
{
    return (ib_hash_iterator_t *)ib_mm_alloc(
        mm,
        sizeof(ib_hash_iterator_t)
    );
}

ib_hash_iterator_t *ib_hash_iterator_create_malloc()
{
    return malloc(sizeof(ib_hash_iterator_t));
}

bool ib_hash_iterator_at_end(const ib_hash_iterator_t *iterator)
{
    return iterator->current_entry == NULL;
}

void ib_hash_iterator_first(
    ib_hash_iterator_t *iterator,
    const ib_hash_t    *hash
)
{
    assert(iterator != NULL);
    assert(hash     != NULL);

    memset(iterator, 0, sizeof(*iterator));
    iterator->hash = hash;
    ib_hash_iterator_next(iterator);
}

void ib_hash_iterator_fetch(
    const char               **key,
    size_t                    *key_length,
    void                      *value,
    const ib_hash_iterator_t  *iterator
)
{
    assert(iterator != NULL);

    if (key != NULL) {
        *key            = iterator->current_entry->key;
    }
    if (key_length != NULL) {
        *key_length     = iterator->current_entry->key_length;
    }
    if (value != NULL) {
        *(void **)value = iterator->current_entry->value;
    }
}

void ib_hash_iterator_next(
    ib_hash_iterator_t *iterator
) {
    assert(iterator != NULL);

    iterator->current_entry = iterator->next_entry;
    while (! iterator->current_entry) {
        if (iterator->slot_index > iterator->hash->max_slot) {
            return;
        }
        iterator->current_entry = iterator->hash->slots[iterator->slot_index];
        ++iterator->slot_index;
    }
    iterator->next_entry = iterator->current_entry->next_entry;
}

void ib_hash_iterator_copy(
    ib_hash_iterator_t       *to,
    const ib_hash_iterator_t *from
)
{
    *to = *from;
}

bool ib_hash_iterator_equal(
    const ib_hash_iterator_t *a,
    const ib_hash_iterator_t *b
)
{
    return
        a->hash          == b->hash          &&
        a->current_entry == b->current_entry &&
        a->next_entry    == b->next_entry    &&
        a->slot_index    == b->slot_index
        ;
}

ib_status_t ib_hash_resize_slots(
    ib_hash_t *hash
) {
    assert(hash != NULL);

    ib_hash_entry_t    **new_slots     = NULL;
    size_t               new_max_slot = 0;
    ib_hash_iterator_t   i;

    /* Maintain power of 2 slots */
    new_max_slot = 2 * hash->max_slot + 1;
    new_slots = (ib_hash_entry_t **)ib_mm_calloc(
        hash->mm,
        new_max_slot + 1,
        sizeof(*new_slots)
    );
    if (new_slots == NULL) {
        return IB_EALLOC;
    }

    IB_HASH_LOOP(i, hash) {
        size_t j =
            i.current_entry->hash_value & new_max_slot;
        i.current_entry->next_entry = new_slots[j];
        new_slots[j] = i.current_entry;
    }
    hash->max_slot = new_max_slot;
    hash->slots     = new_slots;

    return IB_OK;
}

inline
static char ib_hash_tolower(
    char c
)
{
    static const char s_table[] = {
        0,   1,   2,   3,   4,   5,   6,   7,
        8,   9,   10,  11,  12,  13,  14,  15,
        16,  17,  18,  19,  20,  21,  22,  23,
        24,  25,  26,  27,  28,  29,  30,  31,
        32,  33,  34,  35,  36,  37,  38,  39,
        40,  41,  42,  43,  44,  45,  46,  47,
        48,  49,  50,  51,  52,  53,  54,  55,
        56,  57,  58,  59,  60,  61,  62,  63,
        64,  97,  98,  99,  100, 101, 102, 103,
        104, 105, 106, 107, 108, 109, 110, 111,
        112, 113, 114, 115, 116, 117, 118, 119,
        120, 121, 122, 91,  92,  93,  94,  95,
        96,  97,  98,  99,  100, 101, 102, 103,
        104, 105, 106, 107, 108, 109, 110, 111,
        112, 113, 114, 115, 116, 117, 118, 119,
        120, 121, 122, 123, 124, 125, 126, 127,
        128, 129, 130, 131, 132, 133, 134, 135,
        136, 137, 138, 139, 140, 141, 142, 143,
        144, 145, 146, 147, 148, 149, 150, 151,
        152, 153, 154, 155, 156, 157, 158, 159,
        160, 161, 162, 163, 164, 165, 166, 167,
        168, 169, 170, 171, 172, 173, 174, 175,
        176, 177, 178, 179, 180, 181, 182, 183,
        184, 185, 186, 187, 188, 189, 190, 191,
        192, 193, 194, 195, 196, 197, 198, 199,
        200, 201, 202, 203, 204, 205, 206, 207,
        208, 209, 210, 211, 212, 213, 214, 215,
        216, 217, 218, 219, 220, 221, 222, 223,
        224, 225, 226, 227, 228, 229, 230, 231,
        232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 242, 243, 244, 245, 246, 247,
        248, 249, 250, 251, 252, 253, 254, 255
    };

    return s_table[(unsigned char)c];
}

/* End Internal Definitions */

uint32_t ib_hashfunc_djb2(
    const char *key,
    size_t      key_length,
    uint32_t    randomizer,
    void       *cbdata
) {
    assert(key != NULL);

    uint32_t      hash  = randomizer;
    const char   *key_s = (const char *)key;

    for (size_t i = 0; i < key_length; ++i) {
        hash = ((hash << 5) + hash) + key_s[i];
    }

    return hash;
}

uint32_t ib_hashfunc_djb2_nocase(
    const char *key,
    size_t      key_length,
    uint32_t    randomizer,
    void       *cbdata
) {
    assert(key != NULL);

    uint32_t             hash  = randomizer;
    const unsigned char *key_s = (const unsigned char *)key;

    for (size_t i = 0; i < key_length; ++i) {
        hash = ((hash << 5) + hash) + ib_hash_tolower(key_s[i]);
    }

    return hash;
}

int ib_hashequal_default(
    const char *a,
    size_t      a_length,
    const char *b,
    size_t      b_length,
    void       *cbdata
) {
    assert(a != NULL);
    assert(b != NULL);

    return (a_length == b_length) && (memcmp(a, b, a_length) == 0);
}

int ib_hashequal_nocase(
    const char *a,
    size_t      a_length,
    const char *b,
    size_t      b_length,
    void       *cbdata
) {
    assert(a != NULL);
    assert(b != NULL);

    const unsigned char *a_s = (const unsigned char *)a;
    const unsigned char *b_s = (const unsigned char *)b;

    if (a_length != b_length) {
        return 0;
    }

    for (size_t i = 0; i < a_length; ++i) {
        if (ib_hash_tolower(a_s[i]) != ib_hash_tolower(b_s[i])) {
            return 0;
        }
    }

    return 1;
}

ib_status_t ib_hash_create_ex(
    ib_hash_t          **hash,
    ib_mm_t              mm,
    size_t               size,
    ib_hash_function_t   hash_function,
    void                *hash_cbdata,
    ib_hash_equal_t      equal_predicate,
    void                *equal_cbdata
) {
    assert(hash != NULL);
    assert(size > 0);

    ib_hash_t *new_hash = NULL;

    if (hash == NULL) {
        return IB_EINVAL;
    }

    {
        int num_ones = 0;
        for (
            size_t temp_size = size;
            temp_size > 0;
            temp_size = temp_size >> 1
        ) {
            if ( ( temp_size & 1 ) == 1 ) {
                ++num_ones;
            }
        }
        if (num_ones != 1) {
            return IB_EINVAL;
        }
    }

    new_hash = (ib_hash_t *)ib_mm_alloc(mm, sizeof(*new_hash));
    if (new_hash == NULL) {
        *hash = NULL;
        return IB_EALLOC;
    }

    ib_hash_entry_t **slots = (ib_hash_entry_t **)ib_mm_calloc(
        mm,
        size + 1,
        sizeof(*slots)
    );
    if (slots == NULL) {
        *hash = NULL;
        return IB_EALLOC;
    }

    new_hash->hash_function   = hash_function;
    new_hash->hash_cbdata     = hash_cbdata;
    new_hash->equal_predicate = equal_predicate;
    new_hash->equal_cbdata    = equal_cbdata;
    new_hash->max_slot        = size-1;
    new_hash->slots           = slots;
    new_hash->mm              = mm;
    new_hash->free            = NULL;
    new_hash->size            = 0;
    new_hash->randomizer      = (uint32_t)clock();

    *hash = new_hash;

    return IB_OK;
}

ib_status_t ib_hash_create(
    ib_hash_t **hash,
    ib_mm_t     mm
) {
    assert(hash != NULL);

    return ib_hash_create_ex(
        hash,
        mm,
        IB_HASH_INITIAL_SIZE,
        ib_hashfunc_djb2, NULL,
        ib_hashequal_default, NULL
    );
}

ib_status_t ib_hash_create_nocase(
    ib_hash_t **hash,
    ib_mm_t     mm
) {
    assert(hash != NULL);

    return ib_hash_create_ex(
        hash,
        mm,
        IB_HASH_INITIAL_SIZE,
        ib_hashfunc_djb2_nocase, NULL,
        ib_hashequal_nocase, NULL
    );
}

ib_mm_t ib_hash_mm(
    const ib_hash_t *hash
) {
    assert(hash != NULL);

    return hash->mm;
}

size_t ib_hash_size(
    const ib_hash_t* hash
) {
    assert(hash != NULL);

    return hash->size;
}

ib_status_t ib_hash_get_ex(
    const ib_hash_t  *hash,
    void             *value,
    const char       *key,
    size_t            key_length
) {
    assert(hash  != NULL);

    ib_status_t      rc;
    ib_hash_entry_t *current_entry = NULL;

    if (key == NULL) {
        *(void **)value = NULL;
        return IB_EINVAL;
    }

    rc = ib_hash_find_entry(
        hash,
        &current_entry,
        key,
        key_length
    );
    if (value != NULL) {
        if (rc == IB_OK) {
            assert(current_entry != NULL);

            *(void **)value = current_entry->value;
        }
        else {
            *(void **)value = NULL;
        }
    }

    return rc;
}

ib_status_t ib_hash_get(
    const ib_hash_t   *hash,
    void              *value,
    const char        *key
) {
    assert(hash  != NULL);

    if (key == NULL) {
        if (value != NULL) {
            *(void **)value = NULL;
        }
        return IB_EINVAL;
    }

    return ib_hash_get_ex(
        hash,
        value,
        key,
        strlen(key)
    );
}

ib_status_t ib_hash_get_all(
    const ib_hash_t *hash,
    ib_list_t       *list
) {
    assert(list != NULL);
    assert(hash != NULL);

    ib_hash_iterator_t i;
    IB_HASH_LOOP(i, hash) {
        ib_list_push(list, i.current_entry->value);
    }

    if (ib_list_elements(list) <= 0) {
        return IB_ENOENT;
    }

    return IB_OK;
}

ib_status_t ib_hash_set_ex(
    ib_hash_t  *hash,
    const char *key,
    size_t      key_length,
    void       *value
) {
    assert(hash != NULL);
    assert(key  != NULL);

    uint32_t     hash_value = 0;
    size_t       slot_index = 0;
    int          found      = 0;

    ib_hash_entry_t  *current_entry         = NULL;
    /* Points to pointer that points to current_entry */
    ib_hash_entry_t **current_entry_handle  = NULL;

    hash_value = hash->hash_function(
        key, key_length,
        hash->randomizer,
        hash->hash_cbdata
    );
    slot_index = (hash_value & hash->max_slot);

    current_entry_handle = &hash->slots[slot_index];

    while (*current_entry_handle != NULL) {
        current_entry = *current_entry_handle;
        if (
            current_entry->hash_value == hash_value &&
            hash->equal_predicate(
               current_entry->key, current_entry->key_length,
               key,                key_length,
               hash->equal_cbdata
            )
        ) {
            found = 1;
            break;
        }
        current_entry_handle = &(current_entry->next_entry);
    }
    /* current_entry is now the entry to change, and current_entry_handler
     * points to the pointer to it.
     */
    if (found) {
        assert(current_entry != NULL);
        assert(current_entry == *current_entry_handle);

        /* Update. */
        current_entry->value = value;

        /* Delete if appropriate. */
        if (value == NULL) {
            /* Delete */
            --hash->size;

            /* Remove from slot list. */
            *current_entry_handle = current_entry->next_entry;

            /* Add to free list. */
            current_entry->next_entry = hash->free;
            hash->free                = current_entry;
        }
    }
    else {
        /* It's not in the list. Add it if value != NULL. */
        if (value != NULL) {
            ib_hash_entry_t *entry = NULL;

            if (hash->free != NULL) {
                entry = hash->free;
                hash->free = entry->next_entry;
            }
            else {
                entry = (ib_hash_entry_t *)ib_mm_alloc(
                    hash->mm,
                    sizeof(*entry)
                );
                if (entry == NULL) {
                    return IB_EALLOC;
                }
            }

            entry->hash_value = hash_value;
            entry->key        = key;
            entry->key_length = key_length;
            entry->value      = value;
            entry->next_entry = hash->slots[slot_index];

            hash->slots[slot_index] = entry;

            ++hash->size;

            /* If we have more elements that slots, resize. */
            if (hash->size > hash->max_slot+1) {
                return ib_hash_resize_slots(hash);
            }
        }
        /* Else value == NULL and no changes are needed. */
    }

    return IB_OK;
}

ib_status_t ib_hash_set(
    ib_hash_t  *hash,
    const char *key,
    void       *value
) {
    assert(hash != NULL);
    assert(key  != NULL);

    return ib_hash_set_ex(
        hash,
        (void *)key,
        strlen(key),
        value
    );
}

void ib_hash_clear(ib_hash_t *hash) {
    assert(hash != NULL);

    for (size_t i = 0; i <= hash->max_slot; ++i) {
        if (hash->slots[i] != NULL) {
            ib_hash_entry_t *current_entry;
            for (
                current_entry = hash->slots[i];
                current_entry->next_entry != NULL;
                current_entry = current_entry->next_entry
            ) {
                current_entry->value = NULL;
            }

            current_entry->next_entry = hash->free;
            hash->free                = hash->slots[i];
            hash->slots[i]            = NULL;
        }
    }
    hash->size = 0;

    return;
}

ib_status_t ib_hash_remove_ex(
    ib_hash_t  *hash,
    void       *value,
    const char *key,
    size_t      key_length
) {
    assert(hash  != NULL);
    assert(key   != NULL);

    ib_status_t  rc          = IB_ENOENT;
    void        *local_value = NULL;

    rc = ib_hash_get_ex(hash, &local_value, key, key_length);
    if (rc != IB_OK) {
        return rc;
    }

    if ((value != NULL) && (local_value != NULL)) {
        *(void **)value = local_value;
    }
    rc = ib_hash_set_ex(hash, key, key_length, NULL);

    return rc;
}

ib_status_t ib_hash_remove(
    ib_hash_t   *hash,
    void        *value,
    const char  *key
) {
    assert(hash != NULL);
    assert(key  != NULL);

    return ib_hash_remove_ex(hash, value, (void *)key, strlen(key));
}

/** @} IronBeeUtilHash */
