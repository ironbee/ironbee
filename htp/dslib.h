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

#ifndef _DSLIB_H
#define	_DSLIB_H

typedef struct list_t list_t;
typedef struct list_array_t list_array_t;
typedef struct list_linked_element_t list_linked_element_t;
typedef struct list_linked_t list_linked_t;
typedef struct table_t table_t;

#include "bstr.h"

#ifdef __cplusplus
extern "C" {
#endif

// IMPORTANT This library is used internally by the parser and you should
//           not rely on it in your code. The implementation may change at
//           any time.s

// Lists

#define list_push(L, E) (L)->push(L, E)
#define list_pop(L) (L)->pop(L)
#define list_empty(L) (L)->empty(L)
#define list_get(L, N) (L)->get((list_t *)L, N)
#define list_replace(L, N, E) (L)->replace((list_t *)L, N, E)
#define list_add(L, N) (L)->push(L, N)
#define list_size(L) (L)->size(L)
#define list_iterator_reset(L) (L)->iterator_reset(L)
#define list_iterator_next(L) (L)->iterator_next(L)
#define list_destroy(L) (*(L))->destroy(L)
#define list_shift(L) (L)->shift(L)

#define LIST_COMMON \
    int (*push)(list_t *, void *); \
    void *(*pop)(list_t *); \
    int (*empty)(list_t *); \
    void *(*get)(list_t *, size_t index); \
    int (*replace)(list_t *, size_t index, void *); \
    size_t (*size)(list_t *); \
    void (*iterator_reset)(list_t *); \
    void *(*iterator_next)(list_t *); \
    void (*destroy)(list_t **); \
    void *(*shift)(list_t *)

struct list_t {
    LIST_COMMON;
};

struct list_linked_element_t {
    void *data;
    list_linked_element_t *next;
};

struct list_linked_t {
    LIST_COMMON;

    list_linked_element_t *first;
    list_linked_element_t *last;
};

struct list_array_t {
    LIST_COMMON;

    size_t first;
    size_t last;
    size_t max_size;
    size_t current_size;
    void **elements;

    size_t iterator_index;
};

list_t *list_linked_create(void);
void list_linked_destroy(list_linked_t **_l);

list_t *list_array_create(size_t size);
void list_array_iterator_reset(list_array_t *l);
void *list_array_iterator_next(list_array_t *l);
void list_array_destroy(list_array_t **_l);


// Table

struct table_t {
    list_t *list;
};

table_t *table_create(size_t size);
     int table_add(table_t *, bstr *, void *);
     int table_addn(table_t *, bstr *, void *);
    void table_set(table_t *, bstr *, void *);     
   void *table_get(table_t *, bstr *);
   void *table_get_c(table_t *, char *);
    void table_iterator_reset(table_t *);
   bstr *table_iterator_next(table_t *, void **);         
  size_t table_size(table_t *t);
    void table_destroy(table_t **);
    void table_clear(table_t *);

#ifdef __cplusplus
}
#endif

#endif	/* _DSLIB_H */

