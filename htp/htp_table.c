/***************************************************************************
 * Copyright (c) 2009-2010, Open Information Security Foundation
 * Copyright (c) 2009-2012, Qualys, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the Qualys, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************/

#include "htp_table_private.h"

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

htp_status_t htp_table_add(htp_table_t *table, const bstr *key, const void *element) {
    bstr *dupkey = bstr_dup(key);
    if (dupkey == NULL) return HTP_ERROR;

    if (htp_table_addn(table, dupkey, element) != HTP_OK) {
        free(dupkey);
        return HTP_ERROR;
    }

    return HTP_OK;
}

htp_status_t htp_table_addn(htp_table_t *table, const bstr *key, const void *element) {
    // Add key
    if (list_add(table->list, (void *)key) != HTP_OK) return HTP_ERROR;

    // Add element
    if (list_add(table->list, (void *)element) != HTP_OK) {
        list_pop(table->list);
        return HTP_ERROR;
    }

    return HTP_OK;
}

void htp_table_clear(htp_table_t *table) {
    if (table == NULL) return;

    size_t size = list_size(table->list);

    list_destroy(&table->list);

    // Use a list behind the scenes
    table->list = list_array_create(size == 0 ? 10 : size);
    if (table->list == NULL) {
        free(table);
    }
}

htp_table_t *htp_table_create(size_t size) {
    htp_table_t *t = calloc(1, sizeof (htp_table_t));
    if (t == NULL) return NULL;

    // Use a list behind the scenes
    t->list = list_array_create(size * 2);
    if (t->list == NULL) {
        free(t);
        return NULL;
    }

    return t;
}

void htp_table_destroy(htp_table_t **_table) {
    if ((_table == NULL)||(*_table == NULL)) return;

    htp_table_t *table = *_table;

    // Free table keys only
    for (int i = 0, n = list_size(table->list); i < n; i += 2) {
        bstr *key = list_get(table->list, i);
        bstr_free(&key);
    }

    list_destroy(&table->list);

    free(table);
    *_table = NULL;
}

void *htp_table_get(const htp_table_t *table, const bstr *key) {
    if ((table == NULL)||(key == NULL)) return NULL;

    // Iterate through the list, comparing
    // keys with the parameter, return data if found.    
    for (int i = 0, n = list_size(table->list); i < n; i += 2) {
        bstr *key_candidate = list_get(table->list, i);        
        void *element = list_get(table->list, i + 1);
        if (bstr_cmp_nocase(key_candidate, key) == 0) {
            return element;
        }
    }

    return NULL;
}

void *htp_table_get_c(const htp_table_t *table, const char *ckey) {
    if ((table == NULL)||(ckey == NULL)) return NULL;

    // Iterate through the list, comparing
    // keys with the parameter, return data if found.    
    for (int i = 0, n = list_size(table->list); i < n; i += 2) {
        bstr *key_candidate = list_get(table->list, i);
        void *element = list_get(table->list, i + 1);
        if (bstr_cmp_c_nocase(key_candidate, ckey) == 0) {
            return element;
        }
    }

    return NULL;
}

bstr *htp_table_iterator_next(htp_table_t *t, void **data) {
    // Get the key
    bstr *s = list_iterator_next(t->list);

    if (s != NULL) {
        // Get the value
        *data = list_iterator_next(t->list);
    }

    // Return the key
    return s;
}

void htp_table_iterator_reset(htp_table_t *table) {
    list_iterator_reset(table->list);
}

size_t htp_table_size(const htp_table_t *table) {
    return list_size(table->list) / 2;
}

