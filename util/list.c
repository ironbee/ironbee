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

/**
 * @file
 * @brief IronBee &mdash; Utility List Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 */

/**
 * This is a doubly linked list.
 */

#include "ironbee_config_auto.h"

#include <ironbee/list.h>

#include <ironbee/debug.h>

ib_status_t ib_list_create(ib_list_t **plist, ib_mpool_t *pool)
{
    IB_FTRACE_INIT();
    /* Create the structure. */
    *plist = (ib_list_t *)ib_mpool_calloc(pool, 1, sizeof(**plist));
    if (*plist == NULL) {
        *plist = NULL;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    (*plist)->mp = pool;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_list_push(ib_list_t *list, void *data)
{
    IB_FTRACE_INIT();
    ib_list_node_t *node = (ib_list_node_t *)ib_mpool_calloc(list->mp,
                                                             1, sizeof(*node));
    if (node == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    node->data = data;

    if (list->nelts == 0) {
        IB_LIST_NODE_INSERT_INITIAL(list, node);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    IB_LIST_NODE_INSERT_LAST(list, node, ib_list_node_t);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_list_pop(ib_list_t *list, void *pdata)
{
    IB_FTRACE_INIT();
    if (list->nelts == 0) {
        if (pdata != NULL) {
            *(void **)pdata = NULL;
        }
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    if (pdata != NULL) {
        *(void **)pdata = IB_LIST_NODE_DATA(list->tail);
    }
    IB_LIST_NODE_REMOVE_LAST(list);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_list_unshift(ib_list_t *list, void *data)
{
    IB_FTRACE_INIT();
    ib_list_node_t *node = (ib_list_node_t *)ib_mpool_calloc(list->mp,
                                                             1, sizeof(*node));
    if (node == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    node->data = data;

    if (list->nelts == 0) {
        IB_LIST_NODE_INSERT_INITIAL(list, node);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    IB_LIST_NODE_INSERT_FIRST(list, node, ib_list_node_t);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_list_shift(ib_list_t *list, void *pdata)
{
    IB_FTRACE_INIT();
    if (list->nelts == 0) {
        if (pdata != NULL) {
            *(void **)pdata = NULL;
        }
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    if (pdata != NULL) {
        *(void **)pdata = IB_LIST_NODE_DATA(list->head);
    }
    IB_LIST_NODE_REMOVE_FIRST(list);

    IB_FTRACE_RET_STATUS(IB_OK);
}

void ib_list_clear(ib_list_t *list)
{
    IB_FTRACE_INIT();
    list->nelts = 0;
    list->head = list->tail = NULL;
    IB_FTRACE_RET_VOID();
}

size_t ib_list_elements(const ib_list_t *list)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_SIZET(list->nelts);
}

ib_list_node_t *ib_list_first(ib_list_t *list)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(ib_list_node_t, IB_LIST_FIRST(list));
}

ib_list_node_t *ib_list_last(ib_list_t *list)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(ib_list_node_t, IB_LIST_LAST(list));
}

ib_list_node_t *ib_list_node_next(ib_list_node_t *node)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(ib_list_node_t, IB_LIST_NODE_NEXT(node));
}

ib_list_node_t *ib_list_node_prev(ib_list_node_t *node)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(ib_list_node_t, IB_LIST_NODE_PREV(node));
}

const ib_list_node_t *ib_list_first_const(const ib_list_t *list)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(ib_list_node_t, IB_LIST_FIRST(list));
}

const ib_list_node_t *ib_list_last_const(const ib_list_t *list)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(ib_list_node_t, IB_LIST_LAST(list));
}

const ib_list_node_t *ib_list_node_next_const(const ib_list_node_t *node)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(ib_list_node_t, IB_LIST_NODE_NEXT(node));
}

const ib_list_node_t *ib_list_node_prev_const(const ib_list_node_t *node)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(ib_list_node_t, IB_LIST_NODE_PREV(node));
}

void ib_list_node_remove(ib_list_t *list, ib_list_node_t *node)
{
    IB_FTRACE_INIT();
    IB_LIST_NODE_REMOVE(list, node);
    IB_FTRACE_RET_VOID();
}

void *ib_list_node_data(ib_list_node_t *node)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(void, IB_LIST_NODE_DATA(node));
}

const void *ib_list_node_data_const(const ib_list_node_t *node)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_PTR(void, IB_LIST_NODE_DATA(node));
}

