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

#include <sys/time.h> /// @todo Temp for gettimeofday()


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


/* -- Log Event Routines -- */

ib_status_t ib_logevent_create(ib_logevent_t **ple,
                               ib_mpool_t *pool,
                               uint8_t type,
                               uint8_t activity,
                               uint8_t pri_cat,
                               uint8_t sec_cat,
                               uint8_t confidence,
                               uint8_t severity,
                               uint8_t sys_env,
                               uint8_t rec_action,
                               const char *fmt,
                               ...)
{
    IB_FTRACE_INIT(ib_logevent_create);
    char buf[8192];
    struct timeval tv;
    va_list ap;

    *ple = (ib_logevent_t *)ib_mpool_calloc(pool, 1, sizeof(**ple));
    if (*ple == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /// @todo Need a true unique id generator
    gettimeofday(&tv, NULL);
    (*ple)->id = (tv.tv_sec << 32) + tv.tv_usec;

    /// @todo Generate the remaining portions of the event

    (*ple)->mp = pool;
    (*ple)->type = type;
    (*ple)->activity = activity;
    (*ple)->pri_cat = pri_cat;
    (*ple)->sec_cat = sec_cat;
    (*ple)->confidence = confidence;
    (*ple)->severity = severity;
    (*ple)->sys_env = sys_env;
    (*ple)->rec_action = rec_action;

    va_start(ap, fmt);
    if (vsnprintf(buf, sizeof(buf), fmt, ap) >= (int)sizeof(buf)) {
        strcpy(buf, "<msg too long>");
    }
    va_end(ap);

    /* Copy the formatted message. */
    (*ple)->msg = (char *)ib_mpool_memdup(pool, buf, strlen(buf) + 1);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_provider_inst_t *ib_logevent_provider_get_instance(ib_context_t *ctx)
{
    return ctx->logevent;
}

void ib_logevent_provider_set_instance(ib_context_t *ctx, ib_provider_inst_t *pi)
{
    IB_FTRACE_INIT(ib_logevent_provider_set_instance);
    ctx->logevent = pi;
    IB_FTRACE_RET_VOID();
}


/* -- Audit Log Routines -- */

ib_provider_inst_t *ib_audit_provider_get_instance(ib_context_t *ctx)
{
    return ctx->audit;
}

void ib_audit_provider_set_instance(ib_context_t *ctx, ib_provider_inst_t *pi)
{
    IB_FTRACE_INIT(ib_audit_provider_set_instance);
    ctx->audit = pi;
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

ib_status_t ib_clog_event(ib_context_t *ctx,
                          ib_logevent_t *e)
{
    IB_PROVIDER_API_TYPE(logevent) *api;

    api = (IB_PROVIDER_API_TYPE(logevent) *)ctx->logevent->pr->api;
    return api->add_event(ctx->logevent, e);
}

ib_status_t ib_clog_event_remove(ib_context_t *ctx,
                                 uint64_t id)
{
    IB_PROVIDER_API_TYPE(logevent) *api;

    api = (IB_PROVIDER_API_TYPE(logevent) *)ctx->logevent->pr->api;
    return api->remove_event(ctx->logevent, id);
}

ib_status_t ib_clog_events_get(ib_context_t *ctx,
                               ib_list_t **pevents)
{
    IB_PROVIDER_API_TYPE(logevent) *api;

    api = (IB_PROVIDER_API_TYPE(logevent) *)ctx->logevent->pr->api;
    return api->fetch_events(ctx->logevent, pevents);
}

void ib_clog_events_write(ib_context_t *ctx)
{
    IB_PROVIDER_API_TYPE(logevent) *api;

    api = (IB_PROVIDER_API_TYPE(logevent) *)ctx->logevent->pr->api;
    api->write_events(ctx->logevent);
}

void ib_clog_auditlog_write(ib_context_t *ctx)
{
    IB_PROVIDER_API_TYPE(audit) *api;

    api = (IB_PROVIDER_API_TYPE(audit) *)ctx->audit->pr->api;
    api->write_log(ctx->audit);
}
