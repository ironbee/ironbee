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
 ****************************************************************************/

#ifndef _IB_LIST_H_
#define _IB_LIST_H_

/**
 * @file
 * @brief IronBee --- List Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/mpool.h>
#include <ironbee/types.h>

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilList List
 * @ingroup IronBeeUtil
 *
 * List routines.
 *
 * This is currently implemented as a doubly linked list.
 *
 * @{
 */

typedef struct ib_list_t ib_list_t;
typedef struct ib_list_node_t ib_list_node_t;

/**
 * Required fields for a list node structure.
 *
 * @todo Perhaps this should be a struct?
 *
 * @param ntype Node type literal
 */
#define IB_LIST_NODE_REQ_FIELDS(ntype) \
    ntype *next; /**< Next list node */ \
    ntype *prev /**< Previous list node */

/**
 * Required fields for a list structure.
 *
 * @todo Perhaps this should be a struct?
 *
 * @param ntype Node type literal
 */
#define IB_LIST_REQ_FIELDS(ntype) \
    size_t nelts; /**< Number of elements in list */ \
    ntype *head; /**< First node in list */ \
    ntype *tail /**< Last node in list */

/** @cond internal */
/**
 * List node structure.
 */
struct ib_list_node_t {
    IB_LIST_NODE_REQ_FIELDS(ib_list_node_t);  /* Required fields */
    void              *data;                  /**< Node data */
};

/**
 * List structure.
 */
struct ib_list_t {
    ib_mpool_t       *mp;
    IB_LIST_REQ_FIELDS(ib_list_node_t);       /* Required fields */
};
/** @endcond */

/**
 * Number of list elements
 *
 * @param list List
 *
 * @returns Number of list elements
 */
#define IB_LIST_ELEMENTS(list) ((list)->nelts)

/**
 * First node of a list.
 *
 * @param list List
 *
 * @returns List node
 */
#define IB_LIST_FIRST(list) ((list)->head)

/**
 * Last node of a list.
 *
 * @param list List
 *
 * @returns List node
 */
#define IB_LIST_LAST(list) ((list)->tail)

/**
 * Next node in a list in relation to another node.
 *
 * @param node Node
 *
 * @returns List node
 */
#define IB_LIST_NODE_NEXT(node) ((node) == NULL ? NULL : (node)->next)

/**
 * Previous node in a list in relation to another node.
 *
 * @param node Node
 *
 * @returns List node
 */
#define IB_LIST_NODE_PREV(node) ((node) == NULL ? NULL : (node)->prev)

/**
 * List node data.
 *
 * @param node Node
 *
 * @returns List node data
 */
#define IB_LIST_NODE_DATA(node) ((node) == NULL ? NULL : (node)->data)

/**
 * Insert a node after another node in a list.
 *
 * @param list List
 * @param at Node to insert after
 * @param node Node to insert
 * @param ntype Node type literal
 */
#define IB_LIST_NODE_INSERT_AFTER(list, at, node, ntype) \
    do { \
        ntype *__ib_list_node_ia_tmp = (at)->next; \
        (at)->next = (node); \
        (node)->prev = (at); \
        (node)->next = __ib_list_node_ia_tmp; \
        ++(list)->nelts; \
    } while(0)

/**
 * Insert a node before another node in a list.
 *
 * @param list List
 * @param at Node to insert before
 * @param node Node to insert
 * @param ntype Node type literal
 */
#define IB_LIST_NODE_INSERT_BEFORE(list, at, node, ntype) \
    do { \
        ntype *__ib_list_node_ib_tmp = (at)->prev; \
        (at)->prev = (node); \
        (node)->prev = __ib_list_node_ib_tmp; \
        (node)->next = (at); \
        ++(list)->nelts; \
    } while(0)

/**
 * Insert a node at the end of a list.
 *
 * @param list List
 * @param node Node to insert
 * @param ntype Node type literal
 */
#define IB_LIST_NODE_INSERT_LAST(list, node, ntype) \
    do { \
        IB_LIST_NODE_INSERT_AFTER((list), (list)->tail, (node), ntype); \
        (list)->tail = (node); \
    } while(0)

/**
 * Insert a node at the beginning of a list.
 *
 * @param list List
 * @param node Node to insert
 * @param ntype Node type literal
 */
#define IB_LIST_NODE_INSERT_FIRST(list, node, ntype) \
    do { \
        IB_LIST_NODE_INSERT_BEFORE((list), (list)->head, (node), ntype); \
        (list)->head = (node); \
    } while(0)

/**
 * Insert the first node of a list.
 *
 * @param list List
 * @param node Node to insert
 */
#define IB_LIST_NODE_INSERT_INITIAL(list, node) \
    do { \
        (list)->head = (list)->tail =(node); \
        (node)->next = (node)->prev = NULL; \
        (list)->nelts = 1; \
    } while(0)

/**
 * Remove a node from a list.
 *
 * @param list List
 * @param node Node to remove
 */
