/***************************************************************************
 * Copyright (c) 2009-2010 Open Information Security Foundation
 * Copyright (c) 2010-2013 Qualys, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.

 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.

 * - Neither the name of the Qualys, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
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

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#ifndef _BSTR_BUILDER_H
#define	_BSTR_BUILDER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bstr_builder_t bstr_builder_t;

#include "htp_list.h"

struct bstr_builder_t {
    htp_list_t *pieces;
};

#define BSTR_BUILDER_DEFAULT_SIZE 16

/**
 * Adds one new string to the builder. This function will adopt the
 * string and destroy it when the builder itself is destroyed.
 *
 * @param[in] bb
 * @param[in] b
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
htp_status_t bstr_builder_appendn(bstr_builder_t *bb, bstr *b);

/**
 * Adds one new piece, in the form of a NUL-terminated string, to
 * the builder. This function will make a copy of the provided string.
 *
 * @param[in] bb
 * @param[in] cstr
 * @return HTP_OK on success, HTP_ERROR on failure.
 */
htp_status_t bstr_builder_append_c(bstr_builder_t *bb, const char *cstr);

/**
 * Adds one new piece, defined with the supplied pointer and
 * length, to the builder. This function will make a copy of the
 * provided data region.
 *
 * @param[in] bb
 * @param[in] data
 * @param[in] len
 * @return @return HTP_OK on success, HTP_ERROR on failure.
 */
htp_status_t bstr_builder_append_mem(bstr_builder_t *bb, const void *data, size_t len);

/**
 * Clears this string builder, destroying all existing pieces. You may
 * want to clear a builder once you've either read all the pieces and
 * done something with them, or after you've converted the builder into
 * a single string.
 *
 * @param[in] bb
 */
void bstr_builder_clear(bstr_builder_t *bb);

/**
 * Creates a new string builder.
 *
 * @return New string builder, or NULL on error.
 */
bstr_builder_t *bstr_builder_create(void);

/**
 * Destroys an existing string builder, also destroying all
 * the pieces stored within.
 *
 * @param[in] bb
 */
void bstr_builder_destroy(bstr_builder_t *bb);

/**
 * Returns the size (the number of pieces) currently in a string builder.
 *
 * @param[in] bb
 * @return size
 */
size_t bstr_builder_size(const bstr_builder_t *bb);

/**
 * Creates a single string out of all the pieces held in a
 * string builder. This method will not destroy any of the pieces.
 *
 * @param[in] bb
 * @return New string, or NULL on error.
 */
bstr *bstr_builder_to_str(const bstr_builder_t *bb);


#ifdef __cplusplus
}
#endif

#endif	/* _BSTR_BUILDER_H */

