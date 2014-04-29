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
 * @brief IronBee --- String Set Implementation
 *
 * See @ref ib_stringset_t for details.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @nosubgrouping
 */

#include "ironbee_config_auto.h"

#include <ironbee/stringset.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Is @a a a prefix of a @b. */

static
bool is_prefix(const ib_stringset_entry_t *a, const ib_stringset_entry_t *b)
{
    const ib_stringset_entry_t *a_entry = (const ib_stringset_entry_t *)a;
    const ib_stringset_entry_t *b_entry = (const ib_stringset_entry_t *)b;

    return
        a_entry->length <= b_entry->length &&
        memcmp(a_entry->string, b_entry->string, a_entry->length) == 0
        ;
}

/** Alphabetical order for qsort() and less() */
static
int compare(const void *a, const void *b)
{
    const ib_stringset_entry_t *a_entry = (const ib_stringset_entry_t *)a;
    const ib_stringset_entry_t *b_entry = (const ib_stringset_entry_t *)b;

    size_t min_size =
        a_entry->length < b_entry->length ? a_entry->length : b_entry->length;
    int cmp = memcmp(a_entry->string, b_entry->string, min_size);

    if (cmp == 0) {
        if (a_entry->length < b_entry->length) {
            return -1;
        }
        else if (b_entry->length < a_entry->length) {
            return 1;
        }
        else {
            return 0;
        }
    }
    else {
        return cmp;
    }
}

/** Is `compare(a, b) < 0`.  Used by ib_stringset_query(). */
static
bool less(const ib_stringset_entry_t *a, const ib_stringset_entry_t *b)
{
    return compare(a, b) < 0;
}

ib_status_t ib_stringset_init(
    ib_stringset_t       *set,
    ib_stringset_entry_t *entries,
    size_t                num_entries
)
{
    assert(set != NULL);
    assert(entries != NULL);

    set->entries = entries;
    set->num_entries = num_entries;

    qsort((void *)set->entries, num_entries, sizeof(*entries), compare);

    return IB_OK;
}

ib_status_t ib_stringset_query(
    const ib_stringset_t        *set,
    const char                  *string,
    size_t                       string_length,
    const ib_stringset_entry_t **out_entry
)
{
    assert(set != NULL);
    assert(string != NULL);

    ib_stringset_entry_t key = {string, string_length, NULL};

    /* Based on C++ std::upper_bound() */
    size_t len = set->num_entries;
    size_t first = 0;

    while (len > 0) {
        size_t half = len >> 1;
        size_t middle = first + half;

        if (less(&key, &set->entries[middle])) {
            len = half;
        }
        else {
            first = middle + 1;
            len = len - half - 1;
        }
    }

    /* At this point, first is the first element greater than key. */
    if (first != 0 && is_prefix(&set->entries[first - 1], &key)) {
        if (out_entry != NULL) {
            *out_entry = &set->entries[first - 1];
        }
        return IB_OK;
    }
    else {
        return IB_ENOENT;
    }
}
