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
static const size_t fmt_size_default = 256;   /**< Size of default format buf */
static const size_t fmt_pad_size = 64;        /**< Size of format padding */
static const size_t log_buf_size = 16 * 1024; /**< Size of log buffer */

void ib_engine_manager_logger(
    const ib_engine_t *ib,
    ib_log_level_t     level,
    const char        *file,
    int                line,
    const char        *fmt,
    va_list            ap,
    void              *cbdata
)
{
    assert(fmt != NULL);
    assert(cbdata != NULL);

    const ib_manager_t *manager = (const ib_manager_t *)cbdata;
    ib_log_level_t     logger_level;
    char               fmt_buf_default[fmt_size_default+1];
    char              *fmt_buf = NULL;
    char              *fmt_free = NULL;
    size_t             fmt_buf_size = 0;
    size_t             fmt_required;
    char              *log_buf = NULL;

    /* Use the engine's log level if available. */
    logger_level = ( (ib != NULL) ? ib_log_get_level(ib) : manager->log_level);

    /* Do nothing if the log level is sufficiently low */
    if (level > logger_level) {
        goto cleanup;
    }

    /* Add padding bytes to required size */
    fmt_required = strlen(fmt) + fmt_pad_size;
    if (fmt_required > fmt_size_default) {
        fmt_buf = (char *)malloc(fmt_required+1);
        if (fmt_buf == NULL) {
            goto cleanup;
        }
        fmt_free = fmt_buf;
    }
    else {
        fmt_buf = fmt_buf_default;
    }
    fmt_buf_size = fmt_required;
    snprintf(fmt_buf, fmt_buf_size, "%-10s- ", ib_log_level_to_string(level));

    /* Add the file name and line number if available and log level >= DEBUG */
    if ( (file != NULL) && (line > 0) && (logger_level >= IB_LOG_DEBUG)) {
        size_t              flen;
        static const size_t line_info_size = 35;
        char                line_info[line_info_size];

        while ( (file != NULL) && (strncmp(file, "../", 3) == 0) ) {
            file += 3;
        }
        flen = strlen(file);
        if (flen > 23) {
            file += (flen - 23);
        }

        snprintf(line_info, line_info_size, "(%23s:%-5d) ", file, line);
        strcat(fmt_buf, line_info);
    }
    strcat(fmt_buf, fmt);

    /* If we're using the va_list logger, use it */
    if (manager->log_va_fn != NULL) {
        manager->log_va_fn(level, manager->log_cbdata, fmt_buf, ap);
        goto cleanup;
    }

    /* Allocate the c buffer */
    log_buf = malloc(log_buf_size);
    if (log_buf == NULL) {
        manager->log_buf_fn(level, manager->log_cbdata,
                           "Failed to allocate message format buffer");
        goto cleanup;
    }

    /* Otherwise, we need to format into a buffer */
    vsnprintf(log_buf, log_buf_size, fmt_buf, ap);
    manager->log_buf_fn(level, manager->log_cbdata, log_buf);

cleanup:
    if (fmt_free != NULL) {
        free(fmt_free);
    }
    if (log_buf != NULL) {
        free(log_buf);
    }
}

void ib_manager_log_ex(
    const ib_manager_t *manager,
    ib_log_level_t      level,
    const char         *file,
    int                 line,
    const char         *fmt,
    ...
)
{
    assert(manager != NULL);
    va_list      ap;

    va_start(ap, fmt);
    ib_engine_manager_logger(NULL, level, file, line, fmt, ap, (void *)manager);
    va_end(ap);
}

void ib_manager_log_flush(
    const ib_manager_t *manager
)
{
    assert(manager != NULL);

    /* If there is a flush function, call it, otherwise do nothing */
    if (manager->log_flush_fn != NULL) {
        manager->log_flush_fn(manager->log_cbdata);
    }
}

void ib_manager_file_logger(
    void       *cbdata,
    const char *buf
)
{
    assert(buf != NULL);

    FILE *fp = (cbdata == NULL) ? stderr : (FILE *)cbdata;

    fputs(buf, fp);
    fputs("\n", fp);
}

void ib_manager_file_vlogger(
    void              *cbdata,
    const char        *fmt,
    va_list            ap
)
{
    assert(fmt != NULL);

    FILE *fp = (cbdata == NULL) ? stderr : (FILE *)cbdata;

    vfprintf(fp, fmt, ap);
    fputs("\n", fp);
}
