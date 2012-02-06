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
#include <ironbee/core.h>
#include <ironbee/mpool.h>
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
    IB_FTRACE_INIT();
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
    "Attempted Attack",
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
    if (num >= (sizeof(ib_logevent_type_str) / sizeof(const char *))) {
        return ib_logevent_type_str[0];
    }
    return ib_logevent_type_str[num];
}

const char *ib_logevent_activity_name(ib_logevent_activity_t num)
{
    if (num >= (sizeof(ib_logevent_activity_str) / sizeof(const char *))) {
        return ib_logevent_activity_str[0];
    }
    return ib_logevent_activity_str[num];
}

const char *ib_logevent_pri_class_name(ib_logevent_pri_class_t num)
{
    if (num >= (sizeof(ib_logevent_pri_class_str) / sizeof(const char *))) {
        return ib_logevent_pri_class_str[0];
    }
    return ib_logevent_pri_class_str[num];
}

const char *ib_logevent_sec_class_name(ib_logevent_sec_class_t num)
{
    if (num >= (sizeof(ib_logevent_sec_class_str) / sizeof(const char *))) {
        return ib_logevent_sec_class_str[0];
    }
    return ib_logevent_sec_class_str[num];
}

const char *ib_logevent_sys_env_name(ib_logevent_sys_env_t num)
{
    if (num >= (sizeof(ib_logevent_sys_env_str) / sizeof(const char *))) {
        return ib_logevent_sys_env_str[0];
    }
    return ib_logevent_sys_env_str[num];
}

const char *ib_logevent_action_name(ib_logevent_action_t num)
{
    if (num >= (sizeof(ib_logevent_action_str) / sizeof(const char *))) {
        return ib_logevent_action_str[0];
    }
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
    IB_FTRACE_INIT();
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
    (*ple)->msg = ib_mpool_strdup(pool, buf);

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Exported Logging Routines -- */

ib_provider_inst_t *ib_log_provider_get_instance(ib_context_t *ctx)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg;
    ib_status_t rc;

    rc = ib_context_module_config(ctx, ib_core_module(),
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        IB_FTRACE_RET_PTR(ib_provider_inst_t, NULL);
    }

    IB_FTRACE_RET_PTR(ib_provider_inst_t, corecfg->pi.logger);
}

void ib_log_provider_set_instance(ib_context_t *ctx, ib_provider_inst_t *pi)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg;
    ib_status_t rc;

    rc = ib_context_module_config(ctx, ib_core_module(),
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        /// @todo This func should return ib_status_t now
        IB_FTRACE_RET_VOID();
    }

    corecfg->pi.logger = pi;

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
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(logger) *api;
    ib_core_cfg_t *corecfg;
    ib_provider_inst_t *pi = NULL;
    ib_status_t rc;
    char prefix_with_pid[1024];

    if (prefix != NULL) {
      snprintf(prefix_with_pid, 1024, "[%d] %s", getpid(), prefix);
    }
    else {
      snprintf(prefix_with_pid, 1024, "[%d] ", getpid());
    }

    if (ctx != NULL) {
        rc = ib_context_module_config(ctx, ib_core_module(),
                                      (void *)&corecfg);
        if (rc == IB_OK) {
            pi = corecfg->pi.logger;
        }

        if (pi != NULL) {
            api = (IB_PROVIDER_API_TYPE(logger) *)pi->pr->api;

            api->vlogmsg(pi, ctx, level, prefix_with_pid, file, line, fmt, ap);

            IB_FTRACE_RET_VOID();
        }
    }

    default_logger(stderr, level, prefix_with_pid, file, line, fmt, ap);
}

ib_status_t ib_event_add(ib_provider_inst_t *pi,
                         ib_logevent_t *e)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(logevent) *api;
    ib_status_t rc;

    if (pi == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)pi->pr->api;

    rc = api->add_event(pi, e);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_event_remove(ib_provider_inst_t *pi,
                            uint32_t id)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(logevent) *api;
    ib_status_t rc;

    if (pi == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)pi->pr->api;

    rc = api->remove_event(pi, id);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_event_get_all(ib_provider_inst_t *pi,
                             ib_list_t **pevents)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(logevent) *api;
    ib_status_t rc;

    if (pi == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)pi->pr->api;

    rc = api->fetch_events(pi, pevents);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_event_write_all(ib_provider_inst_t *pi)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(logevent) *api;
    ib_status_t rc;

    if (pi == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    api = (IB_PROVIDER_API_TYPE(logevent) *)pi->pr->api;

    rc = api->write_events(pi);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_auditlog_write(ib_provider_inst_t *pi)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_API_TYPE(audit) *api;
    ib_status_t rc;

    if (pi == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    api = (IB_PROVIDER_API_TYPE(audit) *)pi->pr->api;

    rc = api->write_log(pi);
    IB_FTRACE_RET_STATUS(rc);
}
