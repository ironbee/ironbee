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
 * @brief IronBee --- Engine Manager Logging
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "engine_manager_private.h"

#include <ironbee/config.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_manager.h>
#include <ironbee/list.h>
#include <ironbee/log.h>
#include <ironbee/mpool.h>
#include <ironbee/server.h>

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Logging buffer sizes */
static const size_t fmt_pad_size = 128;       /**< Size of format padding */
static const size_t log_buf_size = 16 * 1024; /**< Size of log buffer */

ib_status_t manager_logger_open(ib_logger_t *logger, void *data) {
    return IB_OK;
}

ib_status_t manager_logger_close(ib_logger_t *logger, void *data) {
    ib_manager_t *manager = (ib_manager_t *)data;

    manager->log_flush_fn(manager->log_flush_cbdata);
    return IB_OK;
}

ib_status_t manager_logger_reopen(ib_logger_t *logger, void *data) {
    ib_manager_t *manager = (ib_manager_t *)data;

    manager->log_flush_fn(manager->log_flush_cbdata);
    return IB_OK;
}

ib_status_t manager_logger_format(
    ib_logger_t           *logger,
    const ib_logger_rec_t *rec,
    const uint8_t         *msg,
    const size_t           msg_sz,
    void                  *writer_record,
    void                  *data
)
{
    assert(data != NULL);

    ib_status_t               rc = IB_OK;

    /* Clang analyzer cannot follow record through a void* and so
     * gives a false positive on a memory leak. */
#ifndef __clang_analyzer__

    ib_logger_standard_msg_t   *std_msg = NULL;
    size_t                      prefix_sz;

    /* Format the message into the standard message struct. */
    rc = ib_logger_standard_formatter(
        logger,
        rec,
        msg,
        msg_sz,
        &std_msg,
        data);

    if (rc != IB_OK) {
        return rc;
    }

    ib_manager_logger_record_t *manager_logger_record;

    /* Build a log record to populate. */
    manager_logger_record = malloc(sizeof(*manager_logger_record));
    if (manager_logger_record == NULL) {
        ib_logger_standard_msg_free(std_msg);
        return IB_EALLOC;
    }

    prefix_sz = strlen(std_msg->prefix);

    manager_logger_record->msg_sz = prefix_sz + std_msg->msg_sz;
    manager_logger_record->msg = malloc(manager_logger_record->msg_sz + 1);
    if (manager_logger_record->msg == NULL) {
        free(manager_logger_record);
        ib_logger_standard_msg_free(std_msg);
        return IB_EALLOC;
    }

    memcpy(
        (uint8_t *)(manager_logger_record->msg),
        std_msg->prefix,
        prefix_sz);
    memcpy(
        (uint8_t *)(manager_logger_record->msg + prefix_sz),
        std_msg->msg,
        std_msg->msg_sz);
    *(uint8_t *)(manager_logger_record->msg + manager_logger_record->msg_sz) =
        '\0';

    manager_logger_record->level  = rec->level;

    /* Return the formatted log message. */
    *(void **)writer_record = manager_logger_record;

#endif

    return rc;
}

static void write_log_record(void *record, void *cbdata)
{
    assert(record != NULL);
    assert(cbdata != NULL);

    ib_manager_t *manager = (ib_manager_t *)cbdata;
    ib_manager_logger_record_t *rec = (ib_manager_logger_record_t *)record;

    manager->log_buf_fn(rec, manager->log_buf_cbdata);
    free((void *)rec->msg);
    free(rec);
}

ib_status_t manager_logger_record(
    ib_logger_t        *logger,
    ib_logger_writer_t *writer,
    void *data
)
{
    return ib_logger_dequeue(logger, writer, write_log_record, data);
}

void ib_manager_log_flush(
    const ib_manager_t *manager
)
{
    assert(manager != NULL);

    /* If there is a flush function, call it, otherwise do nothing */
    if (manager->log_flush_fn != NULL) {
        manager->log_flush_fn(manager->log_flush_cbdata);
    }
}

void DLL_LOCAL ib_manager_log_ex(
    ib_manager_t       *manager,
    ib_logger_level_t   level,
    const char         *file,
    const char         *func,
    int                 line,
    ib_log_call_data_t *calldata,
    const char         *fmt,
    ...
)
{
    assert(manager != NULL);
    assert(manager->logger != NULL);

    va_list      ap;

    if (manager->log_buf_fn != NULL) {
        const size_t               msg_sz_mx = 1024;
        ib_manager_logger_record_t rec = {
            .level  = level,
            .msg    = NULL,
            .msg_sz = 0
        };

        rec.msg = malloc(msg_sz_mx);
        if (rec.msg == NULL) {
            return;
        }

        va_start(ap, fmt);
        rec.msg_sz = vsnprintf((char *)rec.msg, msg_sz_mx, fmt, ap);
        va_end(ap);

        manager->log_buf_fn(&rec, manager->log_buf_cbdata);

        if (manager->log_flush_fn != NULL) {
            manager->log_flush_fn(manager->log_flush_cbdata);
        }

        free((void *)rec.msg);
    }

}
