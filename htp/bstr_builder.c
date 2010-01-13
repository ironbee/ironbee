/*
 * LibHTP (http://www.libhtp.org)
 * Copyright 2009,2010 Ivan Ristic <ivanr@webkreator.com>
 *
 * LibHTP is an open source product, released under terms of the General Public Licence
 * version 2 (GPLv2). Please refer to the file LICENSE, which contains the complete text
 * of the license.
 *
 * In addition, there is a special exception that allows LibHTP to be freely
 * used with any OSI-approved open source licence. Please refer to the file
 * LIBHTP_LICENSING_EXCEPTION for the full text of the exception.
 *
 */

#include "bstr.h"
#include "bstr_builder.h"
#include "dslib.h"

size_t bstr_builder_size(bstr_builder_t *bb) {
    return list_size(bb->pieces);
}

void bstr_builder_clear(bstr_builder_t *bb) {
    // Destroy any pieces we might have
    bstr *b = NULL;
    list_iterator_reset(bb->pieces);
    while ((b = list_iterator_next(bb->pieces)) != NULL) {
        bstr_free(b);
    }

    list_destroy(bb->pieces);

    bb->pieces = list_array_create(BSTR_BUILDER_DEFAULT_SIZE);
    // TODO What should we do on allocation failure?
}

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

void bstr_builder_destroy(bstr_builder_t *bb) {
    if (bb == NULL) return;

    // Destroy any pieces we might have
    bstr *b = NULL;
    list_iterator_reset(bb->pieces);
    while ((b = list_iterator_next(bb->pieces)) != NULL) {
        bstr_free(b);
    }

    list_destroy(bb->pieces);
    
    free(bb);
}

int bstr_builder_append(bstr_builder_t *bb, bstr *b) {
    return list_push(bb->pieces, b);
}

int bstr_builder_append_mem(bstr_builder_t *bb, char *data, size_t len) {
    bstr *b = bstr_memdup(data, len);
    if (b == NULL) return -1; // TODO Is the return code correct?
    return list_push(bb->pieces, b);
}

int bstr_builder_append_cstr(bstr_builder_t *bb, char *str) {
    bstr *b = bstr_cstrdup(str);
    if (b == NULL) return -1; // TODO Is the return code correct?
    return list_push(bb->pieces, b);
}

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
        bstr_add_str_noex(bnew, b);
    }

    return bnew;
}
