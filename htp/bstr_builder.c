/***************************************************************************
 * Copyright 2009-2010 Open Information Security Foundation
 * Copyright 2010-2011 Qualys, Inc.
 *
 * Licensed to You under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#include "bstr.h"
#include "bstr_builder.h"
#include "dslib.h"

/**
 * Returns the size (the number of pieces) currently in a string builder.
 *
 * @param bb
 * @return size
 */
size_t bstr_builder_size(bstr_builder_t *bb) {
    return list_size(bb->pieces);
}

/**
 * Clears this string builder, destroying all existing pieces. You may
 * want to clear a builder once you've either read all the pieces and
 * done something with them, or after you've converted the builder into
 * a single string.
 *
 * @param bb
 */
void bstr_builder_clear(bstr_builder_t *bb) {
    // TODO Need list_clear() here.

    // Do nothing if the list is empty
    if (list_size(bb->pieces) == 0) return;
    
    // Destroy any pieces we might have
    bstr *b = NULL;
    list_iterator_reset(bb->pieces);
    while ((b = list_iterator_next(bb->pieces)) != NULL) {
        bstr_free(&b);
    }

    list_destroy(&bb->pieces);

    bb->pieces = list_array_create(BSTR_BUILDER_DEFAULT_SIZE);    
}

/**
 * Creates a new string builder.
 *
 * @return New string builder
 */
bstr_builder_t * bstr_builder_create() {
    bstr_builder_t *bb = calloc(1, sizeof(bstr_builder_t));
    if (bb == NULL) return NULL;

    bb->pieces = list_array_create(BSTR_BUILDER_DEFAULT_SIZE);
    if (bb->pieces == NULL) {
        free(bb);
        return NULL;
    }

    return bb;
}

/**
 * Destroys an existing string builder, also destroying all
 * the pieces stored within.
 * 
 * @param bb
 */
void bstr_builder_destroy(bstr_builder_t *bb) {
    if (bb == NULL) return;

    // Destroy any pieces we might have
    bstr *b = NULL;
    list_iterator_reset(bb->pieces);
    while ((b = list_iterator_next(bb->pieces)) != NULL) {
        bstr_free(&b);
    }

    list_destroy(&bb->pieces);
    
    free(bb);
}

/**
 * Adds one new string to the builder.
 *
 * @param bb
 * @param b
 * @return Success indication
 */
int bstr_builder_append(bstr_builder_t *bb, bstr *b) {
    return list_push(bb->pieces, b);
}

/**
 * Adds one new piece, defined with the supplied pointer and
 * length, to the builder.
 *
 * @param bb
 * @param data
 * @param len
 * @return Success indication
 */
int bstr_builder_append_mem(bstr_builder_t *bb, const char *data, size_t len) {
    bstr *b = bstr_dup_mem(data, len);
    if (b == NULL) return -1;
    return list_push(bb->pieces, b);
}

/**
 * Adds one new piece, in the form of a NUL-terminated string, to
 * the builder.
 *
 * @param bb
 * @param cstr
 * @return Success indication
 */
int bstr_builder_append_c(bstr_builder_t *bb, const char *cstr) {
    bstr *b = bstr_dup_c(cstr);
    if (b == NULL) return -1;
    return list_push(bb->pieces, b);
}

/**
 * Creates a single string out of all the pieces held in a
 * string builder. This method will not destroy any of the pieces.
 *
 * @param bb
 * @return New string
 */
bstr * bstr_builder_to_str(bstr_builder_t *bb) {
    bstr *b = NULL;
    size_t len = 0;

    // Determine the size of the string
    list_iterator_reset(bb->pieces);
    while ((b = list_iterator_next(bb->pieces)) != NULL) {
        len += bstr_len(b);
    }

    // Allocate string
    bstr *bnew = bstr_alloc(len);
    if (bnew == NULL) return NULL;

    // Determine the size of the string
    list_iterator_reset(bb->pieces);
    while ((b = list_iterator_next(bb->pieces)) != NULL) {
        bstr_add_noex(bnew, b);
    }

    return bnew;
}
