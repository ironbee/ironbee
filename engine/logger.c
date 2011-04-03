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

    if (level > 4) {
        IB_FTRACE_RET_VOID();
    }

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

/* Log Event Types */
const char *ib_logevent_type_str[] = {
    "Unknown",
    "Alert",
    NULL
};

/** Log Event Activities */
static const char *ib_logevent_activity_str[] = {
    "Unknown",
    "Recon",
    "Attempted Atack",
    "Successful Attack",
    NULL
};

/** Log Event Primary Classification */
static const char *ib_logevent_pri_class_str[] = {
    "Unknown",
    "Injection",
    NULL
};

/** Log Event Secondary Classification */
static const char *ib_logevent_sec_class_str[] = {
    "Unknown",
    "SQL",
    NULL
};

/** Log Event System Environment */
static const char *ib_logevent_sys_env_str[] = {
    "Unknown",
    "Public",
    "Private",
    NULL
};

/** Log Event Recommended Action */
static const char *ib_logevent_action_str[] = {
    "Unknown",
    "Log",
    "Block",
    "Ignore",
    NULL
};

const char *ib_logevent_type_name(ib_logevent_type_t num)
{
    return ib_logevent_type_str[num];
}

const char *ib_logevent_activity_name(ib_logevent_activity_t num)
{
    return ib_logevent_activity_str[num];
}

const char *ib_logevent_pri_class_name(ib_logevent_pri_class_t num)
{
    return ib_logevent_pri_class_str[num];
}

const char *ib_logevent_sec_class_name(ib_logevent_sec_class_t num)
{
    return ib_logevent_sec_class_str[num];
}

const char *ib_logevent_sys_env_name(ib_logevent_sys_env_t num)
{
    return ib_logevent_sys_env_str[num];
}

const char *ib_logevent_action_name(ib_logevent_action_t num)
{
    return ib_logevent_action_str[num];
}

/// @todo Change this to _ex function with all fields and only use
///       the required fields here.
ib_status_t ib_logevent_create(ib_logevent_t **ple,
                               ib_mpool_t *pool,
                               const char *rule_id,
                               ib_logevent_type_t type,
                               ib_logevent_activity_t activity,
                               ib_logevent_pri_class_t pri_class,
                               ib_logevent_sec_class_t sec_class,
                               ib_logevent_sys_env_t sys_env,
                               ib_logevent_action_t rec_action,
                               ib_logevent_action_t action,
                               uint8_t confidence,
                               uint8_t severity,
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
    (*ple)->event_id = (tv.tv_sec << (32-8)) + tv.tv_usec;

    /// @todo Generate the remaining portions of the event

    (*ple)->mp = pool;
    (*ple)->rule_id = rule_id;
    (*ple)->type = type;
    (*ple)->activity = activity;
    (*ple)->pri_class = pri_class;
    (*ple)->sec_class = sec_class;
    (*ple)->sys_env = sys_env;
    (*ple)->rec_action = rec_action;
    (*ple)->action = action;
    (*ple)->confidence = confidence;
    (*ple)->severity = severity;

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

    if (ctx == NULL) {
        return IB_EINVAL;
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)ctx->logevent->pr->api;
    return api->add_event(ctx->logevent, e);
}

ib_status_t ib_clog_event_remove(ib_context_t *ctx,
                                 uint32_t id)
{
    IB_PROVIDER_API_TYPE(logevent) *api;

    if (ctx == NULL) {
        return IB_EINVAL;
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)ctx->logevent->pr->api;
    return api->remove_event(ctx->logevent, id);
}

ib_status_t ib_clog_events_get(ib_context_t *ctx,
                               ib_list_t **pevents)
{
    IB_PROVIDER_API_TYPE(logevent) *api;

    if (ctx == NULL) {
        return IB_EINVAL;
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)ctx->logevent->pr->api;
    return api->fetch_events(ctx->logevent, pevents);
}

void ib_clog_events_write(ib_context_t *ctx)
{
    IB_PROVIDER_API_TYPE(logevent) *api;

    if (ctx == NULL) {
        return;
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)ctx->logevent->pr->api;
    api->write_events(ctx->logevent);
}

void ib_clog_auditlog_write(ib_context_t *ctx)
{
    IB_PROVIDER_API_TYPE(audit) *api;

    if (ctx == NULL) {
        return;
    }

    api = (IB_PROVIDER_API_TYPE(audit) *)ctx->audit->pr->api;
    api->write_log(ctx->audit);
}
