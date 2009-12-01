
#ifndef _DSLIB_H
#define	_DSLIB_H

#include "bstr.h"

#define list_push(L, E) (L)->push(L, E)
#define list_pop(L) (L)->pop(L)
#define list_empty(L) (L)->empty(L)
#define list_get(L, N) (L)->get((list_t *)L, N)
#define list_replace(L, N, E) (L)->replace((list_t *)L, N, E)
#define list_add(L, N) (L)->push(L, N)
#define list_size(L) (L)->size(L)
#define list_iterator_reset(L) (L)->iterator_reset(L)
#define list_iterator_next(L) (L)->iterator_next(L)
#define list_destroy(L) (L)->destroy(L)

#define LIST_COMMON \
    int (*push)(list_t *, void *); \
    void *(*pop)(list_t *); \
    int (*empty)(list_t *); \
    void *(*get)(list_t *, size_t index); \
    int (*replace)(list_t *, size_t index, void *); \
    size_t (*size)(list_t *); \
    void (*iterator_reset)(list_t *); \
    void *(*iterator_next)(list_t *); \
    void (*destroy)(list_t *);

typedef struct list_t list_t;
typedef struct list_array_t list_array_t;
typedef struct list_linked_element_t list_linked_element_t;
typedef struct list_linked_t list_linked_t;

typedef struct table_t table_t;

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

    int first;
    int last;
    int max_size;
    int current_size;
    void **elements;

    size_t iterator_index;
};

list_t *list_linked_create(void);
list_t *list_array_create(int size);

struct table_t {
    list_t *list;
};

table_t *table_create(int size);
void table_add(table_t *, bstr *, void *);
void table_set(table_t *, bstr *, void *);
void *table_get(table_t *, bstr *);
void *table_getc(table_t *, char *);
void table_iterator_reset(table_t *);
bstr *table_iterator_next(table_t *, void **);
int table_size(table_t *t);
void table_destroy(table_t *);

#endif	/* _DSLIB_H */