#define IB_LIST_NODE_REMOVE(list, node) \
    do { \
        if ((list)->nelts == 1) { \
            (list)->head = NULL; \
            (list)->tail = NULL; \
        } \
        else if ((node) == (list)->head) {    \
            (list)->head = (list)->head->next; \
            (list)->head->prev = NULL; \
        } \
        else if ((node) == (list)->tail) { \
            (node)->prev->next = (node)->next; \
            (list)->tail = (node)->prev; \
        } \
        else { \
            (node)->prev->next = (node)->next; \
            (node)->next->prev = (node)->prev; \
        } \
        --(list)->nelts; \
    } while(0)

/**
 * Remove the last node from a list.
 *
 * @param list List
 */
#define IB_LIST_NODE_REMOVE_LAST(list) \
    do { \
        if ((list)->tail != NULL) { \
            IB_LIST_NODE_REMOVE((list), (list)->tail); \
        } \
    } while(0)

/**
 * Remove the first node from a list.
 *
 * @param list List
 */
#define IB_LIST_NODE_REMOVE_FIRST(list) \
    do { \
        if ((list)->head != NULL) { \
            IB_LIST_NODE_REMOVE((list), (list)->head); \
        } \
    } while(0)

/**
 * Loop through all elements in the list.
 *
 * @todo Make this generic (non-ib_list_t specific)
 *
 * @warning Do not use to delete an element in the list. Instead use
 *          the @ref IB_LIST_LOOP_SAFE loop.
 *
 * @param list List
 * @param node Symbol holding node
 */
#define IB_LIST_LOOP(list, node) \
    for ((node) = ib_list_first(list); \
         (node) != NULL; \
         (node) = ib_list_node_next(node))

/**
 * Loop through all elements in the const list.
 *
 * @todo Make this generic (non-ib_list_t specific)
 *
 * @warning Do not use to delete an element in the list. Instead use
 *          the @ref IB_LIST_LOOP_SAFE loop.
 *
 * @param list List
 * @param node Symbol holding node
 */
#define IB_LIST_LOOP_CONST(list, node) \
    for ((node) = ib_list_first_const(list); \
         (node) != NULL; \
         (node) = ib_list_node_next_const(node))

/**
 * Loop through all elements in the list, taking care to allow for deletions.
 *
 * @todo Make this generic (non-ib_list_t specific)
 *
 * This loop allows deleting elements. If this is not needed, then
 * use the @ref IB_LIST_LOOP loop.
 *
 * @param list List
 * @param node Symbol holding node
 * @param node_next Symbol holding next node
 */
#define IB_LIST_LOOP_SAFE(list, node, node_next) \
    for ((node) = ib_list_first(list), \
         (node_next) = ib_list_node_next(node); \
         (node) != NULL; \
         (node) = (node_next), \
           (node_next) = ib_list_node_next(node))

/**
 * Loop through all elements in the list in reverse order.
 *
 * @todo Make this generic (non-ib_list_t specific)
 *
 * @warning Do not use to delete an element in the list. Instead use
 *          the @ref IB_LIST_LOOP_REVERSE_SAFE loop.
 *
 * @param list List
 * @param node Symbol holding node
 */
#define IB_LIST_LOOP_REVERSE(list, node) \
    for ((node) = ib_list_last(list); \
         (node) != NULL; \
         (node) = ib_list_node_prev(node))

/**
 * Loop through all elements in the list in reverse order.
 *
 * @todo Make this generic (non-ib_list_t specific)
 *
 * @warning Do not use to delete an element in the list. Instead use
 *          the @ref IB_LIST_LOOP_REVERSE_SAFE loop.
 *
 * @param list List
 * @param node Symbol holding node
 */
#define IB_LIST_LOOP_REVERSE_CONST(list, node) \
    for ((node) = ib_list_last_const(list); \
         (node) != NULL; \
         (node) = ib_list_node_prev(node))

/**
 * Loop through all elements in the list in reverse order, taking care
 * to allow for deletions.
 *
 * @todo Make this generic (non-ib_list_t specific)
 *
 * This loop allows deleting elements. If this is not needed, then
 * use the @ref IB_LIST_LOOP_REVERSE loop.
 *
 * @param list List
 * @param node Symbol holding node
 * @param node_next Symbol holding next node
 */
#define IB_LIST_LOOP_REVERSE_SAFE(list, node, node_next) \
    for ((node) = ib_list_last(list), \
         (node_next) = ib_list_node_prev(node); \
         (node) != NULL; \
         (node) = (node_next), \
           (node_next) = ib_list_node_prev(node))

