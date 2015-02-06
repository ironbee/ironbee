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
#include <ironbee/mm.h>
#include <ironbee/types.h>
#include <ironbee/gen/list_gen.h>

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

/** @cond internal */
/**
 * List node structure.
 */
struct ib_list_node_t {
    IB_LIST_GEN_NODE_REQ_FIELDS(ib_list_node_t);  /* Required fields */
    void              *data;                  /**< Node data */
};

/**
 * List structure.
 */
struct ib_list_t {
    ib_mm_t mm;
    IB_LIST_GEN_REQ_FIELDS(ib_list_node_t);       /* Required fields */
};
/** @endcond */

/**
 * Create a list.
 *
 * @param plist Address which new list is written
 * @param mm Memory manager to use
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_create(ib_list_t **plist, ib_mm_t mm);

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
 * Set @a node 's data value.
 *
 * @param[in] node The node whose data element to set.
 * @param[in] data The data pointer to set.
 */
void DLL_PUBLIC ib_list_node_data_set(
    ib_list_node_t *node,
    void           *data
) NONNULL_ATTRIBUTE(1);

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
 * @param[in]  mm Memory manager
 * @param[out] pdest Pointer to new list
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_list_copy(const ib_list_t *src,
                                    ib_mm_t mm,
                                    ib_list_t **pdest);

/**
 * Insert an element into a list and the given index.
 *
 * This is O(index) if the index is not 0 or the size of the list.
 * This is O(1) if the index is 0 or the size of the list.
 *
 * @param[in] list The list.
 * @param[in] data The data to add.
 * @param[in] index The index to insert @a data at.
 *
 * @return
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 */
ib_status_t DLL_PUBLIC ib_list_insert(
    ib_list_t    *list,
    void         *data,
    const size_t  index
) NONNULL_ATTRIBUTE(1);

/** @} IronBeeUtilList */


#ifdef __cplusplus
}
#endif

#endif /* _IB_LIST_H_ */
