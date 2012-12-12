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

#include "htp_list.h"

#include <stdlib.h>
#include <stdio.h>

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

list_t *list_array_create(size_t size) {
    // Allocate the list structure
    list_array_t *l = calloc(1, sizeof (list_array_t));
    if (l == NULL) return NULL;

    // Allocate the initial batch of elements
    l->elements = malloc(size * sizeof (void *));
    if (l->elements == NULL) {
        free(l);
        return NULL;
    }

    // Initialize structure
    l->first = 0;
    l->last = 0;
    l->max_size = size;

    return (list_t *) l;
}

void list_array_destroy(list_array_t **_l) {
    if ((_l == NULL) || (*_l == NULL)) return;

    list_array_t *l = *_l;
    free(l->elements);
    free(l);
    *_l = NULL;
}

void *list_array_get(const list_array_t *l, size_t idx) {
    const void *r = NULL;

    if (idx + 1 > l->current_size) return NULL;

    size_t i = l->first;
    r = l->elements[l->first];

    while (idx--) {
        if (++i == l->max_size) {
            i = 0;
        }

        r = l->elements[i];
    }

    return (void *) r;
}

void *list_array_pop(list_array_t *l) {
    const void *r = NULL;

    if (l->current_size == 0) {
        return NULL;
    }

    size_t pos = l->first + l->current_size - 1;
    if (pos > l->max_size - 1) pos -= l->max_size;

    r = l->elements[pos];
    l->last = pos;

    l->current_size--;

    return (void *) r;
}

htp_status_t list_array_push(list_array_t *l, const void *e) {
    // Check whether we're full
    if (l->current_size >= l->max_size) {
        size_t new_size = l->max_size * 2;
        void *newblock = NULL;

        if (l->first == 0) {
            // The simple case of expansion is when the first
            // element in the list resides in the first slot. In
            // that case we just add some new space to the end,
            // adjust the max_size and that's that.
            newblock = realloc(l->elements, new_size * sizeof (void *));
            if (newblock == NULL) return HTP_ERROR;
        } else {
            // When the first element is not in the first
            // memory slot, we need to rearrange the order
            // of the elements in order to expand the storage area.
            newblock = malloc(new_size * sizeof (void *));
            if (newblock == NULL) return HTP_ERROR;

            // Copy the beginning of the list to the beginning of the new memory block
            memcpy((char *) newblock, (char *) l->elements + l->first * sizeof (void *), (l->max_size - l->first) * sizeof (void *));
            // Append the second part of the list to the end
            memcpy((char *) newblock + (l->max_size - l->first) * sizeof (void *), l->elements, l->first * sizeof (void *));

            free(l->elements);
        }

        l->first = 0;
        l->last = l->current_size;
        l->max_size = new_size;
        l->elements = newblock;
    }

    l->elements[l->last] = e;
    l->current_size++;

    l->last++;
    if (l->last == l->max_size) {
        l->last = 0;
    }

    return HTP_OK;
}

htp_status_t list_array_replace(list_array_t *l, size_t idx, const void *e) {
    if (idx + 1 > l->current_size) return HTP_ERROR;

    size_t i = l->first;

    while (idx--) {
        if (++i == l->max_size) {
            i = 0;
        }
    }

    l->elements[i] = e;

    return HTP_OK;
}

size_t list_array_size(const list_array_t *l) {
    return l->current_size;
}

void *list_array_shift(list_array_t *l) {
    void *r = NULL;

    if (l->current_size == 0) {
        return NULL;
    }

    r = l->elements[l->first];
    l->first++;
    if (l->first == l->max_size) {
        l->first = 0;
    }

    l->current_size--;

    return r;
}







// -- Queue List --

/**
 * Add element to list.
 *
 * @param list
 * @param element
 * @return 1 on success, -1 on error (memory allocation failure)
 */
int list_linked_push(list_t *_q, void *element) {
    list_linked_t *q = (list_linked_t *) _q;
    list_linked_element_t *qe = calloc(1, sizeof (list_linked_element_t));
    if (qe == NULL) return -1;

    // Remember the element
    qe->data = element;

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
 * Remove one element from the end of the list.
 *
 * @param list
 * @return a pointer to the removed element, or NULL if the list is empty.
 */
void *list_linked_pop(list_t *_q) {
    list_linked_t *q = (list_linked_t *) _q;
    void *r = NULL;

    if (!q->first) {
        return NULL;
    }

    // Find the last element
    list_linked_element_t *qprev = NULL;
    list_linked_element_t *qe = q->first;
    while (qe->next != NULL) {
        qprev = qe;
        qe = qe->next;
    }

    r = qe->data;
    free(qe);

    if (qprev != NULL) {
        qprev->next = NULL;
        q->last = qprev;
    } else {
        q->first = NULL;
        q->last = NULL;
    }

    return r;
}

/**
 * Remove one element from the beginning of the list.
 *
 * @param list
 * @return a pointer to the removed element, or NULL if the list is empty.
 */
void *list_linked_shift(list_t *_q) {
    list_linked_t *q = (list_linked_t *) _q;
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
 * Is the list empty?
 *
 * @param list
 * @return 1 if the list is empty, 0 if it is not
 */
int list_linked_empty(const list_t *_q) {
    const list_linked_t *q = (const list_linked_t *) _q;

    if (!q->first) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * Destroy list. This function will not destroy any of the
 * data stored in it. You'll have to do that manually beforehand.
 *
 * @param l
 */
void list_linked_destroy(list_linked_t **_l) {
    if ((_l == NULL) || (*_l == NULL)) return;

    list_linked_t *l = *_l;
    // Free the list structures
    list_linked_element_t *temp = l->first;
    list_linked_element_t *prev = NULL;
    while (temp != NULL) {
        free(temp->data);
        prev = temp;
        temp = temp->next;
        free(prev);
    }

    // Free the list itself
    free(l);
    *_l = NULL;
}

/**
 * Create a new linked list.
 *
 * @return a pointer to the newly created list (list_t), or NULL on memory allocation failure
 */
list_t *list_linked_create(void) {
    list_linked_t *q = calloc(1, sizeof (list_linked_t));
    if (q == NULL) return NULL;

    return (list_t *) q;
}

// -- Queue Array --



#if 0

int main(int argc, char **argv) {
    list_t *q = list_array_create(4);

    list_push(q, "1");
    list_push(q, "2");
    list_push(q, "3");
    list_push(q, "4");

    list_shift(q);
    list_push(q, "5");
    list_push(q, "6");

    char *s = NULL;
    while ((s = (char *) list_pop(q)) != NULL) {
        printf("Got: %s\n", s);
    }

    printf("---\n");

    list_push(q, "1");
    list_push(q, "2");
    list_push(q, "3");
    list_push(q, "4");

    while ((s = (char *) list_shift(q)) != NULL) {
        printf("Got: %s\n", s);
    }

    free(q);

    return 0;
}
#endif



