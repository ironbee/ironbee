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

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#ifndef _BSTR_BUILDER_H
#define	_BSTR_BUILDER_H

typedef struct bstr_builder_t bstr_builder_t;

#include "dslib.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bstr_builder_t {
    list_t *pieces;
};

#define BSTR_BUILDER_DEFAULT_SIZE 16

bstr_builder_t * bstr_builder_create(void);
void bstr_builder_destroy(bstr_builder_t *bb);

size_t bstr_builder_size(bstr_builder_t *bb);
void bstr_builder_clear(bstr_builder_t *bb);

int bstr_builder_append(bstr_builder_t *bb, bstr *b);
int bstr_builder_append_mem(bstr_builder_t *bb, const char *data, size_t len);
int bstr_builder_append_c(bstr_builder_t *bb, const char *str);
bstr * bstr_builder_to_str(bstr_builder_t *bb);

#ifdef __cplusplus
}
#endif

#endif	/* _BSTR_BUILDER_H */

