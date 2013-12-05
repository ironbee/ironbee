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
                               ib_mpool_t *mp,
                               const char *rule_id,
                               ib_logevent_type_t type,
                               ib_logevent_action_t rec_action,
                               uint8_t confidence,
                               uint8_t severity,
                               const char *fmt,
                               ...)
{
    /*
     * Defined so that size_t to int cast is avoided
     * checking the result of vsnprintf below.
     * NOTE: This is assumed >3 bytes and should not
     *       be overly large as it is used as the size
     *       of a stack buffer.
     */
#define IB_LEVENT_MSG_BUF_SIZE 1024

    char buf[IB_LEVENT_MSG_BUF_SIZE];
    va_list ap;
    int len = 0;
    ib_status_t rc;

    *ple = (ib_logevent_t *)ib_mpool_calloc(mp, 1, sizeof(**ple));
    if (*ple == NULL) {
        return IB_EALLOC;
    }

    (*ple)->event_id = (uint32_t)ib_clock_get_time(); /* truncated */
    (*ple)->mp = mp;
    (*ple)->rule_id = ib_mpool_strdup(mp, rule_id);
    (*ple)->type = type;
    (*ple)->rec_action = rec_action;
    (*ple)->confidence = confidence;
    (*ple)->severity = severity;
    (*ple)->suppress = IB_LEVENT_SUPPRESS_NONE;

    rc = ib_list_create(&((*ple)->tags), mp);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_list_create(&((*ple)->fields), mp);
    if (rc != IB_OK) {
        return rc;
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
    (*ple)->msg = ib_mpool_strdup(mp, buf);

    return IB_OK;
}

ib_status_t ib_logevent_tag_add(ib_logevent_t *le,
                                const char *tag)
{
    char *tag_copy;
    ib_status_t rc;

    assert(le != NULL);

    if (le->tags == NULL) {
        rc = ib_list_create(&le->tags, le->mp);
        if (rc != IB_OK) {
            return rc;
        }
    }

    tag_copy = ib_mpool_memdup(le->mp, tag, strlen(tag) + 1);
    rc = ib_list_push(le->tags, tag_copy);

    return rc;
}

ib_status_t ib_logevent_field_add(ib_logevent_t *le,
                                  const char *name)
{
    char *name_copy;
    ib_status_t rc;

    assert(le != NULL);

    if (le->fields == NULL) {
        rc = ib_list_create(&le->fields, le->mp);
        if (rc != IB_OK) {
            return rc;
        }
    }

    name_copy = ib_mpool_memdup(le->mp, name, strlen(name) + 1);
    rc = ib_list_push(le->fields, name_copy);

    return rc;
}

ib_status_t ib_logevent_field_add_ex(ib_logevent_t *le,
                                     const char *name,
                                     size_t nlen)
{
    char *name_copy;
    ib_status_t rc;

    assert(le != NULL);

    if (le->fields == NULL) {
        rc = ib_list_create(&le->fields, le->mp);
        if (rc != IB_OK) {
            return rc;
        }
    }

    name_copy = ib_mpool_memdup_to_str(le->mp, name, nlen);
    rc = ib_list_push(le->fields, name_copy);

    return rc;
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

    rc = ib_state_notify_logevent(tx->ib, tx);

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
            rc = ib_state_notify_logevent(tx->ib, tx);
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