/**
 * Create a list.
 *
 * @param plist Address which new list is written
 * @param pool Memory pool to use
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_create(ib_list_t **plist, ib_mpool_t *pool);

/**
 * Insert data at the end of a list.
 *
 * @param list List
 * @param data Data to store
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_push(ib_list_t *list, void *data);

/**
 * Fetch and remove data from the end of a list.
 *
 * @param list List
 * @param pdata Address which data is stored (if non-NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_pop(ib_list_t *list, void *pdata);

/**
 * Insert data at the first position of the list (queue behavior)
 *
 * This is currently just an alias for @ref ib_list_unshift().
 *
 * @param list List
 * @param data Address which data is stored (if non-NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_enqueue(ib_list_t *list, void *data);

#define ib_list_enqueue(list, data) \
    ib_list_unshift((list), (data))

/**
 * Fetch and remove data at the end of the list (queue behavior)
 *
 * This is currently just an alias for @ref ib_list_pop().
 *
 * @param list List
 * @param pdata Address which data is stored (if non-NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_dequeue(ib_list_t *list, void *pdata);

#define ib_list_dequeue(list, data) \
    ib_list_pop((list), (data))

/**
 * Insert data at the beginning of a list.
 *
 * @param list List
 * @param data Data to store
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_unshift(ib_list_t *list, void *data);

/**
 * Fetch and remove data from the beginning of a list.
 *
 * @param list List
 * @param pdata Address which data is stored (if non-NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_shift(ib_list_t *list, void *pdata);

/**
 * Clear a list.
 *
 * @note This does not destroy any element, but instead disassociates
 *       the elements with the list.
 *
 * @param list List
 */
void DLL_PUBLIC ib_list_clear(ib_list_t *list);

/**
 * Return number of elements stored in the list.
 *
 * @param list List
 *
 * @returns Number of elements stored in the list
 */
size_t DLL_PUBLIC ib_list_elements(const ib_list_t *list);

/**
 * Return first node in the list or NULL if there are no elements.
 *
 * @param list List
 *
 * @returns First node in the list or NULL if there are no elements.
 */
ib_list_node_t DLL_PUBLIC *ib_list_first(ib_list_t *list);

/**
 * Return last node in the list or NULL if there are no elements.
 *
 * @param list List
 *
 * @returns Last node in the list or NULL if there are no elements.
 */
ib_list_node_t DLL_PUBLIC *ib_list_last(ib_list_t *list);

/**
 * Return next node in the list.
 *
 * @param node Node in a list
 *
 * @returns Next node in the list.
 */
ib_list_node_t DLL_PUBLIC *ib_list_node_next(ib_list_node_t *node);

/**
 * Return previous node in the list.
 *
 * @param node Node in a list
 *
 * @returns Previous node in the list.
 */
ib_list_node_t DLL_PUBLIC *ib_list_node_prev(ib_list_node_t *node);

/**
 * Return first node in the const list or NULL if there are no elements.
 *
 * @param list List
 *
 * @returns First node in the list or NULL if there are no elements.
 */
const ib_list_node_t DLL_PUBLIC *ib_list_first_const(const ib_list_t *list);

/**
 * Return last node in the const list or NULL if there are no elements.
 *
 * @param list List
 *
 * @returns Last node in the list or NULL if there are no elements.
 */
const ib_list_node_t DLL_PUBLIC *ib_list_last_const(const ib_list_t *list);

/**
 * Return next node in the const list.
 *
 * @param node Node in a list
 *
 * @returns Next node in the list.
 */
const ib_list_node_t DLL_PUBLIC *ib_list_node_next_const(const ib_list_node_t *node);

/**
 * Return previous node in the const list.
 *
 * @param node Node in a list
 *
 * @returns Previous node in the list.
 */
const ib_list_node_t DLL_PUBLIC *ib_list_node_prev_const(const ib_list_node_t *node);

/**
 * Remove a node from the list.
 *
 * @param list List
 * @param node Node in a list
 */
void DLL_PUBLIC ib_list_node_remove(ib_list_t *list, ib_list_node_t *node);

/**
 * Return data from the given node.
 *
 * @param node Node in a list
 *
 * @returns Data stored in the node
 */
void DLL_PUBLIC *ib_list_node_data(ib_list_node_t *node);

/**
 * Return const data from the given node.
 *
 * @param node Node in a list
 *
 * @returns Data stored in the node
 */
const void DLL_PUBLIC *ib_list_node_data_const(const ib_list_node_t *node);

/**
 * Copy all items from @a src_list to @a dest_list.
 *
 * @note This is a shallow copy; if the data items themselves are pointers,
 * the pointer is copied, with the new list containing a list of aliases.
 *
 * @param[in] src_list List of items to copy
 * @param[in,out] dest_list List to copy items into
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_copy_nodes(const ib_list_t *src_list,
                                          ib_list_t *dest_list);


/**
 * Create a new list pointed at by @a pdest and copy all items from @a src
 * into it.
 *
 * @note This is a shallow copy; if the data items themselves are pointers,
 * the pointer is copied, with the new list containing a list of aliases.
 *
 * @param[in]  src List of items to copy
 * @param[in]  mp Memory pool
 * @param[out] pdest Pointer to new list
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_copy(const ib_list_t *src,
                                    ib_mpool_t *mp,
                                    ib_list_t **pdest);

/** @} IronBeeUtilList */


#ifdef __cplusplus
}
#endif

#endif /* _IB_LIST_H_ */
