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
 * @brief IronBee - Core Module
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>


#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>

#include "ironbee_private.h"

#define MODULE_NAME        core
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/// @todo Fix this:
#ifndef X_MODULE_BASE_PATH
#define X_MODULE_BASE_PATH IB_XSTRINGIFY(MODULE_BASE_PATH) "/"
#endif


/* Instantiate a module global configuration. */
static ib_core_cfg_t core_global_cfg;


/* -- Core Logger Provider -- */

/**
 * @internal
 * Core debug logger.
 *
 * This is just a simple default logger that prints to stderr. Typically
 * a plugin will register a more elaborate logger and this will not be used,
 * except during startup prior to the registration of another logger.
 *
 * @param fh File handle
 * @param level Log level
 * @param prefix String prefix to prepend to the message or NULL
 * @param file Source code filename (typically __FILE__) or NULL
 * @param line Source code line number (typically __LINE__) or NULL
 * @param fmt Printf like format string
 * @param ap Variable length parameter list
 */
static void core_logger(FILE *fh, int level,
                        const char *prefix, const char *file, int line,
                        const char *fmt, va_list ap)
{
    char fmt2[1024 + 1];

    if ((file != NULL) && (line > 0)) {
        int ec = snprintf(fmt2, 1024,
                          "%s[%d] (%s:%d) %s\n",
                          (prefix?prefix:""), level, file, line, fmt);
        if (ec > 1024) {
            /// @todo Do something better
            fprintf(fh, "Formatter too long (>1024): %d\n", (int)ec);
            fflush(fh);
            abort();
        }
    }
    else {
        int ec = snprintf(fmt2, 1024,
                          "%s[%d] %s\n",
                          (prefix?prefix:""), level, fmt);
        if (ec > 1024) {
            /// @todo Do something better
            fprintf(fh, "Formatter too long (>1024): %d\n", (int)ec);
            fflush(fh);
            abort();
        }
    }

    vfprintf(fh, fmt2, ap);
    fflush(fh);
}


/**
 * @internal
 * Logger provider interface mapping for the core module.
 */
static IB_PROVIDER_IFACE_TYPE(logger) core_logger_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,
    (ib_log_logger_fn_t)core_logger
};


/* -- Core Log Event Provider -- */

static ib_status_t core_logevent_write(ib_provider_inst_t *epi, ib_logevent_t *e)
{
    ib_log_alert(epi->pr->ib, 1, "Event [id %016" PRIxMAX "][type %d]: %s",
                 e->event_id, e->type, e->msg);
    return IB_OK;
}

static IB_PROVIDER_IFACE_TYPE(logevent) core_logevent_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,
    core_logevent_write
};


/* -- Audit Provider -- */

typedef struct core_audit_cfg_t core_audit_cfg_t;
struct core_audit_cfg_t {
    FILE           *fp;
    int             parts_written;
    const char     *boundary;
};

