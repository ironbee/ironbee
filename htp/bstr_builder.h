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

