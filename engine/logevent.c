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

#include <assert.h>
#include <stdio.h>

/** Log Event Types */
const char *ib_logevent_type_str[] = {
    "Unknown",
    "Observation",
    "Alert",
    NULL
};

/** Log Event Action Names */
static const char *ib_logevent_action_str[] = {
    "Unknown",
    "Log",
    "Block",
    "Ignore",
    "Allow",
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

ib_status_t DLL_PUBLIC ib_logevent_create(ib_logevent_t **ple,
                                          ib_mpool_t *pool,
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
    int r = 0;

    *ple = (ib_logevent_t *)ib_mpool_calloc(pool, 1, sizeof(**ple));
    if (*ple == NULL) {
        return IB_EALLOC;
    }

    (*ple)->event_id = (uint32_t)ib_clock_get_time(); /* truncated */
    (*ple)->mp = pool;
    (*ple)->rule_id = ib_mpool_strdup(pool, rule_id);
    (*ple)->type = type;
    (*ple)->rec_action = rec_action;
    (*ple)->confidence = confidence;
    (*ple)->severity = severity;
    (*ple)->suppress = IB_LEVENT_SUPPRESS_NONE;

    /*
     * Generate the message, replacing the last three characters
     * with "..." if truncation is required.
     */
    va_start(ap, fmt);
    r = vsnprintf(buf, IB_LEVENT_MSG_BUF_SIZE, fmt, ap);
    if (r >= IB_LEVENT_MSG_BUF_SIZE) {
        memcpy(buf + (IB_LEVENT_MSG_BUF_SIZE - 4), "...", 3);
    }
    va_end(ap);

    /* Copy the formatted message. */
    (*ple)->msg = ib_mpool_strdup(pool, buf);

    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_logevent_tag_add(ib_logevent_t *le,
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

ib_status_t DLL_PUBLIC ib_logevent_field_add(ib_logevent_t *le,
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

ib_status_t DLL_PUBLIC ib_logevent_field_add_ex(ib_logevent_t *le,
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

ib_status_t DLL_PUBLIC ib_logevent_data_set(ib_logevent_t *le,
                                            const void *data,
                                            size_t dlen)
{
    assert(le != NULL);

    // TODO Copy the data???
    le->data = data;
    le->data_len = dlen;

    return IB_OK;
}


ib_status_t ib_logevent_add(ib_provider_inst_t *pi,
                         ib_logevent_t *e)
{
    IB_PROVIDER_API_TYPE(logevent) *api;
    ib_status_t rc;

    if (pi == NULL) {
        return IB_EINVAL;
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)pi->pr->api;

    rc = api->add_event(pi, e);
    return rc;
}

ib_status_t ib_logevent_remove(ib_provider_inst_t *pi,
                            uint32_t id)
{
    IB_PROVIDER_API_TYPE(logevent) *api;
    ib_status_t rc;

    if (pi == NULL) {
        return IB_EINVAL;
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)pi->pr->api;

    rc = api->remove_event(pi, id);
    return rc;
}

ib_status_t ib_logevent_get_all(ib_provider_inst_t *pi,
                             ib_list_t **pevents)
{
    IB_PROVIDER_API_TYPE(logevent) *api;
    ib_status_t rc;

    if (pi == NULL) {
        return IB_EINVAL;
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)pi->pr->api;

    rc = api->fetch_events(pi, pevents);
    return rc;
}

ib_status_t ib_logevent_write_all(ib_provider_inst_t *pi)
{
    IB_PROVIDER_API_TYPE(logevent) *api;
    ib_status_t rc;

    if (pi == NULL) {
        return IB_EINVAL;
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)pi->pr->api;

    rc = api->write_events(pi);
    return rc;
}
