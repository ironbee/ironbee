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

#ifndef _IB_LIST_GEN_H_
#define _IB_LIST_GEN_H_

/**
 * @file
 * @brief IronBee --- List Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 *
 * A collection of macros used for building doubly linked lists.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Required fields for a list structure.
 *
 * Include this in your list struct.
 *
 * @code
 * struct mylist_t {
 *   const char *my_list_name;
 *   void       *another_field;
 *   IB_LIST_GEN_REQ_FIELDS;
 * };
 * @endcode
 *
 * @param ntype Node type literal. eg. ib_my_list_t.
 */
#define IB_LIST_GEN_REQ_FIELDS(ntype) \
    size_t nelts; /**< Number of elements in list */ \
    ntype *head; /**< First node in list */ \
    ntype *tail /**< Last node in list */

/**
 * Required fields for a list node structure.
 *
 * Include this in your list node struct.
 *
 * @code
 * struct mylist_node_t {
 *   IB_LIST_GEN_REQ_FIELDS;
 * };
 * @endcode
 *
 * @param ntype Node type literal. eg. ib_my_list_t.
 */
#define IB_LIST_GEN_NODE_REQ_FIELDS(ntype) \
    ntype *next; /**< Next list node */ \
    ntype *prev /**< Previous list node */

/**
 * Number of list elements
 *
 * @param list List
 *
 * @returns Number of list elements
 */
#define IB_LIST_GEN_ELEMENTS(list) ((list)->nelts)

/**
 * First node of a list.
 *
 * @param list List
 *
 * @returns List node
 */
#define IB_LIST_GEN_FIRST(list) ((list)->head)

/**
 * Last node of a list.
 *
 * @param list List
 *
 * @returns List node
 */
#define IB_LIST_GEN_LAST(list) ((list)->tail)

/**
 * Next node in a list in relation to another node.
 *
 * @param node Node
 *
 * @returns List node
 */
#define IB_LIST_GEN_NODE_NEXT(node) ((node) == NULL ? NULL : (node)->next)

/**
 * Previous node in a list in relation to another node.
 *
 * @param node Node
 *
 * @returns List node
 */
#define IB_LIST_GEN_NODE_PREV(node) ((node) == NULL ? NULL : (node)->prev)

/**
 * List node data.
 *
 * @param node Node
 *
 * @returns List node data
 */
#define IB_LIST_GEN_NODE_DATA(node) ((node) == NULL ? NULL : (node)->data)

/**
 * Insert a node after another node in a list.
 *
 * @param list List
 * @param at Node to insert after
 * @param node Node to insert
 * @param ntype Node type literal
 */
#define IB_LIST_GEN_NODE_INSERT_AFTER(list, at, node, ntype) \
    do { \
        ntype *__ib_list_node_ia_tmp = (at)->next; \
        if ((at)->next != NULL) { \
            (at)->next->prev = node; \
        } \
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
#define IB_LIST_GEN_NODE_INSERT_BEFORE(list, at, node, ntype) \
    do { \
        ntype *__ib_list_node_ib_tmp = (at)->prev; \
        if ((at)->prev != NULL) { \
            (at)->prev->next = node; \
        } \
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
#define IB_LIST_GEN_NODE_INSERT_LAST(list, node, ntype) \
    do { \
        IB_LIST_GEN_NODE_INSERT_AFTER((list), (list)->tail, (node), ntype); \
        (list)->tail = (node); \
    } while(0)

/**
 * Insert a node at the beginning of a list.
 *
 * @param list List
 * @param node Node to insert
 * @param ntype Node type literal
 */
#define IB_LIST_GEN_NODE_INSERT_FIRST(list, node, ntype) \
    do { \
        IB_LIST_GEN_NODE_INSERT_BEFORE((list), (list)->head, (node), ntype); \
        (list)->head = (node); \
    } while(0)

/**
 * Insert the first node of a list.
 *
 * @param list List
 * @param node Node to insert
 */
#define IB_LIST_GEN_NODE_INSERT_INITIAL(list, node) \
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
#define IB_LIST_GEN_NODE_REMOVE(list, node) \
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
#define IB_LIST_GEN_NODE_REMOVE_LAST(list) \
    do { \
        if ((list)->tail != NULL) { \
            IB_LIST_GEN_NODE_REMOVE((list), (list)->tail); \
        } \
    } while(0)

/**
 * Remove the first node from a list.
 *
 * @param list List
 */
#define IB_LIST_GEN_NODE_REMOVE_FIRST(list) \
    do { \
        if ((list)->head != NULL) { \
            IB_LIST_GEN_NODE_REMOVE((list), (list)->head); \
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

#ifdef __cplusplus
}
#endif

#endif /* _IB_LIST_GEN_H_ */