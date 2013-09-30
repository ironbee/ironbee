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
    const char        *func,
    int                line,
    const char        *fmt,
    ...
)
{
    va_list ap;
    va_start(ap, fmt);

    ib_log_vex_ex(ib, level, file, func, line, fmt, ap);

    va_end(ap);

    return;
}

void DLL_PUBLIC ib_log_tx_ex(
     const ib_tx_t  *tx,
     ib_log_level_t  level,
     const char     *file,
     const char     *func,
     int             line,
     const char     *fmt,
     ...
)
{
    va_list ap;
    va_start(ap, fmt);

    ib_log_tx_vex(tx, level, file, func, line, fmt, ap);

    va_end(ap);

    return;
}

void DLL_PUBLIC ib_log_tx_vex(
     const ib_tx_t  *tx,
     ib_log_level_t  level,
     const char     *file,
     const char     *func,
     int             line,
     const char     *fmt,
     va_list         ap
)
{
    assert(tx != NULL);
    assert(tx->ib != NULL);

    ib_logger_log_va_list(
        ib_engine_logger_get(tx->ib),
        file,
        func,
        (int)line,
        tx->ib,
        NULL,
        tx->conn,
        tx,
        level,
        fmt,
        ap
    );

    return;
}

void DLL_PUBLIC ib_log_vex_ex(
    const ib_engine_t *ib,
    ib_log_level_t     level,
    const char        *file,
    const char        *func,
    int                line,
    const char        *fmt,
    va_list            ap
)
{
    ib_logger_log_va_list(
        ib_engine_logger_get(ib),
        file,
        func,
        (int)line,
        ib,
        NULL,
        NULL,
        NULL,
        level,
        fmt,
        ap
    );
}
