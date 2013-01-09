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
 * @brief IronBee --- Capture implementation.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfel@calfeld.net>
 */

#include "ironbee_config_auto.h"

#include <ironbee/capture.h>

#include <assert.h>

static const int MAX_CAPTURE_NUM = 9;
typedef struct {
    const char *full;
    const char *name;
} capture_names_t;
static const capture_names_t names[] =
{
    { IB_TX_CAPTURE":0", "0" },
    { IB_TX_CAPTURE":1", "1" },
    { IB_TX_CAPTURE":2", "2" },
    { IB_TX_CAPTURE":3", "3" },
    { IB_TX_CAPTURE":4", "4" },
    { IB_TX_CAPTURE":5", "5" },
    { IB_TX_CAPTURE":6", "6" },
    { IB_TX_CAPTURE":7", "7" },
    { IB_TX_CAPTURE":8", "8" },
    { IB_TX_CAPTURE":9", "9" },
};

/**
 * Get the capture list, create if required.
 *
 * @param[in] tx Transaction
 * @param[out] olist If not NULL, pointer to the capture item's list
 *
 * @returns IB_OK: All OK
 *          IB_EINVAL: @a num is too large
 *          Error status from: ib_data_get()
 *                             ib_data_add_list()
 *                             ib_field_value()
 */
static
ib_status_t get_capture_list(
    ib_tx_t    *tx,
    ib_list_t **olist
)
{
    ib_status_t rc;
    ib_field_t *field = NULL;
    ib_list_t *list = NULL;

    assert(tx != NULL);
    assert(tx->data != NULL);

    /* Look up the capture list */
    rc = ib_data_get(tx->data, IB_TX_CAPTURE, &field);
    if (rc == IB_ENOENT) {
        rc = ib_data_add_list(tx->data, IB_TX_CAPTURE, &field);
    }
    if (rc != IB_OK) {
        return rc;
    }

    if (field->type != IB_FTYPE_LIST) {
        ib_data_remove(tx->data, IB_TX_CAPTURE, NULL);
    }
    rc = ib_field_mutable_value(field, ib_ftype_list_mutable_out(&list));
    if (rc != IB_OK) {
        return rc;
    }

    if (olist != NULL) {
        *olist = list;
    }

    return rc;
}

const char *ib_capture_name(int num)
{
    assert(num >= 0);

    if (num <= MAX_CAPTURE_NUM) {
        return names[num].name;
    }
    else {
        return "??";
    }
}

const char *ib_capture_fullname(int num)
{
    assert(num >= 0);

    if (num <= MAX_CAPTURE_NUM) {
        return names[num].full;
    }
    else {
        return IB_TX_CAPTURE":??";
    }
}

ib_status_t ib_capture_clear(ib_tx_t *tx)
{
    assert(tx != NULL);

    ib_status_t rc;
    ib_list_t *list;

    rc = get_capture_list(tx, &list);
    if (rc != IB_OK) {
        return rc;
    }
    ib_list_clear(list);
    return IB_OK;
}

ib_status_t ib_capture_set_item(
    ib_tx_t    *tx,
    int         num,
    ib_field_t *in_field
)
{
    assert(tx != NULL);
    assert(num >= 0);

    if (num > MAX_CAPTURE_NUM) {
        return IB_EINVAL;
    }

    ib_status_t rc;
    ib_list_t *list;
    ib_field_t *field;
    ib_list_node_t *node;
    ib_list_node_t *next;
    const char *name;

    name = ib_capture_name(num);

    rc = get_capture_list(tx, &list);
    if (rc != IB_OK) {
        return rc;
    }

    /* Remove any nodes with the same name */
    IB_LIST_LOOP_SAFE(list, node, next) {
        field = (ib_field_t *)node->data;
        if (strncmp(name, field->name, field->nlen) == 0) {
            ib_list_node_remove(list, node);
        }
    }
    field = NULL;

    if(in_field == NULL) {
        return IB_OK;
    }

    /* Make sure we have the correct name */
    if (strncmp(name, in_field->name, in_field->nlen) == 0) {
        field = in_field;
    }
    else {
        rc = ib_field_alias(&field, tx->mp, name, strlen(name), in_field);
        if (rc != IB_OK) {
            return rc;
        }

    }
    assert(field != NULL);

    /* Add the node to the list */
    rc = ib_list_push(list, field);

    return rc;
}
