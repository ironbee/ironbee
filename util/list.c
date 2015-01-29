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

/**
 * @file
 * @brief IronBee --- Utility List Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 */

/**
 * This is a doubly linked list.
 */

#include "ironbee_config_auto.h"

#include <ironbee/list.h>

#include <assert.h>

ib_status_t ib_list_create(ib_list_t **plist, ib_mm_t mm)
{
    /* Create the structure. */
    *plist = (ib_list_t *)ib_mm_calloc(mm, 1, sizeof(**plist));
    if (*plist == NULL) {
        *plist = NULL;
        return IB_EALLOC;
    }
    (*plist)->mm = mm;

    return IB_OK;
}

ib_status_t ib_list_copy_nodes(const ib_list_t *src_list,
                               ib_list_t *dest_list)
{
    assert(src_list != NULL);
    assert(dest_list != NULL);

    ib_status_t rc;
    const ib_list_node_t *node;

    IB_LIST_LOOP_CONST(src_list, node) {
        assert(node->data != NULL);
        rc = ib_list_push(dest_list, node->data);
        if (rc != IB_OK) {
            return rc;
        }
    }
    return IB_OK;
}

ib_status_t ib_list_copy(const ib_list_t *src,
                         ib_mm_t mm,
                         ib_list_t **pdest)
{
    assert(src != NULL);
    assert(pdest != NULL);

    ib_status_t  rc;
    ib_list_t   *dest_list;

    rc = ib_list_create(&dest_list, mm);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_list_copy_nodes(src, dest_list);
    if (rc != IB_OK) {
        return rc;
    }

    *pdest = dest_list;
    return IB_OK;
}

ib_status_t ib_list_push(ib_list_t *list, void *data)
{
    ib_list_node_t *node = (ib_list_node_t *)ib_mm_calloc(list->mm,
        1, sizeof(*node)
    );
    if (node == NULL) {
        return IB_EALLOC;
    }
    node->data = data;

    if (list->nelts == 0) {
        IB_LIST_NODE_INSERT_INITIAL(list, node);
        return IB_OK;
    }

    IB_LIST_NODE_INSERT_LAST(list, node, ib_list_node_t);

    return IB_OK;
}

ib_status_t ib_list_pop(ib_list_t *list, void *pdata)
{
    if (list->nelts == 0) {
        if (pdata != NULL) {
            *(void **)pdata = NULL;
        }
        return IB_ENOENT;
    }

    if (pdata != NULL) {
        *(void **)pdata = IB_LIST_NODE_DATA(list->tail);
    }
    IB_LIST_NODE_REMOVE_LAST(list);

    return IB_OK;
}

ib_status_t ib_list_unshift(ib_list_t *list, void *data)
{
    ib_list_node_t *node = (ib_list_node_t *)ib_mm_calloc(list->mm,
        1, sizeof(*node)
    );
    if (node == NULL) {
        return IB_EALLOC;
    }
    node->data = data;

    if (list->nelts == 0) {
        IB_LIST_NODE_INSERT_INITIAL(list, node);
        return IB_OK;
    }

    IB_LIST_NODE_INSERT_FIRST(list, node, ib_list_node_t);

    return IB_OK;
}

ib_status_t ib_list_shift(ib_list_t *list, void *pdata)
{
    if (list->nelts == 0) {
        if (pdata != NULL) {
            *(void **)pdata = NULL;
        }
        return IB_ENOENT;
    }

    if (pdata != NULL) {
        *(void **)pdata = IB_LIST_NODE_DATA(list->head);
    }
    IB_LIST_NODE_REMOVE_FIRST(list);

    return IB_OK;
}

void ib_list_clear(ib_list_t *list)
{
    list->nelts = 0;
    list->head = list->tail = NULL;
    return;
}

size_t ib_list_elements(const ib_list_t *list)
{
    return list->nelts;
}

ib_list_node_t *ib_list_first(ib_list_t *list)
{
    return IB_LIST_FIRST(list);
}

ib_list_node_t *ib_list_last(ib_list_t *list)
{
    return IB_LIST_LAST(list);
}

ib_list_node_t *ib_list_node_next(ib_list_node_t *node)
{
    return IB_LIST_NODE_NEXT(node);
}

ib_list_node_t *ib_list_node_prev(ib_list_node_t *node)
{
    return IB_LIST_NODE_PREV(node);
}

const ib_list_node_t *ib_list_first_const(const ib_list_t *list)
{
    return IB_LIST_FIRST(list);
}

const ib_list_node_t *ib_list_last_const(const ib_list_t *list)
{
    return IB_LIST_LAST(list);
}

const ib_list_node_t *ib_list_node_next_const(const ib_list_node_t *node)
{
    return IB_LIST_NODE_NEXT(node);
}

const ib_list_node_t *ib_list_node_prev_const(const ib_list_node_t *node)
{
    return IB_LIST_NODE_PREV(node);
}

void ib_list_node_remove(ib_list_t *list, ib_list_node_t *node)
{
    IB_LIST_NODE_REMOVE(list, node);
    return;
}

void *ib_list_node_data(ib_list_node_t *node)
{
    return IB_LIST_NODE_DATA(node);
}

const void *ib_list_node_data_const(const ib_list_node_t *node)
{
    return IB_LIST_NODE_DATA(node);
}

void ib_list_node_data_set(
    ib_list_node_t *node,
    void           *data
)
{
    assert(node != NULL);

    node->data = data;
}