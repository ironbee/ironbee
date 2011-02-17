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
 * @brief IronBee - Logger
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>


#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/provider.h>

#include "ironbee_private.h"


/* -- Internal Routines -- */

/**
 * Engine default logger.
 *
 * This is the default logger that executes when no other logger has
 * been configured.
 *
 * @param fp File pointer
 * @param level Log level
 * @param prefix Optional prefix to the log
 * @param file Optional source filename (or NULL)
 * @param line Optional source line number (or 0)
 * @param fmt Formatting string
 * @param ap Variable argument list
 */
static void default_logger(FILE *fp, int level,
                           const char *prefix, const char *file, int line,
                           const char *fmt, va_list ap)
{
    IB_FTRACE_INIT(default_logger);
    char fmt2[1024 + 1];

    if ((file != NULL) && (line > 0)) {
        int ec = snprintf(fmt2, 1024,
                          "%s[%d] (%s:%d) %s\n",
                          (prefix?prefix:""), level, file, line, fmt);
        if (ec > 1024) {
            /// @todo Do something better
            abort();
        }
    }
    else {
        int ec = snprintf(fmt2, 1024,
                          "%s[%d] %s\n",
                          (prefix?prefix:""), level, fmt);
        if (ec > 1024) {
            /// @todo Do something better
            abort();
        }
    }

    vfprintf(fp, fmt2, ap);
    fflush(fp);
    IB_FTRACE_RET_VOID();
}


/* -- Exported Logging Routines -- */

ib_provider_inst_t *ib_log_provider_get_instance(ib_context_t *ctx)
{
    return ctx->logger;
}

void ib_log_provider_set_instance(ib_context_t *ctx, ib_provider_inst_t *pi)
{
    IB_FTRACE_INIT(ib_log_provider_set_instance);
    ctx->logger = pi;
    IB_FTRACE_RET_VOID();
}

void ib_clog_ex(ib_context_t *ctx, int level,
                const char *prefix, const char *file, int line,
                const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    ib_vclog_ex(ctx, level, prefix, file, line, fmt, ap);
    va_end(ap);
}

void ib_vclog_ex(ib_context_t *ctx, int level,
                 const char *prefix, const char *file, int line,
                 const char *fmt, va_list ap)
{
    IB_PROVIDER_API_TYPE(logger) *api;

    if (ctx->logger != NULL) {
        //default_logger(stderr, level, prefix, file, line, fmt, ap);
        api = (IB_PROVIDER_API_TYPE(logger) *)ctx->logger->pr->api;

        /// @todo Change to use ib_provider_inst_t
        api->vlogmsg(ctx->logger, ctx, level, prefix, file, line, fmt, ap);
    }
    else {
        default_logger(stderr, level, prefix, file, line, fmt, ap);
    }
}

