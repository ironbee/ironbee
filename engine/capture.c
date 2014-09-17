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
#include <ironbee/engine.h>
#include <ironbee/log.h>
#include <ironbee/mm.h>

#include <assert.h>
#include <stdio.h>

#define UNKNOWN_CAPTURE_NAME "??"
static const int MAX_CAPTURE_NUM = 9;
typedef struct {
    const char *full;
    const char *name;
} default_capture_names_t;
static const default_capture_names_t default_names[] =
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
 * Use the default capture collection?
 *
 * @param[in] collection_name The name of the capture collection or NULL
 *
 * @returns Boolean value:
 *  - true if @a is NULL or matches the default collection,
 *  - false if neither of the above is true
 */
static bool use_default_collection(
    const char *collection_name)
{
    if (collection_name == NULL) {
        return true;
    }
    if (strcasecmp(collection_name, IB_TX_CAPTURE) == 0) {
        return true;
    }
    return false;
}

/**
 * Get capture collection name
 *
 * @param[in] collection_name The name of the capture collection or NULL
 *
 * @returns Collection name string
 */
static inline const char *get_collection_name(
    const char *collection_name)
{
    if (collection_name == NULL) {
        collection_name = IB_TX_CAPTURE;
    }
    return collection_name;
}

/**
 * Get the capture list from a field.
 *
 * @param[in] capture Field to get list from.
 * @param[out] olist If not NULL, pointer to the capture item's list
 *
 * @returns
 * - IB_OK: All OK
 * - Other if can't fetch field value.
 */
static
ib_status_t get_capture_list(
    ib_field_t  *capture,
    ib_list_t  **olist
)
{
    ib_status_t rc;
    ib_list_t *list = NULL;

    assert(capture != NULL);
    assert(capture->type == IB_FTYPE_LIST);

    rc = ib_field_mutable_value(capture, ib_ftype_list_mutable_out(&list));
    if (rc != IB_OK) {
        return rc;
    }

    if (olist != NULL) {
        *olist = list;
    }

    return rc;
}

ib_status_t ib_capture_acquire(
    const ib_tx_t  *tx,
    const char     *collection_name,
    ib_field_t    **field
)
{
    assert(tx != NULL);
    assert(tx->var_store != NULL);

    ib_status_t rc;
    ib_var_source_t *source;

    /* Look up the capture list */
    collection_name = get_collection_name(collection_name);
    // @todo Acquire source at configuration time.
    rc = ib_var_source_acquire(&source,
        tx->mm,
        ib_engine_var_config_get(tx->ib),
        collection_name, strlen(collection_name)
    );
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_var_source_get(source, field, tx->var_store);
    if (
        rc == IB_ENOENT ||
        (rc == IB_OK && (*field)->type != IB_FTYPE_LIST)
    ) {
        rc = ib_var_source_initialize(
            source,
            field,
            tx->var_store,
            IB_FTYPE_LIST
        );
    }
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

const char *ib_capture_name(
    int         num)
{
    assert(num >= 0);

    if (num <= MAX_CAPTURE_NUM && num >= 0) {
        return default_names[num].name;
    }
    else {
        return UNKNOWN_CAPTURE_NAME;
    }
}

const char *ib_capture_fullname(
    const ib_tx_t *tx,
    const char    *collection_name,
    int            num
)
{
    assert(tx != NULL);
    assert(num >= 0);
    size_t len;
    char *buf;

    if (num < 0) {
        return UNKNOWN_CAPTURE_NAME;
    }

    /* Use the default collection? */
    if (use_default_collection(collection_name)) {
        if (num <= MAX_CAPTURE_NUM) {
            return default_names[num].full;
        }
        else {
            return IB_TX_CAPTURE ":" UNKNOWN_CAPTURE_NAME;
        }
    }

    /* Non-default, build the name dynamically */
    len = strlen(collection_name) + 4; /* name + ':' + digit + '\0'*/
    buf = ib_mm_alloc(tx->mm, len);
    if (buf == NULL) {
        return NULL;
    }
    if (num <= MAX_CAPTURE_NUM) {
        snprintf(buf, len, "%s:%d", collection_name, num);
    }
    else {
        strcpy(buf, collection_name);
        strcat(buf, ":" UNKNOWN_CAPTURE_NAME);
    }
    return buf;
}

ib_status_t ib_capture_clear(ib_field_t *capture)
{
    assert(capture != NULL);

    ib_status_t rc;
    ib_list_t *list;

    rc = get_capture_list(capture, &list);
    if (rc != IB_OK) {
        return rc;
    }
    ib_list_clear(list);
    return IB_OK;
}

ib_status_t ib_capture_set_item(
    ib_field_t *capture,
    int         num,
    ib_mm_t     mm,
    const ib_field_t *in_field
)
{
    assert(capture != NULL);
    assert(num >= 0);

    if (num > MAX_CAPTURE_NUM || num < 0) {
        return IB_EINVAL;
    }

    ib_status_t rc;
    ib_field_t *field;
    ib_list_t *list;
    ib_list_node_t *node;
    ib_list_node_t *next;
    const char *name;

    name = ib_capture_name(num);

    rc = get_capture_list(capture, &list);
    if (rc != IB_OK) {
        return rc;
    }

    /* Remove any nodes with the same name */
    IB_LIST_LOOP_SAFE(list, node, next) {
        ib_field_t *tmp_field = (ib_field_t *)node->data;
        if (strncmp(name, tmp_field->name, tmp_field->nlen) == 0) {
            ib_list_node_remove(list, node);
        }
    }

    if(in_field == NULL) {
        return IB_OK;
    }

    /* Make sure we have the correct name. If we do, add the field... */
    if (strncmp(name, in_field->name, in_field->nlen) == 0) {
        /* Add the node to the list */
        rc = ib_list_push(list, (void *)in_field);
    }

    /* ... else, alias to the proper name. */
    else {
        rc = ib_field_alias(&field, mm, name, strlen(name), in_field);
        if (rc != IB_OK) {
            return rc;
        }
        assert(field != NULL);
        /* Add the node to the list */
        rc = ib_list_push(list, field);
    }

    return rc;
}

ib_status_t ib_capture_add_item(
    ib_field_t *capture,
    ib_field_t *in_field
)
{
    assert(capture != NULL);

    ib_status_t rc;
    ib_list_t *list;

    rc = get_capture_list(capture, &list);
    if (rc != IB_OK) {
        return rc;
    }

    /* Add the node to the list */
    rc = ib_list_push(list, in_field);

    return rc;
}