/// @todo Make this public
static ib_status_t ib_auditlog_part_add(ib_auditlog_t *log,
                                        const char *name,
                                        const char *type,
                                        void *data,
                                        ib_auditlog_part_gen_fn_t generator,
                                        void *gen_data)
{
    IB_FTRACE_INIT(ib_auditlog_part_add);
    ib_status_t rc;

    ib_auditlog_part_t *part =
        (ib_auditlog_part_t *)ib_mpool_alloc(log->mp, sizeof(*part));

    if (part == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    part->log = log;
    part->name = name;
    part->content_type = type;
    part->part_data = data;
    part->fn_gen = generator;
    part->gen_data = gen_data;

    rc = ib_list_push(log->parts, part);

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t core_audit_open(ib_provider_inst_t *lpi,
                                   ib_auditlog_t *log)
{
    IB_FTRACE_INIT(core_audit_open);
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;
    /// @todo Use log->ctx to get filename, etc.
    const char *fn = "/tmp/ironbee-audit.log";
    int ec;

    if (cfg == NULL) {
        cfg = (core_audit_cfg_t *)ib_mpool_calloc(log->mp, 1, sizeof(*cfg));

        if (cfg == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        /// @todo Add a random id to this
        cfg->boundary = log->tx->id ? log->tx->id : "FixMe-No-Tx-on-Audit";
        log->cfg_data = cfg;
    }

    if (cfg->fp == NULL) {
        cfg->fp = fopen(fn, "wb");
        if (cfg->fp == NULL) {
            ec = errno;
            ib_log_error(log->ib, 1, "Could not open audit log \"%s\": %s (%d)",
                         fn, strerror(ec), ec);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t core_audit_write_header(ib_provider_inst_t *lpi,
                                           ib_auditlog_t *log)
{
    IB_FTRACE_INIT(core_audit_write_header);
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;
    const char *header = "\r\nThis is a multi-part message in MIME format.\r\n\r\n";
    size_t hlen = strlen(header);

    if (fwrite(header, hlen, 1, cfg->fp) != 1) {
        ib_log_error(lpi->pr->ib, 1, "Failed to write audit log header");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }
    fflush(cfg->fp);

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t core_audit_write_part(ib_provider_inst_t *lpi,
                                         ib_auditlog_part_t *part)
{
    IB_FTRACE_INIT(core_audit_write_part);
    ib_auditlog_t *log = part->log;
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;
    const uint8_t *chunk;
    size_t chunk_size;

    /* Write the MIME boundary and part header */
    fprintf(cfg->fp,
            "\r\n--%s"
            "\r\nContent-Disposition: audit-log-part; name=\"%s\""
            "\r\nContent-Transfer-Encoding: binary"
            "\r\nContent-Type: %s"
            "\r\n\r\n",
            cfg->boundary,
            part->name,
            part->content_type);

    /* Write the part data. */
    while((chunk_size = part->fn_gen(part, &chunk)) != 0) {
        if (fwrite(chunk, chunk_size, 1, cfg->fp) != 1) {
            ib_log_error(lpi->pr->ib, 1, "Failed to write audit log part");
            fflush(cfg->fp);
            IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
        }
        cfg->parts_written++;
    }

    /* Finish the part. */
    fflush(cfg->fp);

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t core_audit_write_footer(ib_provider_inst_t *lpi,
                                           ib_auditlog_t *log)
{
    IB_FTRACE_INIT(core_audit_write_footer);
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;

    if (cfg->parts_written > 0) {
        fprintf(cfg->fp, "\r\n--%s--\r\n", cfg->boundary);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t core_audit_close(ib_provider_inst_t *lpi,
                                    ib_auditlog_t *log)
{
    IB_FTRACE_INIT(core_audit_close);
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;

    if (cfg->fp != NULL) {
        fclose(cfg->fp);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static IB_PROVIDER_IFACE_TYPE(audit) core_audit_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,
    core_audit_open,
    core_audit_write_header,
    core_audit_write_part,
    core_audit_write_footer,
    core_audit_close
};

/* -- Core Data Provider -- */

/**
 * @internal
 * Core data provider implementation to add a data field.
 *
 * @param dpi Data provider instance
 * @param f Field
 *
 * @returns Status code
 */
static ib_status_t core_data_add(ib_provider_inst_t *dpi,
                                 ib_field_t *f)
{
    IB_FTRACE_INIT(core_data_add);
    /// @todo Needs to be more field-aware (handle lists, etc)
    /// @todo Needs to not allow adding if already exists (except list items)
    ib_status_t rc = ib_hash_set_ex((ib_hash_t *)dpi->data,
                                    (void *)f->name, f->nlen, f);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Core data provider implementation to set a data field.
 *
 * @param dpi Data provider instance
 * @param f Field
 *
 * @returns Status code
 */
static ib_status_t core_data_set(ib_provider_inst_t *dpi,
                                 ib_field_t *f)
{
    IB_FTRACE_INIT(core_data_set);
    /// @todo Needs to be more field-aware (handle lists, etc)
    ib_status_t rc = ib_hash_set_ex((ib_hash_t *)dpi->data,
                                    (void *)f->name, f->nlen, f);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Core data provider implementation to set a relative data field value.
 *
 * @warning Not yet implemented.
 *
 * @param dpi Data provider instance
 * @param name Field name
 * @param nlen Field length
 * @param adjval Value to adjust (add or subtract a numeric value)
 *
 * @returns Status code
 */
static ib_status_t core_data_set_relative(ib_provider_inst_t *dpi,
                                          const char *name,
                                          size_t nlen,
                                          intmax_t adjval)
{
    IB_FTRACE_INIT(core_data_set_relative);
    ib_field_t *f;
    ib_status_t rc;

    rc = ib_hash_get_ex((ib_hash_t *)dpi->data,
                        (void *)name, nlen,
                        (void *)&f);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    switch (f->type) {
        case IB_FTYPE_NUM:
            /// @todo Make sure this is atomic
            /// @todo Check for overflow
            *(intmax_t **)((f)->pval) += adjval;
            break;
        case IB_FTYPE_UNUM:
            /// @todo Make sure this is atomic
            /// @todo Check for overflow
            *(uintmax_t **)((f)->pval) += adjval;
            break;
        default:
            IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Core data provider implementation to set a relative data field value.
 *
 * @param dpi Data provider instance
 * @param name Field name
 * @param nlen Field name length
 * @param adjval Value to adjust (add or subtract a numeric value)
 *
 * @returns Status code
 */
static ib_status_t core_data_get(ib_provider_inst_t *dpi,
                                 const char *name,
                                 size_t nlen,
                                 ib_field_t **pf)
{
    IB_FTRACE_INIT(core_data_get);
    ib_status_t rc = ib_hash_get_ex((ib_hash_t *)dpi->data,
                                    (void *)name, nlen,
                                    (void *)pf);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Core data provider implementation to get a data field.
 *
 * @param dpi Data provider instance
 * @param name Field name
 * @param nlen Field name length
 * @param pf Address which field will be written
 *
 * @returns Status code
 */
static ib_status_t core_data_remove(ib_provider_inst_t *dpi,
                                    const char *name,
                                    size_t nlen,
                                    ib_field_t **pf)
{
    IB_FTRACE_INIT(core_data_remove);
    ib_status_t rc = ib_hash_remove_ex((ib_hash_t *)dpi->data,
                                       (void *)name, nlen,
                                       (void *)pf);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Core data provider implementation to clear the data store.
 *
 * @param dpi Data provider instance
 *
 * @returns Status code
 */
static ib_status_t core_data_clear(ib_provider_inst_t *dpi)
{
    IB_FTRACE_INIT(core_data_clear);
    ib_hash_clear((ib_hash_t *)dpi->data);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Data provider interface mapping for the core module.
 */
static IB_PROVIDER_IFACE_TYPE(data) core_data_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,
    core_data_add,
    core_data_set,
    core_data_set_relative,
    core_data_get,
    core_data_remove,
    core_data_clear
};


/* -- Logger API Implementations -- */

/**
 * @internal
 * Core data provider API implementation to log data via va_list args.
 *
 * @param lpi Logger provider instance
 * @param ctx Config context
 * @param level Log level
 * @param prefix String prefix to prepend to the message or NULL
 * @param file Source code filename (typically __FILE__) or NULL
 * @param line Source code line number (typically __LINE__) or NULL
 * @param fmt Printf like format string
 * @param ap Variable length parameter list
 *
 * @returns Status code
 */
static void logger_api_vlogmsg(ib_provider_inst_t *lpi, ib_context_t *ctx,
                               int level,
                               const char *prefix,
                               const char *file, int line,
                               const char *fmt, va_list ap)
{
    IB_PROVIDER_IFACE_TYPE(logger) *iface;

    if (level > (int)ctx->core_cfg->log_level) {
        return;
    }

    iface = (IB_PROVIDER_IFACE_TYPE(logger) *)lpi->pr->iface;

    /* Just calls the interface logger with the provider instance data as
     * the first parameter (if the interface is implemented and not
     * just abstract).
     */
    /// @todo Probably should not need this check
    if (iface != NULL) {
        iface->logger((lpi->pr->data?lpi->pr->data:lpi->data),
                      level, prefix, file, line, fmt, ap);
    }
}

/**
 * @internal
 * Core data provider API implementation to log data via variable args.
 *
 * @param lpi Logger provider instance
 * @param ctx Config context
 * @param level Log level
 * @param prefix String prefix to prepend to the message or NULL
 * @param file Source code filename (typically __FILE__) or NULL
 * @param line Source code line number (typically __LINE__) or NULL
 * @param fmt Printf like format string
 *
 * @returns Status code
 */
static void logger_api_logmsg(ib_provider_inst_t *lpi, ib_context_t *ctx,
                              int level,
                              const char *prefix,
                              const char *file, int line,
                              const char *fmt, ...)
{
    IB_PROVIDER_IFACE_TYPE(logger) *iface;
    va_list ap;

    if (level > (int)ctx->core_cfg->log_level) {
        return;
    }

    iface = (IB_PROVIDER_IFACE_TYPE(logger) *)lpi->pr->iface;

    va_start(ap, fmt);

    /* Just calls the interface logger with the provider instance data as
     * the first parameter (if the interface is implemented and not
     * just abstract).
     */
    /// @todo Probably should not need this check
    if (iface != NULL) {
        iface->logger((lpi->pr->data?lpi->pr->data:lpi->data),
                      level, prefix, file, line, fmt, ap);
    }

    va_end(ap);
}

/**
 * @internal
 * Logger provider registration function.
 *
 * This just does a version and sanity check on a registered provider.
 *
 * @param ib Engine
 * @param lpr Logger provider
 *
 * @returns Status code
 */
static ib_status_t logger_register(ib_engine_t *ib,
                                   ib_provider_t *lpr)
{
    IB_FTRACE_INIT(logger_register);
    IB_PROVIDER_IFACE_TYPE(logger) *iface = (IB_PROVIDER_IFACE_TYPE(logger) *)lpr->iface;

    /* Check that versions match. */
    if (iface->version != IB_PROVIDER_VERSION_LOGGER) {
        IB_FTRACE_RET_STATUS(IB_EINCOMPAT);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Logger provider initialization function.
 *
 * @warning Not yet doing anything.
 *
 * @param lpi Logger provider intstance
 * @param data User data
 *
 * @returns Status code
 */
static ib_status_t logger_init(ib_provider_inst_t *lpi,
                               void *data)
{
    IB_FTRACE_INIT(logger_init);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Logger provider API mapping for core module.
 */
static IB_PROVIDER_API_TYPE(logger) logger_api = {
    logger_api_vlogmsg,
    logger_api_logmsg
};


/* -- Audit API Implementations -- */

/**
 * @internal
 * Write an audit log.
 *
 * @param ib Engine
 * @param lpr Audit provider
 *
 * @returns Status code
 */
static ib_status_t audit_api_write_log(ib_provider_inst_t *lpi)
{
    IB_FTRACE_INIT(audit_api_write_log);
    IB_PROVIDER_IFACE_TYPE(audit) *iface = (IB_PROVIDER_IFACE_TYPE(audit) *)lpi->pr->iface;
    ib_auditlog_t *log = (ib_auditlog_t *)lpi->data;
    ib_list_node_t *node;
    ib_status_t rc;

    if (ib_list_elements(log->parts) == 0) {
        ib_log_debug(lpi->pr->ib, 4, "No parts to write to audit log");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Open the log if required. */
    if (iface->open != NULL) {
        rc = iface->open(lpi, log);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Write the header if required. */
    if (iface->write_header != NULL) {
        rc = iface->write_header(lpi, log);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Write the parts. */
    IB_LIST_LOOP(log->parts, node) {
        ib_auditlog_part_t *part = (ib_auditlog_part_t *)ib_list_node_data(node);
        rc = iface->write_part(lpi, part);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Write the footer if required. */
    if (iface->write_footer != NULL) {
        rc = iface->write_footer(lpi, log);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Close the log if required. */
    if (iface->close != NULL) {
        rc = iface->close(lpi, log);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Audit provider registration function.
 *
 * This just does a version and sanity check on a registered provider.
 *
 * @param ib Engine
 * @param lpr Audit provider
 *
 * @returns Status code
 */
static ib_status_t audit_register(ib_engine_t *ib,
                                  ib_provider_t *lpr)
{
    IB_FTRACE_INIT(audit_register);
    IB_PROVIDER_IFACE_TYPE(audit) *iface = (IB_PROVIDER_IFACE_TYPE(audit) *)lpr->iface;

    /* Check that versions match. */
    if (iface->version != IB_PROVIDER_VERSION_AUDIT) {
        IB_FTRACE_RET_STATUS(IB_EINCOMPAT);
    }

    /* Verify that required interface functions are implemented. */
    if (iface->write_part == NULL) {
        ib_log_error(ib, 0, "The write_part function "
                     "MUST be implemented by a audit provider");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Audit provider API mapping for core module.
 */
static IB_PROVIDER_API_TYPE(audit) audit_api = {
    audit_api_write_log
};


/* -- Logevent API Implementations -- */

/**
 * @internal
 * Core logevent provider API implementation to add an event.
 *
 * @param epi Logevent provider instance
 * @param e Event to add
 *
 * @returns Status code
 */
static ib_status_t logevent_api_add_event(ib_provider_inst_t *epi,
                                          ib_logevent_t *e)
{
    IB_FTRACE_INIT(logevent_api_add_event);
    ib_list_t *events = (ib_list_t *)epi->data;

    ib_list_push(events, e);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Core logevent provider API implementation to remove an event.
 *
 * @param epi Logevent provider instance
 * @param id Event ID to remove
 *
 * @returns Status code
 */
static ib_status_t logevent_api_remove_event(ib_provider_inst_t *epi,
                                             uint32_t id)
{
    IB_FTRACE_INIT(logevent_api_remove_event);
    ib_list_t *events;
    ib_list_node_t *node;
    ib_list_node_t *node_next;

    events = (ib_list_t *)epi->data;
    IB_LIST_LOOP_SAFE(events, node, node_next) {
        ib_logevent_t *e = (ib_logevent_t *)ib_list_node_data(node);
        if (e->event_id == id) {
            ib_list_node_remove(events, node);
            IB_FTRACE_RET_STATUS(IB_OK);
        }
    }

    IB_FTRACE_RET_STATUS(IB_ENOENT);
}

/**
 * @internal
 * Core logevent provider API implementation to fetch events.
 *
 * @param epi Logevent provider instance
 * @param id Event ID to remove
 *
 * @returns Status code
 */
static ib_status_t logevent_api_fetch_events(ib_provider_inst_t *epi,
                                             ib_list_t **pevents)
{
    IB_FTRACE_INIT(logevent_api_fetch_events);
    *pevents = (ib_list_t *)epi->data;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Core logevent provider API implementation to write out (and remove)
 * all the pending events.
 *
 * @param epi Logevent provider instance
 *
 * @returns Status code
 */
static ib_status_t logevent_api_write_events(ib_provider_inst_t *epi)
{
    IB_FTRACE_INIT(logevent_api_write_events);
    IB_PROVIDER_IFACE_TYPE(logevent) *iface;
    ib_list_t *events;
    ib_logevent_t *e;

    events = (ib_list_t *)epi->data;
    if (events == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    iface = (IB_PROVIDER_IFACE_TYPE(logevent) *)epi->pr->iface;
    while (ib_list_pop(events, (void *)&e) == IB_OK) {
        iface->write(epi, e);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static size_t ib_auditlog_gen_raw(ib_auditlog_part_t *part,
                                  const uint8_t **chunk)
{
    //ib_list_t *list = (ib_list_t *)part->part_data;

    if (part->gen_data == NULL) {
        /// @todo Testing
        *chunk = (uint8_t *)"TODO: Real data here";
        part->gen_data = (void *)1;
        return strlen(*(const char **)chunk);
    }

    return 0;
}

static size_t ib_auditlog_gen_json_list(ib_auditlog_part_t *part,
                                   const uint8_t **chunk)
{
    //ib_list_t *list = (ib_list_t *)part->part_data;

    if (part->gen_data == NULL) {
        /// @todo Testing
        *chunk = (uint8_t *)"{\r\n  \"name\": \"value\"\r\n}";
        part->gen_data = (void *)1;
        return strlen(*(const char **)chunk);
    }

    return 0;
}

#if 0
static size_t ib_auditlog_gen_json_ftree(ib_auditlog_part_t *part,
                                         const uint8_t **chunk)
{
    //ib_field_t *f = (ib_field_t *)part->part_data;

    if (part->gen_data == NULL) {
        /// @todo Testing
        *chunk = (uint8_t *)"{\r\n  \"name\": \"value\"\r\n}";
        part->gen_data = (void *)1;
        return strlen(*(const char **)chunk);
    }

    return 0;
}
#endif

static size_t ib_auditlog_gen_json_events(ib_auditlog_part_t *part,
                                          const uint8_t **chunk)
{
    ib_engine_t *ib = part->log->ib;
    ib_logevent_t *e;
    uint8_t *rec;
    size_t rlen;

#define CORE_EVT_MAX_REC_LEN 1024
    
    /* The gen_data field is used to store the current state. NULL
     * means the part has not started yet and a -1 value
     * means it is done. Anything else is a node in the event list.
     */
    if (part->gen_data == NULL) {
        ib_list_t *list;
        ib_status_t rc;


        rc = ib_clog_events_get(part->log->ctx, &list);

        /* Error. */
        if (rc != IB_OK) {
            ib_log_error(ib, 3, "Error fetching event list: %d", rc);
            *chunk = (const uint8_t *)"{}";
            part->gen_data = (void *)-1;
            return 2;
        }

        /* No events. */
        if (ib_list_elements(list) == 0) {
            ib_log_debug(ib, 4, "No events in audit log");
            *chunk = (const uint8_t *)"{}";
            part->gen_data = (void *)-1;
            return 2;
        }

        *chunk = (const uint8_t *)"{\r\n  \"events\": [\r\n";
        part->gen_data = ib_list_first(list);
        return strlen(*(const char **)chunk);
    }
    else if (part->gen_data == (void *)-1) {
        part->gen_data = NULL;
        return 0;
    }

    e = (ib_logevent_t *)ib_list_node_data((ib_list_node_t *)part->gen_data);
    if (e != NULL) {
        rec = (uint8_t *)ib_mpool_alloc(part->log->mp, CORE_EVT_MAX_REC_LEN);

        /* Error. */
        if (rec == NULL) {
            *chunk = (const uint8_t *)"  ]\r\n}";
            return strlen(*(const char **)chunk);
        }

        rlen = snprintf((char *)rec, CORE_EVT_MAX_REC_LEN,
                        "    {\r\n"
                        "      \"event-id\": %" PRIu32 ",\r\n"
                        "      \"rule-id\": \"%s\",\r\n"
                        "      \"publisher\": \"%s\",\r\n"
                        "      \"source\": \"%s\",\r\n"
                        "      \"source-version\": \"%s\",\r\n"
                        "      \"type\": \"%s\",\r\n"
                        "      \"activity\": \"%s\",\r\n"
                        "      \"category\": \"%s/%s\",\r\n"
                        "      \"confidence\": %u,\r\n"
                        "      \"severity\": %u,\r\n"
                        "      \"sys-env\": \"%s\",\r\n"
                        "      \"rec-action\": \"%s\",\r\n"
                        "      \"tags\": [],\r\n"
                        "      \"fields\": [],\r\n"
                        "      \"msg\": \"%s\",\r\n"
                        "      \"data\": \"\"\r\n"
                        "    }\r\n",
                        e->event_id,
                        e->rule_id ? e->rule_id : "-",
                        e->publisher ? e->publisher : "-",
                        e->source ? e->source : "-",
                        e->source_ver ? e->source_ver : "-",
                        e->type ? "TODO:LookupText" : "-",
                        e->activity ? "TODO:LookupText" : "-",
                        e->pri_cat ? "TODO:LookupText" : "-",
                        e->sec_cat ? "TODO:LookupText" : "-",
                        e->confidence,
                        e->severity,
                        e->sys_env ? "TODO:LookupText" : "-",
                        e->rec_action ? "TODO:LookupText" : "-",
                        e->msg ? e->msg : "-");

        /* Verify size. */
        if (rlen >= CORE_EVT_MAX_REC_LEN) {
            ib_log_error(ib, 3, "Event too large to log: %" PRIuMAX, rlen);
            *chunk = (const uint8_t *)"    {}";
            part->gen_data = (void *)-1;
            return 2;
        }

        *chunk = rec;
    }
    else {
        ib_log_error(ib, 4, "NULL event");
        *chunk = (const uint8_t *)"    {}";
        part->gen_data = (void *)-1;
        return 2;
    }
    part->gen_data = ib_list_node_next((ib_list_node_t *)part->gen_data);

    /* Close the json structure. */
    if (part->gen_data == NULL) {
        size_t clen = strlen(*(const char **)chunk);

        part->gen_data = (void *)-1;

        if (clen+6 > CORE_EVT_MAX_REC_LEN) {
            if (clen+2 > CORE_EVT_MAX_REC_LEN) {
                ib_log_error(ib, 4, "Event too large to fit in buffer");
                *chunk = (const uint8_t *)"    {}\r\n  ]\r\n}";
            }
            memcpy(*(uint8_t **)chunk + clen, "]}", 2);
            return clen + 2;
        }
        memcpy(*(uint8_t **)chunk + clen, "  ]\r\n}", 6);
        return clen + 8;
    }

    return strlen(*(const char **)chunk);
}


/**
 * @internal
 * Handle writing the logevents.
 *
 * @param ib Engine
 * @param tx Transaction
 * @param cbdata Callback data
 *
 * @returns Status code
 */
static ib_status_t logevent_hook_postprocess(ib_engine_t *ib,
                                             ib_tx_t *tx,
                                             void *cbdata)
{
    IB_FTRACE_INIT(logevent_hook_postprocess);
    ib_auditlog_t *auditlog;
    ib_provider_inst_t *audit;
    ib_list_t *list;
    ib_status_t rc;

    /* Auditing */
    /// @todo Only create if needed
    auditlog = (ib_auditlog_t *)ib_mpool_calloc(tx->mp, 1, sizeof(*auditlog));
    if (auditlog == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    auditlog->ib = ib;
    auditlog->mp = tx->mp;
    auditlog->ctx = tx->ctx;
    auditlog->tx = tx;

    rc = ib_list_create(&auditlog->parts, auditlog->mp);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /// @todo Parts should be configurable

    /* Part: header */
    ib_list_create(&list, auditlog->mp);
    /// @todo Create real data
    ib_auditlog_part_add(auditlog,
                         "header",
                         "application/json",
                         list,
                         ib_auditlog_gen_json_list,
                         NULL);

    /* Part: events */
    ib_list_create(&list, auditlog->mp);
    /// @todo Create real data
    ib_auditlog_part_add(auditlog,
                         "events",
                         "application/json",
                         list,
                         ib_auditlog_gen_json_events,
                         NULL);

    /* Part: http-request-metadata */
    ib_list_create(&list, auditlog->mp);
    /// @todo Create real data
    ib_auditlog_part_add(auditlog,
                         "http-request-metadata",
                         "application/json",
                         list,
                         ib_auditlog_gen_json_list,
                         NULL);

    /* Part: http-request-headers */
    ib_list_create(&list, auditlog->mp);
    /// @todo Create real data
    ib_auditlog_part_add(auditlog,
                         "http-request-headers",
                         "application/octet-stream",
                         list,
                         ib_auditlog_gen_raw,
                         NULL);

    /* Part: http-request-trailers */
    ib_list_create(&list, auditlog->mp);
    /// @todo Create real data
    ib_auditlog_part_add(auditlog,
                         "http-request-trailers",
                         "application/octet-stream",
                         list,
                         ib_auditlog_gen_raw,
                         NULL);

    /* Part: http-request-body */
    ib_list_create(&list, auditlog->mp);
    /// @todo Create real data
    ib_auditlog_part_add(auditlog,
                         "http-request-body",
                         "application/octet-stream",
                         list,
                         ib_auditlog_gen_raw,
                         NULL);

    /* Part: http-response-metadata */
    ib_list_create(&list, auditlog->mp);
    /// @todo Create real data
    ib_auditlog_part_add(auditlog,
                         "http-response-metadata",
                         "application/json",
                         list,
                         ib_auditlog_gen_json_list,
                         NULL);

    /* Part: http-response-headers */
    ib_list_create(&list, auditlog->mp);
    /// @todo Create real data
    ib_auditlog_part_add(auditlog,
                         "http-response-headers",
                         "application/octet-stream",
                         list,
                         ib_auditlog_gen_raw,
                         NULL);

    /* Part: http-response-trailers */
    ib_list_create(&list, auditlog->mp);
    /// @todo Create real data
    ib_auditlog_part_add(auditlog,
                         "http-response-trailers",
                         "application/octet-stream",
                         list,
                         ib_auditlog_gen_raw,
                         NULL);

    /* Part: http-response-body */
    ib_list_create(&list, auditlog->mp);
    /// @todo Create real data
    ib_auditlog_part_add(auditlog,
                         "http-response-body",
                         "application/octet-stream",
                         list,
                         ib_auditlog_gen_raw,
                         NULL);


    /* Audit Provider */
    rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_AUDIT,
                                     tx->ctx->core_cfg->audit, &audit,
                                     ib->mp, auditlog);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to create %s provider instance: %d", IB_PROVIDER_TYPE_AUDIT, rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_audit_provider_set_instance(tx->ctx, audit);

    ib_clog_auditlog_write(tx->ctx);

    /* Events */
    ib_clog_events_write(tx->ctx);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Logevent provider registration function.
 *
 * This just does a version and sanity check on a registered provider.
 *
 * @param ib Engine
 * @param lpr Logevent provider
 *
 * @returns Status code
 */
static ib_status_t logevent_register(ib_engine_t *ib,
                                     ib_provider_t *lpr)
{
    IB_FTRACE_INIT(logevent_register);
    IB_PROVIDER_IFACE_TYPE(logevent) *iface = (IB_PROVIDER_IFACE_TYPE(logevent) *)lpr->iface;

    /* Check that versions match. */
    if (iface->version != IB_PROVIDER_VERSION_LOGEVENT) {
        IB_FTRACE_RET_STATUS(IB_EINCOMPAT);
    }

    /* Verify that required interface functions are implemented. */
    if (iface->write == NULL) {
        ib_log_error(ib, 0, "The write function "
                     "MUST be implemented by a logevent provider");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Logevent provider initialization function.
 *
 * @warning Not yet doing anything.
 *
 * @param epi Logevent provider intstance
 * @param data User data
 *
 * @returns Status code
 */
static ib_status_t logevent_init(ib_provider_inst_t *epi,
                                 void *data)
{
    IB_FTRACE_INIT(logevent_init);
    ib_list_t *events;
    ib_status_t rc;

    rc = ib_list_create(&events, epi->mp);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    epi->data = events;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Logevent provider API mapping for core module.
 */
static IB_PROVIDER_API_TYPE(logevent) logevent_api = {
    logevent_api_add_event,
    logevent_api_remove_event,
    logevent_api_fetch_events,
    logevent_api_write_events
};



/* -- Parser Implementation -- */

/**
 * @internal
 * Initialize the parser.
 *
 * @param ib Engine
 * @param conn Connection
 * @param cbdata Callback data
 *
 * @returns Status code
 */
static ib_status_t parser_hook_init(ib_engine_t *ib,
                                    ib_conn_t *conn,
                                    void *cbdata)
{
    IB_FTRACE_INIT(parser_hook_init);
    ib_provider_inst_t *pi = ib_parser_provider_get_instance(conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(ib, 0, "Failed to fetch parser interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    if (iface->init == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    rc = iface->init(pi, conn);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Handle a new connection.
 *
 * @param ib Engine
 * @param conn Connection
 * @param cbdata Callback data
 *
 * @returns Status code
 */
static ib_status_t parser_hook_connect(ib_engine_t *ib,
                                       ib_conn_t *conn,
                                       void *cbdata)
{
    IB_FTRACE_INIT(parser_hook_connect);
    ib_provider_inst_t *pi = ib_parser_provider_get_instance(conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        ib_log_error(ib, 0, "Failed to fetch parser interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    if (iface->connect == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    rc = iface->connect(pi, conn);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Handle a disconnection.
 *
 * @param ib Engine
 * @param conn Connection
 * @param cbdata Callback data
 *
 * @returns Status code
 */
static ib_status_t parser_hook_disconnect(ib_engine_t *ib,
                                          ib_conn_t *conn,
                                          void *cbdata)
{
    IB_FTRACE_INIT(parser_hook_disconnect);
    ib_provider_inst_t *pi = ib_parser_provider_get_instance(conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(ib, 0, "Failed to fetch parser interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    if (iface->disconnect == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    rc = iface->disconnect(pi, conn);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Handle incoming data (request).
 *
 * @param ib Engine
 * @param cdata Connection data
 * @param cbdata Callback data
 *
 * @returns Status code
 */
static ib_status_t parser_hook_data_in(ib_engine_t *ib,
                                       ib_conndata_t *cdata,
                                       void *cbdata)
{
    IB_FTRACE_INIT(parser_hook_data_in);
    ib_conn_t *conn = cdata->conn;
    ib_provider_inst_t *pi = ib_parser_provider_get_instance(conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(ib, 0, "Failed to fetch parser interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->data_in(pi, cdata);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Handle outgoing data (response).
 *
 * @param ib Engine
 * @param cdata Connection data
 * @param cbdata Callback data
 *
 * @returns Status code
 */
static ib_status_t parser_hook_data_out(ib_engine_t *ib,
                                        ib_conndata_t *cdata,
                                        void *cbdata)
{
    IB_FTRACE_INIT(parser_hook_data_out);
    ib_conn_t *conn = cdata->conn;
    ib_provider_inst_t *pi = ib_parser_provider_get_instance(conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(ib, 0, "Failed to fetch parser interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->data_out(pi, cdata);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Handle the request header.
 *
 * @param ib Engine
 * @param tx Transaction
 * @param cbdata Callback data
 *
 * @returns Status code
 */
static ib_status_t parser_hook_req_header(ib_engine_t *ib,
                                          ib_tx_t *tx,
                                          void *cbdata)
{
    IB_FTRACE_INIT(parser_hook_req_header);
    ib_provider_inst_t *pi = ib_parser_provider_get_instance(tx->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(ib, 0, "Failed to fetch parser interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->gen_request_header_fields(pi, tx);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Handle the response header.
 *
 * @param ib Engine
 * @param tx Transaction
 * @param cbdata Callback data
 *
 * @returns Status code
 */
static ib_status_t parser_hook_resp_header(ib_engine_t *ib,
                                           ib_tx_t *tx,
                                           void *cbdata)
{
    IB_FTRACE_INIT(parser_hook_resp_header);
    ib_provider_inst_t *pi = ib_parser_provider_get_instance(tx->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(ib, 0, "Failed to fetch parser interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->gen_response_header_fields(pi, tx);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Parser provider registration function.
 *
 * This just does a version and sanity check on a registered provider.
 *
 * @param ib Engine
 * @param lpr Logger provider
 *
 * @returns Status code
 */
static ib_status_t parser_register(ib_engine_t *ib,
                                   ib_provider_t *pr)
{
    IB_FTRACE_INIT(parser_register);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pr?(IB_PROVIDER_IFACE_TYPE(parser) *)pr->iface:NULL;

    /* Check that versions match. */
    if (iface->version != IB_PROVIDER_VERSION_PARSER) {
        IB_FTRACE_RET_STATUS(IB_EINCOMPAT);
    }

    /* Verify that required interface functions are implemented. */
    if (   (iface->data_in == NULL) || (iface->data_out == NULL)
        || (iface->gen_request_header_fields == NULL)
        || (iface->gen_response_header_fields == NULL))
    {
        ib_log_error(ib, 0, "The data in/out and generate interface functions "
                            "MUST be implemented by a parser provider");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Data Implementation -- */

/**
 * @internal
 * Calls a registered provider interface to add a data field to a
 * provider instance.
 *
 * @param dpi Data provider instance
 * @param f Field to add
 * 
 * @returns Status code
 */
static ib_status_t data_api_add(ib_provider_inst_t *dpi,
                                ib_field_t *f)
{
    IB_FTRACE_INIT(data_api_add);
    IB_PROVIDER_IFACE_TYPE(data) *iface = dpi?(IB_PROVIDER_IFACE_TYPE(data) *)dpi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(dpi->pr->ib, 0, "Failed to fetch data interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->add(dpi, f);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Calls a registered provider interface to set a data field in a
 * provider instance.
 *
 * @param dpi Data provider instance
 * @param f Field to add
 * 
 * @returns Status code
 */
static ib_status_t data_api_set(ib_provider_inst_t *dpi,
                                ib_field_t *f)
{
    IB_FTRACE_INIT(data_api_set);
    IB_PROVIDER_IFACE_TYPE(data) *iface = dpi?(IB_PROVIDER_IFACE_TYPE(data) *)dpi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(dpi->pr->ib, 0, "Failed to fetch data interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->set(dpi, f);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Calls a registered provider interface to set a relative value for a data
 * field in a provider instance.
 *
 * This can either increment or decrement a value.
 *
 * @param dpi Data provider instance
 * @param name Field name
 * @param nlen Field name length
 * @param adjval Relative value adjustment
 * 
 * @returns Status code
 */
static ib_status_t data_api_set_relative(ib_provider_inst_t *dpi,
                                         const char *name,
                                         size_t nlen,
                                         intmax_t adjval)
{
    IB_FTRACE_INIT(data_api_set_relative);
    IB_PROVIDER_IFACE_TYPE(data) *iface = dpi?(IB_PROVIDER_IFACE_TYPE(data) *)dpi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(dpi->pr->ib, 0, "Failed to fetch data interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->set_relative(dpi, name, nlen, adjval);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Calls a registered provider interface to get a data field in a
 * provider instance.
 *
 * @param dpi Data provider instance
 * @param name Field name
 * @param nlen Field name length
 * @param pf Address which field is written
 * 
 * @returns Status code
 */
static ib_status_t data_api_get(ib_provider_inst_t *dpi,
                                const char *name,
                                size_t nlen,
                                ib_field_t **pf)
{
    IB_FTRACE_INIT(data_api_get);
    IB_PROVIDER_IFACE_TYPE(data) *iface = dpi?(IB_PROVIDER_IFACE_TYPE(data) *)dpi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(dpi->pr->ib, 0, "Failed to fetch data interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->get(dpi, name, nlen, pf);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Calls a registered provider interface to remove a data field in a
 * provider instance.
 *
 * @param dpi Data provider instance
 * @param name Field name
 * @param nlen Field name length
 * @param pf Address which removed field is written (if non-NULL)
 * 
 * @returns Status code
 */
static ib_status_t data_api_remove(ib_provider_inst_t *dpi,
                                   const char *name,
                                   size_t nlen,
                                   ib_field_t **pf)
{
    IB_FTRACE_INIT(data_api_remove);
    IB_PROVIDER_IFACE_TYPE(data) *iface = dpi?(IB_PROVIDER_IFACE_TYPE(data) *)dpi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(dpi->pr->ib, 0, "Failed to fetch data interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->remove(dpi, name, nlen, pf);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Calls a registered provider interface to clear all fields from a
 * provider instance.
 *
 * @param dpi Data provider instance
 * 
 * @returns Status code
 */
static ib_status_t data_api_clear(ib_provider_inst_t *dpi)
{
    IB_FTRACE_INIT(data_api_clear);
    IB_PROVIDER_IFACE_TYPE(data) *iface = dpi?(IB_PROVIDER_IFACE_TYPE(data) *)dpi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(dpi->pr->ib, 0, "Failed to fetch data interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->clear(dpi);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Data access provider API mapping for core module.
 */
static IB_PROVIDER_API_TYPE(data) data_api = {
    data_api_add,
    data_api_set,
    data_api_set_relative,
    data_api_get,
    data_api_remove,
    data_api_clear,
};

/**
 * @internal
 * Data access provider registration function.
 *
 * This just does a version and sanity check on a registered provider.
 *
 * @param ib Engine
 * @param lpr Logger provider
 *
 * @returns Status code
 */
static ib_status_t data_register(ib_engine_t *ib,
                                 ib_provider_t *pr)
{
    IB_FTRACE_INIT(data_register);
    IB_PROVIDER_IFACE_TYPE(data) *iface = (IB_PROVIDER_IFACE_TYPE(data) *)pr->iface;

    /* Check that versions match. */
    if (iface->version != IB_PROVIDER_VERSION_DATA) {
        IB_FTRACE_RET_STATUS(IB_EINCOMPAT);
    }

    /* Verify that required interface functions are implemented. */
    if (   (iface->add == NULL)
        || (iface->set == NULL)
        || (iface->set_relative == NULL)
        || (iface->get == NULL)
        || (iface->remove == NULL)
        || (iface->clear == NULL))
    {
        ib_log_error(ib, 0, "All required interface functions "
                            "MUST be implemented by a data provider");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Inintialize the data access provider instance.
 *
 * @param dpi Data provider instance
 * @param data Initalization data
 *
 * @returns Status code
 */
static ib_status_t data_init(ib_provider_inst_t *dpi,
                             void *data)
{
    IB_FTRACE_INIT(data_init);
    ib_status_t rc;
    ib_hash_t *ht;

    rc = ib_hash_create(&ht, dpi->mp);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    dpi->data = (void *)ht;

    ib_log_debug(dpi->pr->ib, 9, "Initialized core data provider instance: %p", dpi);

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Matcher Implementation -- */

/**
 * @internal
 * Compile a pattern.
 *
 * @param mpr Matcher provider
 * @param pool Memory pool
 * @param pcpatt Address which compiled pattern is written
 * @param patt Pattern
 * @param errptr Address which any error is written (if non-NULL)
 * @param erroffset Offset in pattern where the error occured (if non-NULL)
 *
 * @returns Status code
 */
static ib_status_t matcher_api_compile_pattern(ib_provider_t *mpr,
                                               ib_mpool_t *pool,
                                               void *pcpatt,
                                               const char *patt,
                                               const char **errptr,
                                               int *erroffset)

{
    IB_FTRACE_INIT(matcher_api_compile_pattern);
    IB_PROVIDER_IFACE_TYPE(matcher) *iface = mpr?(IB_PROVIDER_IFACE_TYPE(matcher) *)mpr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_util_log_error(0, "Failed to fetch matcher interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    if (iface->compile == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
    }

    rc = iface->compile(mpr, pool, pcpatt, patt, errptr, erroffset);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Match a compiled pattern against a buffer.
 *
 * @param mpr Matcher provider
 * @param cpatt Compiled pattern
 * @param flags Flags
 * @param data Data buffer to perform match on
 * @param dlen Data buffer length
 *
 * @returns Status code
 */
static ib_status_t matcher_api_match_compiled(ib_provider_t *mpr,
                                              void *cpatt,
                                              ib_flags_t flags,
                                              const uint8_t *data,
                                              size_t dlen)
{
    IB_FTRACE_INIT(matcher_api_match_compiled);
    IB_PROVIDER_IFACE_TYPE(matcher) *iface = mpr?(IB_PROVIDER_IFACE_TYPE(matcher) *)mpr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_util_log_error(0, "Failed to fetch matcher interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    if (iface->match_compiled == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
    }

    rc = iface->match_compiled(mpr, cpatt, flags, data, dlen);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Add a pattern to a matcher provider instance.
 *
 * Multiple patterns can be added to a provider instance and all used
 * to perform a match later on.
 *
 * @param mpi Matcher provider instance
 * @param patt Pattern
 *
 * @returns Status code
 */
static ib_status_t matcher_api_add_pattern(ib_provider_inst_t *mpi,
                                           const char *patt)
{
    IB_FTRACE_INIT(matcher_api_add_pattern);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

/**
 * @internal
 * Match all the provider instance patterns on a data field.
 *
 * @warning Not yet implemented
 *
 * @param mpi Matcher provider instance
 * @param flags Flags
 * @param data Data buffer
 * @param dlen Data buffer length
 *
 * @returns Status code
 */
static ib_status_t matcher_api_match(ib_provider_inst_t *mpi,
                                     ib_flags_t flags,
                                     const uint8_t *data,
                                     size_t dlen)
                                     
{
    IB_FTRACE_INIT(matcher_api_match);
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

/**
 * @internal
 * Matcher provider API mapping for core module.
 */
static IB_PROVIDER_API_TYPE(matcher) matcher_api = {
    matcher_api_compile_pattern,
    matcher_api_match_compiled,
    matcher_api_add_pattern,
    matcher_api_match,
};

/**
 * @internal
 * Matcher provider registration function.
 *
 * This just does a version and sanity check on a registered provider.
 *
 * @param ib Engine
 * @param mpr Matcher provider
 *
 * @returns Status code
 */
static ib_status_t matcher_register(ib_engine_t *ib,
                                    ib_provider_t *mpr)
{
    IB_FTRACE_INIT(matcher_register);
    IB_PROVIDER_IFACE_TYPE(matcher) *iface = (IB_PROVIDER_IFACE_TYPE(matcher) *)mpr->iface;

    /* Check that versions match. */
    if (iface->version != IB_PROVIDER_VERSION_MATCHER) {
        IB_FTRACE_RET_STATUS(IB_EINCOMPAT);
    }

    /* Verify that required interface functions are implemented. */
    /// @todo

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Directive Handlers -- */

/**
 * @internal
 * Make an absolute filename out of a base directory and relative filename.
 *
 * @todo Needs to not assume the trailing slash will be there.
 * 
 * @param ib Engine
 * @param basedir Base directory
 * @param file Relative filename
 * @param pabsfile Address which absolute path is written
 *
 * @returns Status code
 */
static ib_status_t core_abs_module_path(ib_engine_t *ib,
                                        const char *basedir,
                                        const char *file,
                                        char **pabsfile)
{
    IB_FTRACE_INIT(core_abs_module_path);
    ib_mpool_t *pool = ib_engine_pool_config_get(ib);
    
    *pabsfile = (char *)ib_mpool_alloc(pool, strlen(basedir) + 1 + strlen(file) + 1);
    if (*pabsfile == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    strcpy(*pabsfile, basedir);
    strcat(*pabsfile, file);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Handle the start of a Site block.
 *
 * This function sets up the new site and pushes it onto the parser stack.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param p1 First parameter
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_site_start(ib_cfgparser_t *cp,
                                       const char *name,
                                       const char *p1,
                                       void *cbdata)
{
    IB_FTRACE_INIT(core_dir_site_start);
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;
    ib_site_t *site;
    ib_loc_t *loc;
    ib_status_t rc;
    ib_module_t *m;
    size_t ne;
    size_t i;

    ib_log_debug(ib, 4, "Creating site \"%s\"", p1);
    rc = ib_site_create(&site, ib, p1);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "Failed to create site \"%s\": %d", rc);
    }

    ib_log_debug(ib, 4, "Creating default location for site \"%s\"", p1);
    rc = ib_site_loc_create_default(site, &loc);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "Failed to create default location for site \"%s\": %d", p1, rc);
    }

    ib_log_debug(ib, 4, "Creating context for \"%s:%s\"", p1, loc->path);
    rc = ib_context_create(&ctx, ib, ib_context_siteloc_chooser, loc);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "Failed to create context for \"%s:%s\": %d", p1, loc->path, rc);
    }
    ib_cfgparser_context_push(cp, ctx);

    /// @todo For now, register all modules with each context
    ib_log_debug(ib, 4, "Registering modules with site \"%s:%s\" context", p1, loc->path);
    IB_ARRAY_LOOP(ib->modules, ne, i, m) {
        if (   (m != NULL) && (m->fn_ctx_init != NULL)
            && (strcmp("core", m->name) != 0))
        {
            ib_log_debug(ib, 4, "Registering module: %s", m->name);
            ib_module_register_context(m, ctx);
        }
    }

    ib_log_debug(ib, 4, "Stack: ctx=%p site=%p(%s) loc=%p",
                 cp->cur_ctx,
                 cp->cur_site, cp->cur_site?cp->cur_site->name:"NONE",
                 cp->cur_loc);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Handle the end of a Site block.
 *
 * This function closes out the site and pops it from the parser stack.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param p1 First parameter
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_site_end(ib_cfgparser_t *cp,
                                     const char *name,
                                     void *cbdata)
{
    IB_FTRACE_INIT(core_dir_site_end);
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;
    ib_status_t rc;

    ib_log_debug(ib, 4, "Processing site block \"%s\"", name);

    /* Pop the current items off the stack */
    rc = ib_cfgparser_context_pop(cp, &ctx);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "Failed to pop context for \"%s\": %d", name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug(ib, 4, "Initializing context for \"%s\"", name);
    rc = ib_context_init(ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Error initializing context for \"%s\": %d",
                     name, rc);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_log_debug(ib, 4, "Stack: lastctx=%p ctx=%p site=%p(%s) loc=%p",
                 ctx, cp->cur_ctx,
                 cp->cur_site, cp->cur_site?cp->cur_site->name:"NONE",
                 cp->cur_loc);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Handle the start of a Location block.
 *
 * This function sets up the new locationand pushes it onto the parser stack.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param p1 First parameter
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_loc_start(ib_cfgparser_t *cp,
                                      const char *name,
                                      const char *p1,
                                      void *cbdata)
{
    IB_FTRACE_INIT(core_dir_loc_start);
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;
    ib_site_t *site = cp->cur_site;
    ib_loc_t *loc;
    ib_status_t rc;
    ib_module_t *m;
    size_t ne;
    size_t i;

    ib_log_debug(ib, 4, "Creating location \"%s\" for site \"%s\"", p1, site->name);
    rc = ib_site_loc_create(site, &loc, p1);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "Failed to create location \"%s:%s\": %d", site->name, p1, rc);
    }

    ib_log_debug(ib, 4, "Creating context for \"%s:%s\"", site->name, loc->path);
    rc = ib_context_create(&ctx, ib, ib_context_siteloc_chooser, loc);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "Failed to create context for \"%s:%s\": %d", site->name, loc->path, rc);
    }
    ib_cfgparser_context_push(cp, ctx);

    /// @todo For now, register all modules with each context
    ib_log_debug(ib, 4, "Registering modules with site \"%s:%s\" context", site->name, loc->path);
    IB_ARRAY_LOOP(ib->modules, ne, i, m) {
        if (   (m != NULL) && (m->fn_ctx_init != NULL)
            && (strcmp("core", m->name) != 0))
        {
            ib_log_debug(ib, 4, "Registering module: %s", m->name);
            ib_module_register_context(m, ctx);
        }
    }

    ib_log_debug(ib, 4, "Stack: ctx=%p site=%p(%s) loc=%p",
                 cp->cur_ctx,
                 cp->cur_site, cp->cur_site?cp->cur_site->name:"NONE",
                 cp->cur_loc);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Handle the end of a Location block.
 *
 * This function closes out the location and pops it from the parser stack.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param p1 First parameter
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_loc_end(ib_cfgparser_t *cp,
                                    const char *name,
                                    void *cbdata)
{
    IB_FTRACE_INIT(core_dir_loc_end);
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;
    ib_status_t rc;

    ib_log_debug(ib, 4, "Processing location block \"%s\"", name);

    /* Pop the current items off the stack */
    rc = ib_cfgparser_context_pop(cp, &ctx);
    if (rc != IB_OK) {
        ib_log_debug(ib, 4, "Failed to pop context for \"%s\": %d", name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug(ib, 4, "Initializing context for \"%s\"", name);
    rc = ib_context_init(ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Error initializing context for \"%s\": %d",
                     name, rc);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_log_debug(ib, 4, "Stack: lastctx=%p ctx=%p site=%p(%s) loc=%p",
                 ctx, cp->cur_ctx,
                 cp->cur_site, cp->cur_site?cp->cur_site->name:"NONE",
                 cp->cur_loc);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Handle a Hostname directive.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param args List of directive arguments
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_hostname(ib_cfgparser_t *cp,
                                     const char *name,
                                     ib_list_t *args,
                                     void *cbdata)
{
    IB_FTRACE_INIT(core_dir_hostname);
    ib_engine_t *ib = cp->ib;
    ib_list_node_t *node;
    ib_status_t rc = IB_EINVAL;

    IB_LIST_LOOP(args, node) {
        char *p = (char *)ib_list_node_data(node);

        if (strncasecmp("ip=", p, 3) == 0) {
            p += 3; /* Skip over ip= */
            ib_log_debug(ib, 4, "Adding IP \"%s\" to site \"%s\"",
                         p, cp->cur_site->name);
            rc = ib_site_address_add(cp->cur_site, p);
        }
        else if (strncasecmp("path=", p, 5) == 0) {
            //p += 5; /* Skip over path= */
            ib_log_debug(ib, 4, "TODO: Handle: %s %p", name, p);
        }
        else if (strncasecmp("port=", p, 5) == 0) {
            //p += 5; /* Skip over port= */
            ib_log_debug(ib, 4, "TODO: Handle: %s %p", name, p);
        }
        else {
            /// @todo Handle full wildcards
            if (*p == '*') {
                /* Currently we do a match on the end of the host, so
                 * just skipping over the wildcard (assuming only one)
                 * for now.
                 */
                p++;
            }
            ib_log_debug(ib, 4, "Adding host \"%s\" to site \"%s\"",
                         p, cp->cur_site->name);
            rc = ib_site_hostname_add(cp->cur_site, p);
        }
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Handle single parameter directives.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param p1 First parameter
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_param1(ib_cfgparser_t *cp,
                                   const char *name,
                                   const char *p1,
                                   void *cbdata)
{
    IB_FTRACE_INIT(core_dir_param1);
    ib_engine_t *ib = cp->ib;
    ib_status_t rc;

    if (strcasecmp("InspectionEngine", name) == 0) {
        ib_log_debug(ib, 4, "TODO: Handle Directive: %s \"%s\"", name, p1);
    }
    else if (strcasecmp("AuditEngine", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        ib_log_debug(ib, 4, "Setting: %s \"%s\"", name, p1);
        if (strcasecmp("On", p1) == 0) {
            ib_context_set_num(ctx, "audit_engine", 1);
        }
        else if (strcasecmp("Off", p1) == 0) {
            ib_context_set_num(ctx, "audit_engine", 0);
        }
        else {
            ib_log_error(ib, 1, "Failed to parse directive: %s \"%s\"", name, p1);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }
    else if (strcasecmp("AuditLog", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        ib_log_debug(ib, 4, "Setting: %s \"%s\"", name, p1);
        ib_context_set_string(ctx, "auditlog", p1);
    }
    else if (strcasecmp("AuditLogStorageDir", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        ib_log_debug(ib, 4, "Setting: %s \"%s\"", name, p1);
        ib_context_set_string(ctx, "auditlog_dir", p1);
    }
    else if (strcasecmp("LoadModule", name) == 0) {
        char *absfile;
        ib_context_t *ctx = ib_context_main(ib);
        ib_module_t *m;

        if (*p1 == '/') {
            absfile = (char *)p1;
        }
        else {
            rc = core_abs_module_path(ib, X_MODULE_BASE_PATH, p1, &absfile);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }

        rc = ib_module_load(&m, ib, absfile);
        if (rc != IB_OK) {
            ib_log_error(ib, 2, "Failed to load module \"%s\": %d", p1, rc);
            IB_FTRACE_RET_STATUS(IB_ENOENT);
        }

        ib_log_debug(ib, 4, "Registering module \"%s\" with main context", p1);
        ib_module_register_context(m, ctx);
    }
    else {
        ib_log_error(ib, 1, "Unhandled directive: %s %s", name, p1);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Handle two parameter directives.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param p1 First parameter
 * @param p2 Second parameter
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_param2(ib_cfgparser_t *cp,
                                   const char *name,
                                   const char *p1,
                                   const char *p2,
                                   void *cbdata)
{
    IB_FTRACE_INIT(core_dir_param2);
    ib_engine_t *ib = cp->ib;
    //ib_status_t rc;

    if (strcasecmp("Set", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        void *val;
        ib_ftype_t type;

        ib_context_get(ctx, p1, &val, &type);
        switch(type) {
            case IB_FTYPE_NULSTR:
                ib_context_set_string(ctx, p1, p2);
                break;
            case IB_FTYPE_NUM:
                ib_context_set_num(ctx, p1, atol(p2));
                break;
            default:
                ib_log_error(ib, 3,
                             "Can only set string(%d) or numeric(%d) "
                             "types, but %s was type=%d",
                             IB_FTYPE_NULSTR, IB_FTYPE_NUM,
                             p1, type);
                IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }
    else {
        ib_log_error(ib, 1, "Unhandled directive: %s %s %s", name, p1, p2);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Directive initialization structure.
 */
static IB_DIRMAP_INIT_STRUCTURE(core_directive_map) = {
    /* Modules */
    IB_DIRMAP_INIT_PARAM1(
        "LoadModule",
        core_dir_param1,
        NULL,
        NULL
    ),

    /* Parameters */
    IB_DIRMAP_INIT_PARAM2(
        "Set",
        core_dir_param2,
        NULL,
        NULL
    ),

    /* Config */
    IB_DIRMAP_INIT_SBLK1(
        "Site",
        core_dir_site_start,
        core_dir_site_end,
        NULL
    ),
    IB_DIRMAP_INIT_SBLK1(
        "Location",
        core_dir_loc_start,
        core_dir_loc_end,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "Hostname",
        core_dir_hostname,
        NULL,
        NULL
    ),

    /* Inspection Engine */
    IB_DIRMAP_INIT_PARAM1(
        "InspectionEngine",
        core_dir_param1,
        NULL,
        NULL
    ),

    /* Audit Engine */
    IB_DIRMAP_INIT_PARAM1(
        "AuditEngine",
        core_dir_param1,
        NULL,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "AuditLog",
        core_dir_param1,
        NULL,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "AuditLogStorageDir",
        core_dir_param1,
        NULL,
        NULL
    ),

    /* End */
    IB_DIRMAP_INIT_LAST
};


/* -- Module Routines -- */

/**
 * @internal
 * Initialize the core module on load.
 *
 * @param ib Engine
 *
 * @returns Status code
 */
static ib_status_t core_init(ib_engine_t *ib)
{
    IB_FTRACE_INIT(core_init);
    ib_provider_t *core_log_provider;
    ib_provider_t *core_audit_provider;
    ib_provider_t *core_data_provider;
    ib_status_t rc;

    /* Define the logger provider API. */
    rc = ib_provider_define(ib, IB_PROVIDER_TYPE_LOGGER,
                            logger_register, &logger_api);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the core logger provider. */
    rc = ib_provider_register(ib, IB_PROVIDER_TYPE_LOGGER,
                              MODULE_NAME_STR, &core_log_provider,
                              &core_logger_iface,
                              logger_init);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    /// @todo Move to using provider instance
    ib_provider_data_set(core_log_provider, stderr);

    /* Force any IBUtil calls to use the default logger */
    rc = ib_util_log_logger((ib_util_fn_logger_t)ib_vclog_ex, ib->ctx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Define the logevent provider API. */
    rc = ib_provider_define(ib, IB_PROVIDER_TYPE_LOGEVENT,
                            logevent_register, &logevent_api);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the core logevent provider. */
    rc = ib_provider_register(ib, IB_PROVIDER_TYPE_LOGEVENT,
                              MODULE_NAME_STR, NULL,
                              &core_logevent_iface,
                              logevent_init);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Define the audit provider API. */
    rc = ib_provider_define(ib, IB_PROVIDER_TYPE_AUDIT,
                            audit_register, &audit_api);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the core audit provider. */
    rc = ib_provider_register(ib, IB_PROVIDER_TYPE_AUDIT,
                              MODULE_NAME_STR, &core_audit_provider,
                              &core_audit_iface,
                              NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Define the parser provider API. */
    rc = ib_provider_define(ib, IB_PROVIDER_TYPE_PARSER,
                            parser_register, NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to define parser provider: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register parser hooks. */
    ib_hook_register(ib, conn_started_event,
                     (ib_void_fn_t)parser_hook_init, NULL);
    ib_hook_register(ib, handle_connect_event,
                     (ib_void_fn_t)parser_hook_connect, NULL);
    ib_hook_register(ib, handle_disconnect_event,
                     (ib_void_fn_t)parser_hook_disconnect, NULL);
    ib_hook_register(ib, conn_data_in_event,
                     (ib_void_fn_t)parser_hook_data_in, NULL);
    ib_hook_register(ib, conn_data_out_event,
                     (ib_void_fn_t)parser_hook_data_out, NULL);
    /// @todo Need the parser to parser headers before context, but others after context so that the personality can change based on headers (Host, uri path, etc)
    //ib_hook_register(ib, handle_context_tx_event, (void *)parser_hook_req_header, NULL);
    ib_hook_register(ib, request_headers_event,
                     (ib_void_fn_t)parser_hook_req_header, NULL);
    ib_hook_register(ib, response_headers_event,
                     (ib_void_fn_t)parser_hook_resp_header, NULL);

    /* Register logevent hooks. */
    ib_hook_register(ib, handle_postprocess_event,
                     (ib_void_fn_t)logevent_hook_postprocess, NULL);

    /* Define the data field provider API */
    rc = ib_provider_define(ib, IB_PROVIDER_TYPE_DATA,
                            data_register, &data_api);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to define data provider: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Register the core data provider. */
    rc = ib_provider_register(ib, IB_PROVIDER_TYPE_DATA,
                              MODULE_NAME_STR, &core_data_provider,
                              &core_data_iface,
                              data_init);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Define the matcher provider API */
    rc = ib_provider_define(ib, IB_PROVIDER_TYPE_MATCHER,
                            matcher_register, &matcher_api);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to define matcher provider: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Finialize the core module on unload.
 *
 * @param ib Engine
 *
 * @returns Status code
 */
static ib_status_t core_fini(ib_engine_t *ib)
{
    IB_FTRACE_INIT(core_fini);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Initialize the core module when it registered with a context.
 *
 * @param ib Engine
 * @param ctx Context
 *
 * @returns Status code
 */
static ib_status_t core_config_init(ib_engine_t *ib,
                                    ib_context_t *ctx)
{
    IB_FTRACE_INIT(core_config_init);
    ib_provider_inst_t *logger;
    ib_provider_inst_t *logevent;
    ib_provider_inst_t *parser;
    ib_status_t rc;

    /* Get the core module config. */
    rc = ib_context_module_config(ctx, ib_core_module(),
                                  (void *)&(ctx->core_cfg));
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to fetch core module config: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Lookup/set logger provider. */
    rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_LOGGER,
                                     ctx->core_cfg->logger, &logger,
                                     ib->mp, NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to create %s provider instance: %d", IB_PROVIDER_TYPE_LOGGER, rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_log_provider_set_instance(ctx, logger);

    /* Lookup/set logevent provider. */
    rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_LOGEVENT,
                                     ctx->core_cfg->logevent, &logevent,
                                     ib->mp, NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to create %s provider instance: %d", IB_PROVIDER_TYPE_LOGEVENT, rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_logevent_provider_set_instance(ctx, logevent);

    /* Lookup/set parser provider if not the "core" parser. */
    ib_log_debug(ib, 9, "PARSER: %s ctx=%p", ctx->core_cfg->parser, ctx);
    if (strcmp(MODULE_NAME_STR, ctx->core_cfg->parser) != 0) {
        rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_PARSER,
                                         ctx->core_cfg->parser, &parser,
                                         ib->mp, NULL);
        if (rc != IB_OK) {
            ib_log_error(ib, 0, "Failed to create %s provider instance: %d", IB_PROVIDER_TYPE_PARSER, rc);
            IB_FTRACE_RET_STATUS(rc);
        }
        ib_parser_provider_set_instance(ctx, parser);
        IB_FTRACE_MSG("PARSER");
        IB_FTRACE_MSG(ctx->parser->pr->type);

    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Core module configuration parameter initialization structure.
 */
static IB_CFGMAP_INIT_STRUCTURE(core_config_map) = {
    /* Logger */
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_LOGGER,
        IB_FTYPE_NULSTR,
        &core_global_cfg,
        logger,
        (const uintptr_t)MODULE_NAME_STR
    ),
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_LOGGER ".log_level",
        IB_FTYPE_NUM,
        &core_global_cfg,
        log_level,
        4
    ),
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_LOGGER ".log_uri",
        IB_FTYPE_NULSTR,
        &core_global_cfg,
        log_uri,
        ""
    ),

    /* Logevent */
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_LOGEVENT,
        IB_FTYPE_NULSTR,
        &core_global_cfg,
        logevent,
        (const uintptr_t)MODULE_NAME_STR
    ),

    /* Parser */
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_PARSER,
        IB_FTYPE_NULSTR,
        &core_global_cfg,
        parser,
        MODULE_NAME_STR
    ),

    /* Audit Log */
    IB_CFGMAP_INIT_ENTRY(
        "audit_engine",
        IB_FTYPE_NUM,
        &core_global_cfg,
        audit_engine,
        /// @todo More appropriate default
        0
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog",
        IB_FTYPE_NULSTR,
        &core_global_cfg,
        auditlog,
        /// @todo More appropriate default
        "/tmp/ironbee-auditlog-index.log"
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog_dir",
        IB_FTYPE_NULSTR,
        &core_global_cfg,
        auditlog_dir,
        /// @todo More appropriate default
        "/tmp/ironbee-auditlog-data"
    ),
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_AUDIT,
        IB_FTYPE_NULSTR,
        &core_global_cfg,
        audit,
        MODULE_NAME_STR
    ),

    /* Data Aquisition */
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_DATA,
        IB_FTYPE_NULSTR,
        &core_global_cfg,
        data,
        MODULE_NAME_STR
    ),

    /* End */
    IB_CFGMAP_INIT_LAST
};

/**
 * @internal
 * Static core module structure.
 *
 * This is a bit of a hack so that the core module can be compiled in (static)
 * but still appear as if it was loaded dynamically.
 */
IB_MODULE_INIT_STATIC(
    ib_core_module,                      /**< Static module name */
    IB_MODULE_HEADER_DEFAULTS,           /**< Default metadata */
    MODULE_NAME_STR,                     /**< Module name */
    IB_MODULE_CONFIG(&core_global_cfg),  /**< Global config data */
    core_config_map,                     /**< Configuration field map */
    core_directive_map,                  /**< Config directive map */
    core_init,                           /**< Initialize function */
    core_fini,                           /**< Finish function */
    core_config_init,                    /**< Context init function */
);
