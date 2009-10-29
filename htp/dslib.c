
#include <stdlib.h>
#include <stdio.h>

#include "dslib.h"


// -- Queue List --

/**
 *
 */
static int list_linked_push(list_t *_q, void *data) {
    list_linked_t *q = (list_linked_t *)_q;
    list_linked_element_t *qe = calloc(1, sizeof(list_linked_element_t));
    if (qe == NULL) return -1;

    // Rememeber the element
    qe->data = data;

    // If the queue is empty, make this element first
    if (!q->first) {
        q->first = qe;
    }

    if (q->last) {
        q->last->next = qe;
    }

    q->last = qe;

    return 1;
}

/**
 *
 */
static void *list_linked_pop(list_t *_q) {
    list_linked_t *q = (list_linked_t *)_q;
    void *r = NULL;

    if (!q->first) {
        return NULL;
    }

    list_linked_element_t *qe = q->first;
    q->first = qe->next;
    r = qe->data;

    if (!q->first) {
        q->last = NULL;
    }

    free(qe);

    return r;
}

/**
 *
 */
static int list_linked_empty(list_t *_q) {
    list_linked_t *q = (list_linked_t *)_q;

    if (!q->first) {
        return 1;
    } else {
        return 0;
    }
}

/**
 *
 */
list_t *list_linked_create(void) {
    list_linked_t *q = calloc(1, sizeof(list_linked_t));
    if (q == NULL) return NULL;

    q->push = list_linked_push;
    q->pop = list_linked_pop;
    q->empty = list_linked_empty;

    return (list_t *)q;
}



// -- Queue Array --

/**
 *
 */
static int list_array_push(list_t *_q, void *element) {
    list_array_t *q = (list_array_t *)_q;

    // Check if we're full
    if (q->current_size >= q->max_size) {
        int new_size = q->max_size * 2;       
        
        q->elements = realloc(q->elements, new_size * sizeof(void *));       
        
        if (q->elements == NULL) {            
            return -1;
        }

        q->max_size = new_size;
        q->last = q->current_size;
    }   

    q->elements[q->last] = element;
    q->last++;
    if (q->last == q->max_size) {
        q->last = 0;
    }

    q->current_size++;

    return 1;
}

/**
 *
 */
static void *list_array_pop(list_t *_q) {
    list_array_t *q = (list_array_t *)_q;
    void *r = NULL;

    if (q->current_size == 0) {
        return NULL;
    }

    r = q->elements[q->first];
    q->first++;
    if (q->first == q->max_size) {
        q->first = 0;
    }

    q->current_size--;

    return r;
}

/**
 *
 */
static size_t list_array_size(list_t *_l) {
    return ((list_array_t *)_l)->current_size;    
}

/**
 *
 */
static void *list_array_get(list_t *_l, size_t index) {
    list_array_t *l = (list_array_t *)_l;
    void *r = NULL;

    if (index + 1 > l->current_size) return NULL;

    int i = l->first;
    r = l->elements[l->first];
    
    while(index--) {        
        if (++i == l->max_size) {
            i = 0;
        }
        
        r = l->elements[i];
    }

    return r;
}

void list_array_iterator_reset(list_array_t *l) {
    l->iterator_index = 0;
}

void *list_array_iterator_next(list_array_t *l) {
    void *r = NULL;

    if (l->iterator_index < l->current_size) {
        r = list_get(l, l->iterator_index);
        l->iterator_index++;
    }

    return r;
}

/**
 *
 */
list_t *list_array_create(int size) {
    // Allocate the list structure
    list_array_t *q = calloc(1, sizeof(list_array_t));
    if (q == NULL) return NULL;

    // Allocate the initial batch of elements
    q->elements = malloc(size * sizeof(void *));
    if (q->elements == NULL) {
        free(q);
        return NULL;
    }

    // Initialise structure
    q->first = 0;
    q->last = 0;
    q->max_size = size;    
    q->push = list_array_push;
    q->pop = list_array_pop;
    q->get = list_array_get;
    q->size = list_array_size;    
    q->iterator_reset = (void (*)(list_t *))list_array_iterator_reset;
    q->iterator_next = (void *(*)(list_t *))list_array_iterator_next;

    return (list_t *)q;
}

#if 0
int main(int argc, char **argv) {
    list_t *q = list_linked_create();

    list_push(q, "1");
    list_push(q, "2");
    list_push(q, "3");
    list_push(q, "4");

    char *s = NULL;
    while((s = (char *)list_pop(q)) != NULL) {
        printf("Got: %s\n", s);
    }

    free(q);
}
#endif


/**
 *
 */
table_t *table_create(int size) {
    table_t *t = calloc(1, sizeof(table_t));
    // Use a list behind the scenes
    t->list = list_array_create(size * 2);
    return t;
}

/**
 *
 */
void table_add(table_t *t, bstr *key, void *data) {
    // Lowercase key
    bstr *lkey = bstr_dup_lower(key);
       
    // Is there room?
    // XXX

    // Add key and data to the list
    list_add(t->list, lkey);
    list_add(t->list, data);
}

static void *table_get_int(table_t *t, bstr *key) {    
    // Iterate through the list, comparing
    // keys with the parameter, return data if found.
    bstr *ts = NULL;
    list_iterator_reset(t->list);
    while ((ts = list_iterator_next(t->list)) != NULL) {
        void *data = list_iterator_next(t->list);   
        if (bstr_cmp(ts, key) == 0) {            
            return data;
        }
    }   

    return NULL;
}

/**
 *
 */
void *table_getc(table_t *t, char *cstr) {
    bstr *key = bstr_cstrdup(cstr);
    bstr_tolowercase(key);

    void *data = table_get_int(t, key);
    free(key);
    return data;
}

/**
 * Note: we expect the key to already be lowercase.
 */
void *table_get(table_t *t, bstr *key) {
    bstr *lkey = bstr_dup_lower(key);

    void *data = table_get_int(t, lkey);
    free(lkey);
    return data;
}

/**
 *
 */
void table_iterator_reset(table_t *t) {
    list_iterator_reset(t->list);
}

/**
 *
 */
bstr *table_iterator_next(table_t *t, void **data) {
    bstr *s = list_iterator_next(t->list);
    if (s != NULL) {
        *data = list_iterator_next(t->list);
    }
    
    return s;
}

/**
 *
 */
int table_size(table_t *t) {    
    return list_size(t->list)/2;
}
