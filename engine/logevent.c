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
 * @brief IronBee --- Logger
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/logevent.h>
#include <ironbee/mm_mpool_lite.h>
#include <ironbee/state_notify.h>

#include <assert.h>
#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <stdio.h>

/** Log Event Types */
static const char *ib_logevent_type_str[] = {
    "Unknown",
    "Observation",
    "Alert",
    NULL
};

/** Log Event Action Names */
static const char *ib_logevent_action_str[] = {
    "NoAction",
    "Log",
    "Block",
    "Ignore",
    "Allow",
    NULL
};

/** Log Event Action Names */
static const char *ib_logevent_suppress_str[] = {
    "None",
    "FalsePositive",
    "Replaced",
    "Incomplete",
    "Other",
    NULL
};

const char *ib_logevent_type_name(ib_logevent_type_t num)
{
    if (
        (unsigned long)num >=
        (sizeof(ib_logevent_type_str) / sizeof(const char *))
    ) {
        return ib_logevent_type_str[0];
    }
    return ib_logevent_type_str[num];
}

const char *ib_logevent_action_name(ib_logevent_action_t num)
{
    if (
        (unsigned long)num >=
        (sizeof(ib_logevent_action_str) / sizeof(const char *))
    ) {
        return ib_logevent_action_str[0];
    }
    return ib_logevent_action_str[num];
}

const char *ib_logevent_suppress_name(ib_logevent_suppress_t num)
{
    if (
        (unsigned long)num >=
        (sizeof(ib_logevent_suppress_str) / sizeof(const char *))
    ) {
        return ib_logevent_suppress_str[0];
    }
    return ib_logevent_suppress_str[num];
}

ib_status_t ib_logevent_create(ib_logevent_t **ple,
                               ib_mm_t mm,
                               const char *rule_id,
                               ib_logevent_type_t type,
                               ib_logevent_action_t rec_action,
                               uint8_t confidence,
                               uint8_t severity,
                               const char *fmt,
                               ...)
{
    assert(ple     != NULL);
    assert(rule_id != NULL);
    assert(fmt     != NULL);
    /*
     * Defined so that size_t to int cast is avoided
     * checking the result of vsnprintf below.
     * NOTE: This is assumed >3 bytes and should not
     *       be overly large as it is used as the size
     *       of a stack buffer.
     */
#define IB_LEVENT_MSG_BUF_SIZE 1024

    ib_mpool_lite_t *mpl;
    ib_mm_t mpl_mm;
    char *buf;
    va_list ap;
    int len = 0;
    ib_status_t rc;

    rc = ib_mpool_lite_create(&mpl);
    if (rc != IB_OK) {
        return rc;
    }
    mpl_mm = ib_mm_mpool_lite(mpl);

    buf = ib_mm_alloc(mpl_mm, IB_LEVENT_MSG_BUF_SIZE);
    if (buf == NULL) {
        rc = IB_EALLOC;
        goto return_rc;
    }

    *ple = (ib_logevent_t *)ib_mm_calloc(mm, 1, sizeof(**ple));
    if (*ple == NULL) {
        rc = IB_EALLOC;
        goto return_rc;
    }

    (*ple)->event_id   = (uint32_t)ib_clock_get_time(); /* truncated */
    (*ple)->mm         = mm;
    (*ple)->rule_id    = ib_mm_strdup(mm, rule_id);
    (*ple)->type       = type;
    (*ple)->rec_action = rec_action;
    (*ple)->confidence = confidence;
    (*ple)->severity   = severity;
    (*ple)->suppress   = IB_LEVENT_SUPPRESS_NONE;

    if ((*ple)->rule_id == NULL) {
        rc = IB_EALLOC;
        goto return_rc;
    }

    rc = ib_list_create(&((*ple)->tags), mm);
    if (rc != IB_OK) {
        goto return_rc;
    }

    /*
     * Generate the message, replacing the last three characters
     * with "..." if truncation is required.
     */
    va_start(ap, fmt);
    len = vsnprintf(buf, IB_LEVENT_MSG_BUF_SIZE, fmt, ap);
    if (len >= IB_LEVENT_MSG_BUF_SIZE) {
        memcpy(buf + (IB_LEVENT_MSG_BUF_SIZE - 4), "...", 3);
    }
    va_end(ap);

    /* Copy the formatted message. */
    (*ple)->msg = ib_mm_strdup(mm, buf);

return_rc:
    ib_mpool_lite_destroy(mpl);
    return rc;
}

ib_status_t ib_logevent_tag_add(ib_logevent_t *le,
                                const char *tag)
{
    assert(le != NULL);
    assert(le->tags != NULL);

    char *tag_copy;
    ib_status_t rc;

    tag_copy = ib_mm_memdup(le->mm, tag, strlen(tag) + 1);
    if (tag_copy == NULL) {
        return IB_EALLOC;
    }

    rc = ib_list_push(le->tags, tag_copy);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_logevent_data_set(ib_logevent_t *le,
                                 const void *data,
                                 size_t dlen)
{
    assert(le != NULL);

    // TODO Copy the data???
    le->data = data;
    le->data_len = dlen;

    return IB_OK;
}


ib_status_t ib_logevent_add(ib_tx_t       *tx,
                            ib_logevent_t *e)
{
    ib_status_t rc;

    if (tx == NULL) {
        return IB_EINVAL;
    }

    rc = ib_list_push(tx->logevents, e);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_engine_notify_logevent(tx->ib, tx, e);

    return rc;
}

ib_status_t ib_logevent_remove(ib_tx_t *tx,
                               uint32_t id)
{
    if (tx == NULL) {
        return IB_EINVAL;
    }

    ib_list_node_t *node;
    ib_list_node_t *node_next;

    IB_LIST_LOOP_SAFE(tx->logevents, node, node_next) {
        ib_logevent_t *e = (ib_logevent_t *)ib_list_node_data(node);
        if (e->event_id == id) {
            ib_status_t rc;
            ib_list_node_remove(tx->logevents, node);
            rc = ib_engine_notify_logevent(tx->ib, tx, e);
            return rc;
        }
    }

    return IB_ENOENT;
}

ib_status_t ib_logevent_get_all(
    ib_tx_t    *tx,
    ib_list_t **pevents)
{
    if (tx == NULL) {
        return IB_EINVAL;
    }

    *pevents = tx->logevents;
    return IB_OK;
}

ib_status_t ib_logevent_get_last(
    ib_tx_t        *tx,
    ib_logevent_t **event
)
{
    assert(tx != NULL);

    ib_list_node_t *node;

    if (tx->logevents == NULL) {
        return IB_ENOENT;
    }

    node = ib_list_last(tx->logevents);

    if (node == NULL) {
        return IB_ENOENT;
    }

    *event = (ib_logevent_t *)ib_list_node_data(node);
    return IB_OK;
}

ib_status_t ib_logevent_write_all(
    ib_tx_t   *tx)
{
    if (tx == NULL) {
        return IB_EINVAL;
    }

    if (tx->logevents == NULL) {
        return IB_OK;
    }

    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_logevent_suppress_set(
    ib_logevent_t          *le,
    ib_logevent_suppress_t  suppress)
{
    if (le == NULL) {
        return IB_EINVAL;
    }
    le->suppress = suppress;
    return IB_OK;
}
