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

#ifndef _BSTR_H
#define	_BSTR_H

typedef struct bstr_t bstr_t;
typedef bstr_t bstr;

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "bstr_builder.h"

#ifdef __cplusplus
extern "C" {
#endif

// IMPORTANT This binary string library is used internally by the parser and you should
//           not rely on it in your code. The interface and the implementation may change
//           without warning.

struct bstr_t {
    /** The length of the string stored in the buffer. */
    size_t len;

    /** The current size of the buffer. If the buffer is bigger than the
     *  string then it will be able to expand without having to reallocate.
     */
    size_t size;

    /** Optional buffer pointer. If this pointer is NULL (as it currently is
     *  in virtually all cases) the string buffer will immediately follow
     *  this structure. If the pointer is not NUL, it points to the actual
     *  buffer used, and there's no data following this structure.
     */
    char *ptr;
};


// Defines

#define bstr_len(X) ((*(bstr_t *)(X)).len)
#define bstr_size(X) ((*(bstr_t *)(X)).size)
#define bstr_ptr(X) ( ((*(bstr_t *)(X)).ptr == NULL) ? ((char *)(X) + sizeof(bstr_t)) : (char *)(*(bstr_t *)(X)).ptr )


// Functions

bstr *bstr_alloc(size_t newsize);
 void bstr_free(bstr **s);
bstr *bstr_expand(bstr *s, size_t newsize);

bstr *bstr_dup(bstr *b);
bstr *bstr_dup_ex(bstr *b, size_t offset, size_t len);
bstr *bstr_dup_c(char *);
bstr *bstr_dup_mem(char *data, size_t len);

bstr *bstr_dup_lower(bstr *);

  int bstr_chr(bstr *, int);
  int bstr_rchr(bstr *, int);

  int bstr_cmp(bstr *, bstr *);
  int bstr_cmp_nocase(bstr *, bstr *);
  int bstr_cmp_c(bstr *, char *);
  int bstr_cmp_c_nocase(bstr *, char *);
  int bstr_cmp_ex(char *, size_t, char *, size_t);
  int bstr_cmp_nocase_ex(char *, size_t, char *, size_t);


bstr *bstr_to_lowercase(bstr *);

bstr *bstr_add(bstr *, bstr *);
bstr *bstr_add_c(bstr *, char *);
bstr *bstr_add_mem(bstr *, char *, size_t);

bstr *bstr_add_noex(bstr *, bstr *);
bstr *bstr_add_c_noex(bstr *, char *);
bstr *bstr_add_mem_noex(bstr *, char *, size_t);

  int bstr_index_of(bstr *haystack, bstr *needle);
  int bstr_index_of_nocase(bstr *haystack, bstr *needle);
  int bstr_index_of_c(bstr *haystack, char *needle);
  int bstr_index_of_c_nocase(bstr *haystack, char *needle);
  int bstr_index_of_mem(bstr *haystack, char *data, size_t len);
  int bstr_index_of_mem_nocase(bstr *haystack, char *data, size_t len);

  int bstr_begins_with_mem(bstr *haystack, char *data, size_t len);
  int bstr_begins_with_mem_nocase(bstr *haystack, char *data, size_t len);
  int bstr_begins_with(bstr *haystack, bstr *needle);
  int bstr_begins_with_c(bstr *haystack, char *needle);
  int bstr_begins_with_nocase(bstr *haystack, bstr *needle);
  int bstr_begins_withc_nocase(bstr *haystack, char *needle);

unsigned char bstr_char_at(bstr *s, size_t pos);

   void bstr_chop(bstr *b);
   void bstr_util_adjust_len(bstr *s, size_t newlen);
int64_t bstr_util_mem_to_pint(char *data, size_t len, int base, size_t *lastlen);
  char *bstr_util_memdup_to_c(char *data, size_t len);
  char *bstr_util_strdup_to_c(bstr *);

#ifdef __cplusplus
}
#endif

#endif	/* _BSTR_H */
