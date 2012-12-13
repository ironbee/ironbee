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
typedef struct list_linked_element_t list_linked_element_t;
typedef struct list_linked_t list_linked_t;

// The default list implementation is array-based. The
//  linked list is not fully implemented yet.
#define list_t list_array_t
#define list_iterator_t list_array_iterator_t
#define list_add list_array_push
#define list_create list_array_create
#define list_destroy list_array_destroy
#define list_get list_array_get
#define list_pop list_array_pop
#define list_push list_array_push
#define list_replace list_array_replace
#define list_size list_array_size
#define list_shift list_array_shift

#include "htp.h"
#include "bstr.h"

// Data structures

struct list_array_t;
struct list_linked_t;


// Functions

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
 * @param idx
 * @return the desired element, or NULL if the list is too small, or
 *         if the element at that position carries a NULL
 */
void *list_array_get(const list_array_t *l, size_t idx);

/**
 * Remove one element from the end of the list.
 *
 * @param[in] l
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
htp_status_t list_array_push(list_array_t *l, void *e);

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
htp_status_t list_array_replace(list_array_t *l, size_t idx, void *e);

/**
 * Returns the size of the list.
 *
 * @param[in] l
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


// Linked list

/**
 * Create a new linked list.
 *
 * @return The newly created list, or NULL on memory allocation failure
 */
list_t *list_linked_create(void);

/**
 * Destroy list. This function will not destroy any of the
 * data stored in it. You'll have to do that manually beforehand.
 *
 * @param[in] l
 */
void list_linked_destroy(list_linked_t **l);

/**
 * Is the list empty?
 *
 * @param[in] l
 * @return 1 if the list is empty, 0 if it is not
 */
int list_linked_empty(const list_linked_t *l);

/**
 * Remove one element from the end of the list.
 *
 * @param[in] list
 * @return Pointer to the removed element, or NULL if the list is empty.
 */
void *list_linked_pop(list_linked_t *l);

/**
 * Add element to list.
 *
 * @param[in] l
 * @param[in] e
 * @return HTP_OK on success, HTP_ERROR on error.
 */
htp_status_t list_linked_push(list_linked_t *l, void *e);

/**
 * Remove one element from the beginning of the list.
 *
 * @param[in] l
 * @return Pointer to the removed element, or NULL if the list is empty.
 */
void *list_linked_shift(list_linked_t *l);

#ifdef	__cplusplus
}
#endif

#endif	/* HTP_LIST_H */

