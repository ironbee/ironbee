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

#ifndef HTP_LIST_H
#define	HTP_LIST_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct list_array_t list_array_t;
typedef struct list_array_iterator_t list_array_iterator_t;
typedef struct list_linked_element_t list_linked_element_t;
typedef struct list_linked_t list_linked_t;

#define list_t list_array_t
#define list_iterator_t list_array_iterator_t

#define list_add list_array_push
#define list_create list_array_create
#define list_destroy list_array_destroy
#define list_get list_array_get
#define list_iterator_next list_array_int_iterator_next
#define list_iterator_reset list_array_int_iterator_reset
#define iterator_init list_array_iterator_init
#define iterator_next list_array_iterator_next
#define list_pop list_array_pop
#define list_push list_array_push
#define list_replace list_array_replace
#define list_size list_array_size
#define list_shift list_array_shift

#include "htp.h"
#include "bstr.h"

struct list_linked_element_t {

    void *data;

    list_linked_element_t *next;
};

struct list_linked_t {

    list_linked_element_t *first;

    list_linked_element_t *last;
};

struct list_array_t {

    size_t first;

    size_t last;

    size_t max_size;

    size_t current_size;

    const void ** elements;

    size_t iterator_index;
};

struct list_array_iterator_t {

    list_array_t *l;

    size_t index;
};


/**
 * Create new array-backed list.
 *
 * @param[in] size
 * @return Newly created list.
 */
list_t *list_array_create(size_t size);

/**
 * Free the memory occupied by this list. This function assumes
 * the elements held by the list were freed beforehand.
 *
 * @param[in] l
 */
void list_array_destroy(list_array_t **l);

/**
 * Find the element at the given index.
 *
 * @param[in] l
 * @param index
 * @return the desired element, or NULL if the list is too small, or
 *         if the element at that position carries a NULL
 */
void *list_array_get(const list_array_t *l, size_t idx);

/**
 * Remove one element from the end of the list.
 *
 * @param[in] ;
 * @return The removed element, or NULL if the list is empty.
 */
void *list_array_pop(list_array_t *l);

/**
 * Add new element to the end of the list, expanding the list as necessary.
 *
 * @param[in] l
 * @param[in] e
 * @return HTP_OK on success or HTP_ERROR on failure.
 *
 */
htp_status_t list_array_push(list_array_t *l, const void *e);

/**
 * Replace the element at the given index with the provided element.
 *
 * @param[in] l
 * @param[in] idx
 * @param[in] e
 *
 * @return HTTP_OK if an element with the given index was replaced; HTP_ERROR
 *         if the desired index does not exist.
 */
htp_status_t list_array_replace(list_array_t *l, size_t idx, const void *e);

/**
 * Returns the size of the list.
 *
 * @param[in] list
 * @return List size.
 */
size_t list_array_size(const list_array_t *l);

/**
 * Remove one element from the beginning of the list.
 *
 * @param[in] l
 * @return The removed element, or NULL if the list is empty.
 */
void *list_array_shift(list_array_t *l);

/**
 * Advance the iterator to the next value.
 *
 * @param[in] l
 * @return The next list value, or NULL if there aren't more elements
 *         left to iterate over or if the element itself is NULL.
 */
void list_array_int_iterator_reset(list_array_t *l);

/**
 * Reset the list iterator.
 *
 * @param[in] l
 */
void *list_array_int_iterator_next(list_array_t *l);

/**
 * Initialize iterator for the given list. After this, repeatedly
 * invoking list_array_iterator_next() will walk the entire list.
 *
 * @param[in] l
 * @param[in] it
 */
void list_array_iterator_init(list_array_t *l, list_array_iterator_t *it);

/**
 * Move the iterator to the next element in the list.
 *
 * @param[in] it
 * @return Pointer to the next element, or NULL if no more elements are available.
 */
void *list_array_iterator_next(list_array_iterator_t *it);





list_t *list_linked_create(void);

void list_linked_destroy(list_linked_t **_l);

int list_linked_push(list_t *_q, void *element);

void *list_linked_pop(list_t *_q);

void *list_linked_shift(list_t *_q);

int list_linked_empty(const list_t *_q);


#ifdef	__cplusplus
}
#endif

#endif	/* HTP_LIST_H */

