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

#include <ironbee/log.h>

#include "engine_private.h"

#include <ironbee/string.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/**
 * Engine default logger.
 *
 * This is the default logger that executes when no other logger has
 * been configured.
 *
 * @param fp File pointer.
 * @param level Log level.
 * @param ib IronBee engine.
 * @param file Source filename.
 * @param line Source line number.
 * @param fmt Formatting string.
 * @param ap Variable argument list.
 */
static void default_logger(FILE *fp, ib_log_level_t level,
                           const ib_engine_t *ib,
                           const char *file, int line,
                           const char *fmt, va_list ap)
{
    char *new_fmt;
    char time_info[32 + 1];
    struct tm *tminfo;
    time_t timet;

    if (level > 4) {
        return;
    }

    timet = time(NULL);
    tminfo = localtime(&timet);
    strftime(time_info, sizeof(time_info)-1, "%d%m%Y.%Hh%Mm%Ss", tminfo);

    /* 100 is more than sufficient. */
    new_fmt = (char *)malloc(strlen(time_info) + strlen(fmt) + 100);
    sprintf(new_fmt, "%s %-10s- ", time_info, ib_log_level_to_string(level));

    if ( (file != NULL) && (line > 0) ) {
        size_t flen;
        while (strncmp(file, "../", 3) == 0) {
            file += 3;
        }
        flen = strlen(file);
        if (flen > 23) {
            file += (flen - 23);
        }

        static const size_t c_line_info_length = 35;
        char line_info[c_line_info_length];
        snprintf(
            line_info,
            c_line_info_length,
            "(%23s:%-5d) ",
            file,
            line
        );
        strcat(new_fmt, line_info);
    }

    strcat(new_fmt, fmt);
    strcat(new_fmt, "\n");

    vfprintf(fp, new_fmt, ap);
    fflush(fp);

    free(new_fmt);

    return;
}

static const char* c_log_levels[] = {
    "EMERGENCY",
    "ALERT",
    "CRITICAL",
    "ERROR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG",
    "DEBUG2",
    "DEBUG3",
    "TRACE"
};
static size_t c_num_levels = sizeof(c_log_levels)/sizeof(*c_log_levels);

void ib_log_set_logger_fn(
    ib_engine_t        *ib,
    ib_log_logger_fn_t  logger,
    void               *cbdata
)
{
    assert(ib != NULL);
    assert(logger != NULL);

    ib->logger_fn = logger;
    ib->logger_cbdata = cbdata;
}

void ib_log_set_loglevel_fn(
    ib_engine_t       *ib,
    ib_log_level_fn_t  log_level,
    void              *cbdata
)
{
    assert(ib != NULL);
    assert(log_level != NULL);

    ib->loglevel_fn = log_level;
    ib->loglevel_cbdata = cbdata;
}

ib_log_level_t ib_log_string_to_level(
    const char     *s,
    ib_log_level_t  dlevel
)
{
    unsigned int i;
    ib_num_t     level;

    /* First, if it's a number, just do a numeric conversion */
    if (ib_string_to_num(s, 10, &level) == IB_OK) {
        return (ib_log_level_t)level;
    }

    /* Now, string compare to level names */
    for (i = 0; i < c_num_levels; ++i) {
        if (
            strncasecmp(s, c_log_levels[i], strlen(c_log_levels[i])) == 0 &&
            strlen(s) == strlen(c_log_levels[i])
        ) {
            return i;
        }
    }

    /* No match, return the default */
    return dlevel;
}

const char *ib_log_level_to_string(ib_log_level_t level)
{
    if (level < c_num_levels) {
        return c_log_levels[level];
    }
    else {
        return "UNKNOWN";
    }
}

void DLL_PUBLIC ib_log_ex(
    const ib_engine_t *ib,
    ib_log_level_t     level,
    const char        *file,
    int                line,
    const char        *fmt,
    ...
)
{
    va_list ap;
    va_start(ap, fmt);

    ib_log_vex_ex(ib, level, file, line, fmt, ap);

    va_end(ap);

    return;
}

void DLL_PUBLIC ib_log_tx_ex(
     const ib_tx_t  *tx,
     ib_log_level_t  level,
     const char     *file,
     int             line,
     const char     *fmt,
     ...
)
{
    va_list ap;
    va_start(ap, fmt);

    ib_log_tx_vex(tx, level, file, line, fmt, ap);

    va_end(ap);

    return;
}

void DLL_PUBLIC ib_log_tx_vex(
     const ib_tx_t  *tx,
     ib_log_level_t  level,
     const char     *file,
     int             line,
     const char     *fmt,
     va_list         ap
)
{
    char *new_fmt = malloc(strlen(fmt) + 45);
    const char *which_fmt = new_fmt;
    if (! new_fmt) {
        /* Do our best */
        which_fmt = fmt;
    }
    else {
        sprintf(new_fmt, "[tx:%.36s] ", tx->id);
        strcat(new_fmt, fmt);
    }

    ib_log_vex_ex(tx->ib, level, file, line, which_fmt, ap);

    free(new_fmt);

    return;
}

void DLL_PUBLIC ib_log_vex_ex(
    const ib_engine_t *ib,
    ib_log_level_t     level,
    const char        *file,
    int                line,
    const char        *fmt,
    va_list            ap
)
{
    /* Check the log level, return if we're not interested. */
    ib_log_level_t logger_level = ib_log_get_level(ib);
    if (level > logger_level) {
        return;
    }

    if (ib->logger_fn != NULL) {
        ib->logger_fn(ib, level, file, line, fmt, ap, ib->logger_cbdata);
    }
    else {
        default_logger(stderr, level, ib, file, line, fmt, ap);
    }
}

ib_log_level_t DLL_PUBLIC ib_log_get_level(const ib_engine_t *ib)
{
    if (ib->loglevel_fn != NULL) {
        return ib->loglevel_fn(ib, ib->loglevel_cbdata);
    }
    else {
        return IB_LOG_TRACE;
    }
}
