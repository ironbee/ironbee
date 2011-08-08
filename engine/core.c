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
#include <sys/time.h> /* gettimeofday */
#include <arpa/inet.h> /* htonl */

#include <ctype.h> /* tolower */
#include <time.h>
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

#define IB_ALPART_HEADER                  (1<< 0)
#define IB_ALPART_EVENTS                  (1<< 1)
#define IB_ALPART_HTTP_REQUEST_METADATA   (1<< 2)
#define IB_ALPART_HTTP_REQUEST_HEADERS    (1<< 3)
#define IB_ALPART_HTTP_REQUEST_BODY       (1<< 4)
#define IB_ALPART_HTTP_REQUEST_TRAILERS   (1<< 5)
#define IB_ALPART_HTTP_RESPONSE_METADATA  (1<< 6)
#define IB_ALPART_HTTP_RESPONSE_HEADERS   (1<< 7)
#define IB_ALPART_HTTP_RESPONSE_BODY      (1<< 8)
#define IB_ALPART_HTTP_RESPONSE_TRAILERS  (1<< 9)
#define IB_ALPART_DEBUG_FIELDS            (1<<10)

/* NOTE: Make sure to add new parts from above to any groups below. */

#define IB_ALPARTS_ALL \
    IB_ALPART_HEADER|IB_ALPART_EVENTS| \
    IB_ALPART_HTTP_REQUEST_METADATA|IB_ALPART_HTTP_REQUEST_HEADERS|\
    IB_ALPART_HTTP_REQUEST_BODY|IB_ALPART_HTTP_REQUEST_TRAILERS| \
    IB_ALPART_HTTP_RESPONSE_METADATA|IB_ALPART_HTTP_RESPONSE_HEADERS| \
    IB_ALPART_HTTP_RESPONSE_BODY|IB_ALPART_HTTP_RESPONSE_TRAILERS| \
    IB_ALPART_DEBUG_FIELDS

#define IB_ALPARTS_DEFAULT \
    IB_ALPART_HEADER|IB_ALPART_EVENTS| \
    IB_ALPART_HTTP_REQUEST_METADATA|IB_ALPART_HTTP_REQUEST_HEADERS|\
    IB_ALPART_HTTP_REQUEST_TRAILERS| \
    IB_ALPART_HTTP_RESPONSE_METADATA|IB_ALPART_HTTP_RESPONSE_HEADERS| \
    IB_ALPART_HTTP_RESPONSE_TRAILERS

#define IB_ALPARTS_REQUEST \
    IB_ALPART_HTTP_REQUEST_METADATA|IB_ALPART_HTTP_REQUEST_HEADERS|\
    IB_ALPART_HTTP_REQUEST_BODY|IB_ALPART_HTTP_REQUEST_TRAILERS

#define IB_ALPARTS_RESPONSE \
    IB_ALPART_HTTP_RESPONSE_METADATA|IB_ALPART_HTTP_RESPONSE_HEADERS| \
    IB_ALPART_HTTP_RESPONSE_BODY|IB_ALPART_HTTP_RESPONSE_TRAILERS


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
    FILE           *index_fp;      /* kept NULL if no index is wanted */
    FILE           *fp;
    const char     *fn;
    int             parts_written;
    const char     *boundary;
    ib_tx_t        *tx;
    ib_timeval_t   *logtime;
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
    ib_core_cfg_t *corecfg;
    ib_status_t rc;
    size_t fnsize;
    char *fn;
    int ec;

    rc = ib_context_module_config(log->ctx, ib_core_module(),
                                  (void *)&corecfg);

    if (   (strcmp(corecfg->auditlog_index, "/dev/null") != 0)
        && (strcmp(corecfg->auditlog_index, "NUL")       != 0))
    {
        /// @todo fclose(cfg->index_fp) if open?
        //        should this be done on any call to core_audit_open(),
        //        on the assumption we're re-initializing logging?
        ib_log_debug(log->ib, 4, "Skipping open of AuditLogIndex");
    }
    else if (cfg->index_fp == NULL) {
        if (corecfg->auditlog_index[0] == '/') {
            fnsize = strlen(corecfg->auditlog_index) + 1;

            fn = (char *)ib_mpool_alloc(cfg->tx->mp, fnsize);
            if (fn == NULL) {
                return IB_EALLOC;
            }
        }
        else {
            fnsize = strlen(corecfg->auditlog_dir) +
                     strlen(corecfg->auditlog_index) + 2;

            rc = ib_util_mkpath(corecfg->auditlog_dir,
                                corecfg->auditlog_dmode);
            if (rc != IB_OK) {
                ib_log_error(log->ib, 1,
                             "Could not create audit log dir: %s",
                             corecfg->auditlog_dir);
                IB_FTRACE_RET_STATUS(rc);
            }

            fn = (char *)ib_mpool_alloc(cfg->tx->mp, fnsize);
            if (fn == NULL) {
                return IB_EALLOC;
            }

            ec = snprintf(fn, fnsize, "%s/%s",
                              corecfg->auditlog_dir, corecfg->auditlog_index);
            if (ec >= (int)fnsize) {
                ib_log_error(log->ib, 1,
                             "Could not create audit log index filename \"%s/%s\":"
                             " too long",
                             corecfg->auditlog_dir, corecfg->auditlog_index);
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }
        }

        /// @todo Use corecfg->auditlog_fmode as file mode for new file
        cfg->index_fp = fopen(fn, "ab");
        if (cfg->index_fp == NULL) {
            ec = errno;
            ib_log_error(log->ib, 1,
                         "Could not open audit log index \"%s\": %s (%d)",
                         fn, strerror(ec), ec);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        ib_log_debug(log->ib, 4, "AUDITLOG INDEX: %s", fn);
    }

    if (cfg->fp == NULL) {
        char dtmp[64]; /// @todo Allocate size???
        char dn[512]; /// @todo Allocate size???
        struct tm *tm;
        size_t ret;

        tm = gmtime(&cfg->logtime->tv_sec);
        if (tm == 0) {
            ib_log_error(log->ib, 1,
                         "Could not create audit log filename template:"
                         " too long");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Generate the audit log filename template. */
        /// @todo Make this template configurable
        /*
        if (   (corecfg->auditlog_sdir_fmt != NULL)
            && (*(corecfg->auditlog_sdir_fmt) != 0))
            */
        if (*(corecfg->auditlog_sdir_fmt) != 0) {
            ret = strftime(dtmp, sizeof(dtmp),
                           corecfg->auditlog_sdir_fmt, tm);
            if (ret == 0) {
                /// @todo Better error - probably should validate at cfg time
                ib_log_error(log->ib, 1,
                             "Could not create audit log filename template, "
                             "using default:"
                             " too long");
                *dtmp = 0;
            }
        }
        else {
            *dtmp = 0;
        }

        /* Generate the full audit log directory name. */
        ec = snprintf(dn, sizeof(dn), "%s%s%s",
                      corecfg->auditlog_dir, (*dtmp)?"/":"", dtmp);
        if (ec >= (int)sizeof(dn)) {
            /// @todo Better error.
            ib_log_error(log->ib, 1,
                         "Could not create audit log directory: too long",
                         corecfg->auditlog_dir, corecfg->auditlog_index);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Generate the full audit log filename. */
        fnsize = strlen(dn) + strlen(cfg->tx->id) + 6;
        fn = (char *)ib_mpool_alloc(cfg->tx->mp, fnsize);
        ec = snprintf(fn, fnsize, "%s/%s.log", dn, cfg->tx->id);
        if (ec >= (int)fnsize) {
            /// @todo Better error.
            ib_log_error(log->ib, 1,
                         "Could not create audit log filename: too long");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        rc = ib_util_mkpath(dn, corecfg->auditlog_dmode);
        if (rc != IB_OK) {
            ib_log_error(log->ib, 1,
                         "Could not create audit log dir: %s", dn);
            IB_FTRACE_RET_STATUS(rc);
        }

        /// @todo Use corecfg->auditlog_fmode as file mode for new file
        cfg->fp = fopen(fn, "ab");
        if (cfg->fp == NULL) {
            ec = errno;
            /// @todo Better error.
            ib_log_error(log->ib, 1, "Could not open audit log \"%s\": %s (%d)",
                         fn, strerror(ec), ec);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Track the relative audit log filename. */
        cfg->fn = fn + (strlen(corecfg->auditlog_dir) + 1);

        ib_log_debug(log->ib, 4, "AUDITLOG: %s", fn);
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
    ib_tx_t *tx = log->tx;
    ib_conn_t *conn = tx->conn;

    /* Close the audit log. */
    if (cfg->fp != NULL) {
        fclose(cfg->fp);
        cfg->fp = NULL;
    }

    /* Write to the index file:
     *  hostname (or IP)
     *  source IP
     *  remote user
     *  local user
     *  timestamp
     *  request line
     *  response status
     *  bytes sent
     *  referrer
     *  user agent
     *  transaction id
     *  session id
     *  audit log filename (relative)
     *  audit log offset
     *  audit log size
     *  audit log hash
     */
    if (cfg->index_fp && cfg->parts_written > 0) {
        fprintf(
            cfg->index_fp,
            "%s %s %s %s [%s] \"%s\" %d %d \"%s\" \"%s\" %s \"%s\" /%s %d %d %s\n",
            tx->hostname,
            conn->remote_ipstr,
            "-",
            "-",
            "-",
            "-",
            0,
            0,
            "-",
            "-",
            tx->id,
            "-",
            cfg->fn,
            0,
            0,
            "-"
        );
        fflush(cfg->index_fp);
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
                                 ib_field_t *f,
                                 const char *name,
                                 size_t nlen)
{
    IB_FTRACE_INIT(core_data_add);
    /// @todo Needs to be more field-aware (handle lists, etc)
    /// @todo Needs to not allow adding if already exists (except list items)
    ib_status_t rc = ib_hash_set_ex((ib_hash_t *)dpi->data,
                                    (void *)name, nlen, f);
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
                                 ib_field_t *f,
                                 const char *name,
                                 size_t nlen)
{
    IB_FTRACE_INIT(core_data_set);
    /// @todo Needs to be more field-aware (handle lists, etc)
    ib_status_t rc = ib_hash_set_ex((ib_hash_t *)dpi->data,
                                    (void *)name, nlen, f);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Core data provider implementation to set a relative data field value.
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
            *(ib_field_value_num(f)) += adjval;
            break;
        case IB_FTYPE_UNUM:
            /// @todo Make sure this is atomic
            /// @todo Check for overflow
            *(ib_field_value_unum(f)) += adjval;
            break;
        default:
            IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
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
static ib_status_t core_data_get(ib_provider_inst_t *dpi,
                                 const char *name,
                                 size_t nlen,
                                 ib_field_t **pf)
{
    IB_FTRACE_INIT(core_data_get);
    const char *subkey;
    size_t klen;
    size_t sklen;
    ib_status_t rc;

    /* Allow "key.subkey" syntax, but still fall through
     * to a full key lookup if that fails.
     */
    if ((subkey = strchr(name, '.')) != NULL) {
        subkey += 1; /* skip over "." */
        klen = (subkey - name) - 1;
        sklen = nlen - klen - 1;

        rc = ib_hash_get_ex((ib_hash_t *)dpi->data,
                            (void *)name, klen,
                            (void *)pf);
        if (rc == IB_OK) {
            if ((*pf)->type == IB_FTYPE_LIST) {
                ib_list_node_t *node;

                /* Lookup the subkey value in the field list. */
                IB_LIST_LOOP(ib_field_value_list(*pf), node) {
                    ib_field_t *sf = (ib_field_t *)ib_list_node_data(node);

                    if (   (sf->nlen == sklen)
                        && (strncasecmp(sf->name, subkey, sklen) == 0))
                    {
                        *pf = sf;
                        IB_FTRACE_RET_STATUS(IB_OK);
                    }
                }
            }
        }
    }

    rc = ib_hash_get_ex((ib_hash_t *)dpi->data,
                                    (void *)name, nlen,
                                    (void *)pf);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Core data provider implementation to get a data all data fields.
 *
 * @param dpi Data provider instance
 * @param list List which fields will be pushed
 *
 * @returns Status code
 */
static ib_status_t core_data_get_all(ib_provider_inst_t *dpi,
                                     ib_list_t *list)
{
    IB_FTRACE_INIT(core_data_get);
    ib_status_t rc;

    rc = ib_hash_get_all((ib_hash_t *)dpi->data, list);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Core data provider implementation to remove a data field.
 *
 * The data field which is removed is written to @ref pf if it
 * is not NULL.
 *
 * @param dpi Data provider instance
 * @param name Field name
 * @param nlen Field name length
 * @param pf Address which field will be written if not NULL
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
    core_data_get_all,
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
    ib_core_cfg_t *corecfg;
    ib_status_t rc;

    rc = ib_context_module_config(ctx, ib_core_module(),
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        corecfg = &core_global_cfg;
    }

    if (level > (int)corecfg->log_level) {
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
    ib_core_cfg_t *corecfg;
    ib_status_t rc;
    va_list ap;

    rc = ib_context_module_config(ctx, ib_core_module(),
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        corecfg = &core_global_cfg;
    }

    if (level > (int)corecfg->log_level) {
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
 * @param lpi Logger provider instance
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
        ib_log_error(lpi->pr->ib, 4, "No parts to write to audit log");
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
            ib_log_error(log->ib, 4, "Failed to write audit log part: %s",
                         part->name);
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

#if 0
static size_t ib_auditlog_gen_raw(ib_auditlog_part_t *part,
                                  const uint8_t **chunk)
{
    ib_engine_t *ib = part->log->ib;
    ib_list_node_t *node;

    if (part->gen_data == NULL) {
        ib_list_t *list = (ib_list_t *)part->part_data;

        /* No data. */
        if (ib_list_elements(list) == 0) {
            ib_log_error(ib, 4, "No data in audit log part: %s", part->name);
            *chunk = NULL;
            part->gen_data = (void *)-1;
            return 0;
        }

        node = ib_list_first(list);
        /// @todo Probably node data needs to be ib_bytestr_t instead
        *chunk = (const uint8_t *)ib_list_node_data(node);

        node = ib_list_node_next(node);
        if (node != NULL) {
            part->gen_data = node;
        }
        else {
            part->gen_data = (void *)1;
        }

        /// @todo Need length
        return strlen(*(const char **)chunk);
    }
    else if (part->gen_data == (void *)-1) {
        part->gen_data = NULL;
        return 0;
    }

    node = (ib_list_node_t *)part->gen_data;
    *chunk = (const uint8_t *)ib_list_node_data(node);

    node = ib_list_node_next(node);
    if (node != NULL) {
        part->gen_data = node;
    }
    else {
        part->gen_data = (void *)1;
    }

    /// @todo Need length
    return strlen(*(const char **)chunk);
}
#endif

static size_t ib_auditlog_gen_json_flist(ib_auditlog_part_t *part,
                                         const uint8_t **chunk)
{
    ib_engine_t *ib = part->log->ib;
    ib_field_t *f;
    uint8_t *rec;
    size_t rlen;

#define CORE_JSON_MAX_FIELD_LEN 256
    
    /* The gen_data field is used to store the current state. NULL
     * means the part has not started yet and a -1 value
     * means it is done. Anything else is a node in the event list.
     */
    if (part->gen_data == NULL) {
        ib_list_t *list = (ib_list_t *)part->part_data;

        /* No data. */
        if (ib_list_elements(list) == 0) {
            ib_log_error(ib, 4, "No data in audit log part: %s", part->name);
            *chunk = (const uint8_t *)"{}";
            part->gen_data = (void *)-1;
            return strlen(*(const char **)chunk);
        }

        *chunk = (const uint8_t *)"{\r\n";
        part->gen_data = ib_list_first(list);
        return strlen(*(const char **)chunk);
    }
    else if (part->gen_data == (void *)-1) {
        part->gen_data = NULL;
        return 0;
    }

    f = (ib_field_t *)ib_list_node_data((ib_list_node_t *)part->gen_data);
    if (f != NULL) {
        const char *comma;
        rec = (uint8_t *)ib_mpool_alloc(part->log->mp, CORE_JSON_MAX_FIELD_LEN);

        /* Error. */
        if (rec == NULL) {
            *chunk = (const uint8_t *)"}";
            return strlen(*(const char **)chunk);
        }

        /* Next is used to determine if there is a trailing comma. */
        comma = ib_list_node_next((ib_list_node_t *)part->gen_data) ? "," : "";

        /// @todo Quote values
        switch(f->type) {
            case IB_FTYPE_NULSTR:
                rlen = snprintf((char *)rec, CORE_JSON_MAX_FIELD_LEN,
                                "  \"%" IB_BYTESTR_FMT "\": \"%s\"%s\r\n",
                                IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                                ib_field_value_nulstr(f),
                                comma);
                break;
            case IB_FTYPE_BYTESTR:
                rlen = snprintf((char *)rec, CORE_JSON_MAX_FIELD_LEN,
                                "  \"%" IB_BYTESTR_FMT "\": "
                                "\"%" IB_BYTESTR_FMT "\"%s\r\n",
                                IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                                IB_BYTESTR_FMT_PARAM(ib_field_value_bytestr(f)),
                                comma);
                break;
            case IB_FTYPE_NUM:
                rlen = snprintf((char *)rec, CORE_JSON_MAX_FIELD_LEN,
                                "  \"%" IB_BYTESTR_FMT "\": "
                                "%" PRIdMAX "%s\r\n",
                                IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                                *(intmax_t *)ib_field_value_num(f),
                                comma);
                break;
            case IB_FTYPE_UNUM:
                rlen = snprintf((char *)rec, CORE_JSON_MAX_FIELD_LEN,
                                "  \"%" IB_BYTESTR_FMT "\": "
                                "%" PRIuMAX "%s\r\n",
                                IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                                *(uintmax_t *)ib_field_value_unum(f),
                                comma);
                break;
            case IB_FTYPE_LIST:
                rlen = snprintf((char *)rec, CORE_JSON_MAX_FIELD_LEN,
                                "  \"%" IB_BYTESTR_FMT "\": [ \"TODO: Handle lists in json conversion\" ]%s\r\n",
                                IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                                comma);
                break;
            default:
                rlen = snprintf((char *)rec, CORE_JSON_MAX_FIELD_LEN,
                                "  \"%" IB_BYTESTR_FMT "\": \"-\"%s\r\n",
                                IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                                comma);
                break;
        }

        /* Verify size. */
        if (rlen >= CORE_JSON_MAX_FIELD_LEN) {
            ib_log_error(ib, 3, "Item too large to log in part %s: %" PRIuMAX,
                         part->name, rlen);
            *chunk = (const uint8_t *)"\r\n";
            part->gen_data = (void *)-1;
            return strlen(*(const char **)chunk);
        }

        *chunk = rec;
    }
    else {
        ib_log_error(ib, 4, "NULL field in part: %s", part->name);
        *chunk = (const uint8_t *)"\r\n";
        part->gen_data = (void *)-1;
        return strlen(*(const char **)chunk);
    }
    part->gen_data = ib_list_node_next((ib_list_node_t *)part->gen_data);

    /* Close the json structure. */
    if (part->gen_data == NULL) {
        size_t clen = strlen(*(const char **)chunk);
        (*(uint8_t **)chunk)[clen] = '}';
        part->gen_data = (void *)-1;
        return clen + 1;
    }

    return strlen(*(const char **)chunk);
}

static size_t ib_auditlog_gen_headers_flist(ib_auditlog_part_t *part,
                                            const uint8_t **chunk)
{
    ib_engine_t *ib = part->log->ib;
    ib_field_t *f;
    uint8_t *rec;
    size_t rlen;

#define CORE_HEADER_MAX_FIELD_LEN 8192
    
    /* The gen_data field is used to store the current state. NULL
     * means the part has not started yet and a -1 value
     * means it is done. Anything else is a node in the event list.
     */
    if (part->gen_data == NULL) {
        ib_list_t *list = (ib_list_t *)part->part_data;

        /* No data. */
        if (ib_list_elements(list) == 0) {
            ib_log_error(ib, 4, "No data in audit log part: %s", part->name);
            part->gen_data = NULL;
            return 0;
        }

        /* First should be a request/response line. */
        part->gen_data = ib_list_first(list);
        f = (ib_field_t *)ib_list_node_data((ib_list_node_t *)part->gen_data);
        if ((f != NULL) && (f->type == IB_FTYPE_BYTESTR)) {
            rec = (uint8_t *)ib_mpool_alloc(part->log->mp, CORE_HEADER_MAX_FIELD_LEN);
            rlen = snprintf((char *)rec, CORE_HEADER_MAX_FIELD_LEN,
                            "%" IB_BYTESTR_FMT "\r\n",
                            IB_BYTESTR_FMT_PARAM(ib_field_value_bytestr(f)));

            /* Verify size. */
            if (rlen >= CORE_HEADER_MAX_FIELD_LEN) {
                ib_log_error(ib, 3, "Item too large to log in part %s: %" PRIuMAX,
                             part->name, rlen);
                *chunk = (const uint8_t *)"\r\n";
                part->gen_data = (void *)-1;
                return strlen(*(const char **)chunk);
            }

            *chunk = rec;

            part->gen_data = ib_list_node_next((ib_list_node_t *)part->gen_data);
            if (part->gen_data == NULL) {
                part->gen_data = (void *)-1;
            }

            return strlen(*(const char **)chunk);
        }
    }
    else if (part->gen_data == (void *)-1) {
        part->gen_data = NULL;
        *chunk = (const uint8_t *)"";
        return 0;
    }

    /* Header Lines */
    f = (ib_field_t *)ib_list_node_data((ib_list_node_t *)part->gen_data);
    if (f == NULL) {
        ib_log_error(ib, 4, "NULL field in part: %s", part->name);
        *chunk = (const uint8_t *)"\r\n";
        part->gen_data = (void *)-1;
        return strlen(*(const char **)chunk);
    }

    rec = (uint8_t *)ib_mpool_alloc(part->log->mp, CORE_HEADER_MAX_FIELD_LEN);
    if (rec == NULL) {
        *chunk = NULL;
        return 0;
    }

    /// @todo Quote values
    switch(f->type) {
        case IB_FTYPE_NULSTR:
            rlen = snprintf((char *)rec, CORE_HEADER_MAX_FIELD_LEN,
                            "%" IB_BYTESTR_FMT ": %s\r\n",
                            IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                            ib_field_value_nulstr(f));
            break;
        case IB_FTYPE_BYTESTR:
            rlen = snprintf((char *)rec, CORE_HEADER_MAX_FIELD_LEN,
                            "%" IB_BYTESTR_FMT ": "
                            "%" IB_BYTESTR_FMT "\r\n",
                            IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                            IB_BYTESTR_FMT_PARAM(ib_field_value_bytestr(f)));
            break;
        default:
            rlen = snprintf((char *)rec, CORE_HEADER_MAX_FIELD_LEN,
                            "%" IB_BYTESTR_FMT ": IronBeeError - unhandled header type %d\r\n",
                            IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                            f->type);
            break;
    }

    /* Verify size. */
    if (rlen >= CORE_HEADER_MAX_FIELD_LEN) {
        ib_log_error(ib, 3, "Item too large to log in part %s: %" PRIuMAX,
                     part->name, rlen);
        *chunk = (const uint8_t *)"\r\n";
        part->gen_data = (void *)-1;
        return strlen(*(const char **)chunk);
    }

    *chunk = rec;

    /* Stage the next chunk of data (header). */
    part->gen_data = ib_list_node_next((ib_list_node_t *)part->gen_data);

    /* Close the structure if there is no more data. */
    if (part->gen_data == NULL) {
        part->gen_data = (void *)-1;
    }

    return strlen(*(const char **)chunk);
}

static size_t ib_auditlog_gen_json_events(ib_auditlog_part_t *part,
                                          const uint8_t **chunk)
{
    ib_engine_t *ib = part->log->ib;
    ib_logevent_t *e;
    uint8_t *rec;
    size_t rlen;

#define CORE_JSON_MAX_REC_LEN 1024
    
    /* The gen_data field is used to store the current state. NULL
     * means the part has not started yet and a -1 value
     * means it is done. Anything else is a node in the event list.
     */
    if (part->gen_data == NULL) {
        ib_list_t *list = (ib_list_t *)part->part_data;

        /* No events. */
        if (ib_list_elements(list) == 0) {
            ib_log_error(ib, 4, "No events in audit log");
            *chunk = (const uint8_t *)"{}";
            part->gen_data = (void *)-1;
            return strlen(*(const char **)chunk);
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
        rec = (uint8_t *)ib_mpool_alloc(part->log->mp, CORE_JSON_MAX_REC_LEN);

        /* Error. */
        if (rec == NULL) {
            *chunk = (const uint8_t *)"  ]\r\n}";
            return strlen(*(const char **)chunk);
        }

        rlen = snprintf((char *)rec, CORE_JSON_MAX_REC_LEN,
                        "    {\r\n"
                        "      \"event-id\": %" PRIu32 ",\r\n"
                        "      \"rule-id\": \"%s\",\r\n"
                        "      \"publisher\": \"%s\",\r\n"
                        "      \"source\": \"%s\",\r\n"
                        "      \"source-version\": \"%s\",\r\n"
                        "      \"type\": \"%s\",\r\n"
                        "      \"activity\": \"%s\",\r\n"
                        "      \"class\": \"%s/%s\",\r\n"
                        "      \"sys-env\": \"%s\",\r\n"
                        "      \"rec-action\": \"%s\",\r\n"
                        "      \"action\": \"%s\",\r\n"
                        "      \"confidence\": %u,\r\n"
                        "      \"severity\": %u,\r\n"
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
                        ib_logevent_type_name(e->type),
                        ib_logevent_activity_name(e->activity),
                        ib_logevent_pri_class_name(e->pri_class),
                        ib_logevent_sec_class_name(e->sec_class),
                        ib_logevent_sys_env_name(e->sys_env),
                        ib_logevent_action_name(e->rec_action),
                        ib_logevent_action_name(e->action),
                        e->confidence,
                        e->severity,
                        e->msg ? e->msg : "-");

        /* Verify size. */
        if (rlen >= CORE_JSON_MAX_REC_LEN) {
            ib_log_error(ib, 3, "Event too large to log: %" PRIuMAX, rlen);
            *chunk = (const uint8_t *)"    {}";
            part->gen_data = (void *)-1;
            return strlen(*(const char **)chunk);
        }

        *chunk = rec;
    }
    else {
        ib_log_error(ib, 4, "NULL event");
        *chunk = (const uint8_t *)"    {}";
        part->gen_data = (void *)-1;
        return strlen(*(const char **)chunk);
    }
    part->gen_data = ib_list_node_next((ib_list_node_t *)part->gen_data);

    /* Close the json structure. */
    if (part->gen_data == NULL) {
        size_t clen = strlen(*(const char **)chunk);

        part->gen_data = (void *)-1;

        if (clen+6 > CORE_JSON_MAX_REC_LEN) {
            if (clen+2 > CORE_JSON_MAX_REC_LEN) {
                ib_log_error(ib, 4, "Event too large to fit in buffer");
                *chunk = (const uint8_t *)"    {}\r\n  ]\r\n}";
            }
            memcpy(*(uint8_t **)chunk + clen, "]}", 2);
            return clen + 2;
        }
        memcpy(*(uint8_t **)chunk + clen, "  ]\r\n}", 6);
        return clen + 6;
    }

    return strlen(*(const char **)chunk);
}

/**
 * @internal
 * Generate a timestamp formatted for the audit log.
 *
 * Format: YYYY-MM-DDTHH:MM:SS.ssss+/-ZZZZ
 * Example: 2010-11-04T12:42:36.3874-0800
 *
 * @param buf Buffer at least 31 bytes in length
 * @param sec Epoch time in seconds
 * @param usec Optional microseconds
 */
static void ib_timestamp(char *buf, ib_timeval_t *tv)
{
    struct tm *tm = localtime((time_t *)&tv->tv_sec);
    
    strftime(buf, 30, "%Y-%m-%dT%H:%M:%S", tm);
    snprintf(buf + 19, 12, ".%04" PRIdMAX, tv->tv_usec);
    strftime(buf + 24, 6, "%z", tm);
}

#define CORE_AUDITLOG_FORMAT "http-message/1"

static ib_status_t ib_auditlog_add_part_header(ib_auditlog_t *log)
{
    IB_FTRACE_INIT(ib_auditlog_add_part_header);
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;
    ib_engine_t *ib = log->ib;
    ib_mpool_t *pool = log->mp;
    ib_field_t *f;
    ib_list_t *list;
    char *tstamp;
    char *log_format;
    ib_num_t sensorid = (ib_num_t)ib->sensor_id;
    ib_status_t rc;

    /* Timestamp */
    tstamp = (char *)ib_mpool_alloc(pool, 30);
    if (tstamp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    ib_timestamp(tstamp, &log->logtime);

    /* Log Format */
    log_format = (char *)ib_mpool_memdup(pool,
                                         CORE_AUDITLOG_FORMAT,
                                         strlen(CORE_AUDITLOG_FORMAT) + 1);
    if (log_format == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, pool);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_field_alias_mem(&f, pool,
                       "log-timestamp",
                       (uint8_t *)tstamp,
                       strlen(tstamp));
    ib_list_push(list, f);

    ib_field_alias_mem(&f, pool,
                       "log-format",
                       (uint8_t *)log_format,
                       strlen(log_format));
    ib_list_push(list, f);

    ib_field_alias_mem(&f, pool,
                       "log-id",
                       (uint8_t *)cfg->boundary,
                       strlen(cfg->boundary));
    ib_list_push(list, f);

    ib_field_create(&f, pool,
                    "sensor-id",
                    IB_FTYPE_UNUM,
                    &sensorid);
    ib_list_push(list, f);

    ib_field_alias_mem(&f, pool,
                       "sensor-name",
                       (uint8_t *)ib->sensor_name,
                       strlen(ib->sensor_name));
    ib_list_push(list, f);

    ib_field_alias_mem(&f, pool,
                       "sensor-version",
                       (uint8_t *)ib->sensor_version,
                       strlen(ib->sensor_version));
    ib_list_push(list, f);

    ib_field_alias_mem(&f, pool,
                       "sensor-hostname",
                       (uint8_t *)ib->sensor_hostname,
                       strlen(ib->sensor_hostname));
    ib_list_push(list, f);

    /* Add the part to the auditlog. */
    rc = ib_auditlog_part_add(log,
                              "header",
                              "application/json",
                              list,
                              ib_auditlog_gen_json_flist,
                              NULL);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_auditlog_add_part_events(ib_auditlog_t *log)
{
    IB_FTRACE_INIT(ib_auditlog_add_part_events);
    ib_list_t *list;
    ib_status_t rc;

    /* Get the list of events. */
    rc = ib_clog_events_get(log->ctx, &list);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Add the part to the auditlog. */
    rc = ib_auditlog_part_add(log,
                              "events",
                              "application/json",
                              list,
                              ib_auditlog_gen_json_events,
                              NULL);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_auditlog_add_part_http_request_meta(ib_auditlog_t *log)
{
    IB_FTRACE_INIT(ib_auditlog_add_part_http_request_meta);
    ib_engine_t *ib = log->ib;
    ib_tx_t *tx = log->tx;
    ib_unum_t message_num = tx ? tx->conn->tx_count : 0;
    ib_mpool_t *pool = log->mp;
    ib_field_t *f;
    ib_list_t *list;
    char *tstamp;
    ib_status_t rc;

    /* Timestamp */
    tstamp = (char *)ib_mpool_alloc(pool, 30);
    if (tstamp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    ib_timestamp(tstamp, &tx->started);

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, pool);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_field_alias_mem(&f, pool,
                       "request-timestamp",
                       (uint8_t *)tstamp,
                       strlen(tstamp));
    ib_list_push(list, f);

    ib_field_alias_mem(&f, pool,
                       "message-id",
                       (uint8_t *)tx->id,
                       strlen(tx->id));
    ib_list_push(list, f);

    ib_field_create(&f, pool,
                    "message-num",
                    IB_FTYPE_UNUM,
                    &message_num);
    ib_list_push(list, f);

    ib_field_alias_mem(&f, pool,
                       "remote-addr",
                       (uint8_t *)tx->conn->remote_ipstr,
                       strlen(tx->conn->remote_ipstr));
    ib_list_push(list, f);

    ib_field_create(&f, pool,
                    "remote-port",
                    IB_FTYPE_UNUM,
                    &tx->conn->remote_port);
    ib_list_push(list, f);

    ib_field_alias_mem(&f, pool,
                       "local-addr",
                       (uint8_t *)tx->conn->local_ipstr,
                       strlen(tx->conn->local_ipstr));
    ib_list_push(list, f);

    ib_field_create(&f, pool,
                    "local-port",
                    IB_FTYPE_UNUM,
                    &tx->conn->local_port);
    ib_list_push(list, f);

    /// @todo If this is NULL, parser failed - what to do???
    if (tx->path != NULL) {
        ib_field_alias_mem(&f, pool,
                           "request-uri-path",
                           (uint8_t *)tx->path,
                           strlen(tx->path));
        ib_list_push(list, f);
    }

    rc = ib_data_get_ex(tx->dpi, IB_S2SL("request_protocol"), &f);
    if (rc == IB_OK) {
        ib_list_push(list, f);
    }
    else {
        ib_log_error(ib, 4, "Failed to get request_protocol: %d", rc);
    }

    rc = ib_data_get_ex(tx->dpi, IB_S2SL("request_method"), &f);
    if (rc == IB_OK) {
        ib_list_push(list, f);
    }
    else {
        ib_log_error(ib, 4, "Failed to get request_method: %d", rc);
    }

    /// @todo If this is NULL, parser failed - what to do???
    if (tx->hostname != NULL) {
        ib_field_alias_mem(&f, pool,
                           "request-hostname",
                           (uint8_t *)tx->hostname,
                           strlen(tx->hostname));
        ib_list_push(list, f);
    }

    /* Add the part to the auditlog. */
    rc = ib_auditlog_part_add(log,
                              "http-request-metadata",
                              "application/json",
                              list,
                              ib_auditlog_gen_json_flist,
                              NULL);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_auditlog_add_part_http_response_meta(ib_auditlog_t *log)
{
    IB_FTRACE_INIT(ib_auditlog_add_part_http_response_meta);
    ib_engine_t *ib = log->ib;
    ib_tx_t *tx = log->tx;
    ib_mpool_t *pool = log->mp;
    ib_field_t *f;
    ib_list_t *list;
    char *tstamp;
    ib_status_t rc;

    /* Timestamp */
    tstamp = (char *)ib_mpool_alloc(pool, 30);
    if (tstamp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    ib_timestamp(tstamp, &tx->tv_response);

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, pool);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_field_alias_mem(&f, pool,
                       "response-timestamp",
                       (uint8_t *)tstamp,
                       strlen(tstamp));
    ib_list_push(list, f);

    rc = ib_data_get_ex(tx->dpi, IB_S2SL("response_status"), &f);
    if (rc == IB_OK) {
        ib_list_push(list, f);
    }
    else {
        ib_log_error(ib, 4, "Failed to get response_status: %d", rc);
    }

    rc = ib_data_get_ex(tx->dpi, IB_S2SL("response_protocol"), &f);
    if (rc == IB_OK) {
        ib_list_push(list, f);
    }
    else {
        ib_log_error(ib, 4, "Failed to get response_protcol: %d", rc);
    }

    /* Add the part to the auditlog. */
    rc = ib_auditlog_part_add(log,
                              "http-response-metadata",
                              "application/json",
                              list,
                              ib_auditlog_gen_json_flist,
                              NULL);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_auditlog_add_part_http_request_head(ib_auditlog_t *log)
{
    IB_FTRACE_INIT(ib_auditlog_add_part_http_request_head);
    ib_engine_t *ib = log->ib;
    ib_mpool_t *pool = log->mp;
    ib_tx_t *tx = log->tx;
    ib_list_t *list;
    ib_list_node_t *node;
    ib_field_t *f;
    ib_status_t rc;

    /// @todo Use raw buffered data when available.

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, pool);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_get_ex(tx->dpi, IB_S2SL("request_line"), &f);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to get request_line: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_list_push(list, f);

    rc = ib_data_get_ex(tx->dpi, IB_S2SL("request_headers"), &f);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to get request_headers: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_LIST_LOOP(ib_field_value_list(f), node) {
        ib_list_push(list, ib_list_node_data(node));
    }

    /* Add the part to the auditlog. */
    rc = ib_auditlog_part_add(log,
                              "http-request-headers",
                              "application/octet-stream",
                              list,
                              ib_auditlog_gen_headers_flist,
                              NULL);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_auditlog_add_part_http_response_head(ib_auditlog_t *log)
{
    IB_FTRACE_INIT(ib_auditlog_add_part_http_response_head);
    ib_engine_t *ib = log->ib;
    ib_mpool_t *pool = log->mp;
    ib_tx_t *tx = log->tx;
    ib_list_t *list;
    ib_list_node_t *node;
    ib_field_t *f;
    ib_status_t rc;

    /// @todo Use raw buffered data when available.

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, pool);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_data_get_ex(tx->dpi, IB_S2SL("response_line"), &f);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to get response_line: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_list_push(list, f);

    rc = ib_data_get_ex(tx->dpi, IB_S2SL("response_headers"), &f);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to get response_headers: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_LIST_LOOP(ib_field_value_list(f), node) {
        ib_list_push(list, ib_list_node_data(node));
    }

    /* Add the part to the auditlog. */
    rc = ib_auditlog_part_add(log,
                              "http-response-headers",
                              "application/octet-stream",
                              list,
                              ib_auditlog_gen_headers_flist,
                              NULL);

    IB_FTRACE_RET_STATUS(rc);
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
    ib_auditlog_t *log;
    ib_core_cfg_t *corecfg;
    core_audit_cfg_t *cfg;
    ib_provider_inst_t *audit;
    ib_list_t *events;
    struct timeval tv;
    uint32_t boundary_rand = rand(); /// @todo better random num
    char boundary[46];
    ib_status_t rc;

    rc = ib_context_module_config(tx->ctx, ib_core_module(),
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    switch (corecfg->audit_engine) {
        /* Always On */
        case 1:
            break;
        /* Only if events are present */
        case 2:
            rc = ib_clog_events_get(tx->ctx, &events);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
            if (ib_list_elements(events) == 0) {
                IB_FTRACE_RET_STATUS(IB_OK);
            }
            break;
        /* Anything else is Off */
        default:
            IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Mark the audit log time. */
    gettimeofday(&tv, NULL);

    /* Auditing */
    /// @todo Only create if needed
    log = (ib_auditlog_t *)ib_mpool_calloc(tx->mp, 1, sizeof(*log));
    if (log == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    log->logtime.tv_sec = tv.tv_sec;
    log->logtime.tv_usec = tv.tv_usec;
    log->ib = ib;
    log->mp = tx->mp;
    log->ctx = tx->ctx;
    log->tx = tx;

    rc = ib_list_create(&log->parts, log->mp);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Create a unique MIME boundary. */
    snprintf(boundary, sizeof(boundary), "%08x-%s",
             boundary_rand, log->tx->id ? log->tx->id : "FixMe-No-Tx-on-Audit");

    /* Create the core config. */
    cfg = (core_audit_cfg_t *)ib_mpool_calloc(log->mp, 1, sizeof(*cfg));
    if (cfg == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    cfg->logtime = &log->logtime;
    cfg->tx = tx;
    cfg->boundary = boundary;
    log->cfg_data = cfg;


    /* Add all the parts to the log. */
    if (corecfg->auditlog_parts & IB_ALPART_HEADER) {
        ib_auditlog_add_part_header(log);
    }
    if (corecfg->auditlog_parts & IB_ALPART_EVENTS) {
        ib_auditlog_add_part_events(log);
    }
    if (corecfg->auditlog_parts & IB_ALPART_HTTP_REQUEST_METADATA) {
        ib_auditlog_add_part_http_request_meta(log);
    }
    if (corecfg->auditlog_parts & IB_ALPART_HTTP_RESPONSE_METADATA) {
        ib_auditlog_add_part_http_response_meta(log);
    }
    if (corecfg->auditlog_parts & IB_ALPART_HTTP_REQUEST_HEADERS) {
        ib_auditlog_add_part_http_request_head(log);
    }
    if (corecfg->auditlog_parts & IB_ALPART_HTTP_RESPONSE_HEADERS) {
        ib_auditlog_add_part_http_response_head(log);
    }

    /* Audit Provider */
    rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_AUDIT,
                                     corecfg->audit, &audit,
                                     ib->mp, log);
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
 * @param epi Logevent provider instance
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
        ib_log_error(ib, 0, "Failed to fetch parser interface on init");
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

    /* Create connection fields. */
    rc = ib_data_add_bytestr(conn->dpi,
                             "server_addr",
                             (uint8_t *)conn->local_ipstr,
                             strlen(conn->local_ipstr),
                             NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    rc = ib_data_add_num(conn->dpi,
                         "server_port",
                         conn->local_port,
                         NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    rc = ib_data_add_bytestr(conn->dpi,
                             "remote_addr",
                             (uint8_t *)conn->remote_ipstr,
                             strlen(conn->remote_ipstr),
                             NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    rc = ib_data_add_num(conn->dpi,
                         "remote_port",
                         conn->remote_port,
                         NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (iface == NULL) {
        ib_log_error(ib, 0, "Failed to fetch parser interface on connect");
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
        ib_log_error(ib, 0, "Failed to fetch parser interface on disconnect");
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
    ib_field_t *f;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(ib, 0, "Failed to fetch parser interface on request header");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->gen_request_header_fields(pi, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }


    /* Alias ARGS fields */
    rc = ib_data_get(tx->dpi, "request_uri_params", &f);
    if (rc == IB_OK) {
        rc = ib_data_add_named(tx->dpi, f, "args", 4);
        if (rc != IB_OK) {
            ib_log_debug(ib, 4, "Failed to alias ARGS: %d", rc);
        }
        rc = ib_data_add_named(tx->dpi, f, "args_get", 8);
        if (rc != IB_OK) {
            ib_log_debug(ib, 4, "Failed to alias ARGS_GET: %d", rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
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
        ib_log_error(ib, 0, "Failed to fetch parser interface response header");
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
                                ib_field_t *f,
                                const char *name,
                                size_t nlen)
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

    rc = iface->add(dpi, f, name, nlen);
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
                                ib_field_t *f,
                                const char *name,
                                size_t nlen)
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

    rc = iface->set(dpi, f, name, nlen);
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
 * Calls a registered provider interface to get all data fields within a
 * provider instance.
 *
 * @param dpi Data provider instance
 * @param list List in which fields are pushed
 * 
 * @returns Status code
 */
static ib_status_t data_api_get_all(ib_provider_inst_t *dpi,
                                    ib_list_t *list)
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

    rc = iface->get_all(dpi, list);
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
    data_api_get_all,
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
 * Initialize the data access provider instance.
 *
 * @param dpi Data provider instance
 * @param data Initialization data
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
 * @param erroffset Offset in pattern where the error occurred (if non-NULL)
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


/* -- Filters -- */

/**
 * @internal
 * Core buffer filter.
 *
 * This is a simplistic buffer filter that holds request data while
 * it can be inspected.
 *
 * @todo This needs lots of work on configuration, etc.
 *
 * @param f Filter
 * @param fdata Filter data
 * @param ctx Config context
 * @param pool Memory pool
 * @param pflags Address which flags are written
 *
 * @returns Status code
 */
static ib_status_t filter_buffer(ib_filter_t *f,
                                 ib_fdata_t *fdata,
                                 ib_context_t *ctx,
                                 ib_mpool_t *pool,
                                 ib_flags_t *pflags)
{
    IB_FTRACE_INIT(filter_buffer);
//    ib_engine_t *ib = f->ib;
    ib_stream_t *buf = (ib_stream_t *)fdata->state;
    ib_sdata_t *sdata;
    ib_status_t rc;

    if (buf == NULL) {
        fdata->state = ib_mpool_calloc(pool, 1, sizeof(*buf));
        if (buf == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
        buf = (ib_stream_t *)fdata->state;
    }

    /* Move data to buffer until we get an EOS, then move
     * the data back into the stream. */
    /// @todo Need API to move data between streams.
    rc = ib_stream_pull(fdata->stream, &sdata);
    while (rc == IB_OK) {
        rc = ib_stream_push_sdata(buf, sdata);
        if (rc == IB_OK) {
            if (sdata->type == IB_STREAM_EOS) {
                rc = ib_stream_pull(buf, &sdata);
                while (rc == IB_OK) {
                    rc = ib_stream_push_sdata(fdata->stream, sdata);
                    if (rc == IB_OK) {
                        rc = ib_stream_pull(buf, &sdata);
                    }
                }
                if (rc != IB_ENOENT) {
                    IB_FTRACE_RET_STATUS(rc);
                }
                break;
            }
            rc = ib_stream_pull(fdata->stream, &sdata);
        }
    }
    if (rc != IB_ENOENT) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Configure the filter controller.
 *
 * @param ib Engine
 * @param tx Transaction
 * @param cbdata Callback data
 *
 * @returns Status code
 */
static ib_status_t filter_ctl_config(ib_engine_t *ib,
                                     ib_tx_t *tx,
                                     void *cbdata)
{
    IB_FTRACE_INIT(filter_ctl_config);
    ib_status_t rc = IB_OK;

    /// @todo Need an API for this.
    tx->fctl->filters = tx->ctx->filters;
    tx->fctl->fbuffer = (ib_filter_t *)cbdata;
    ib_fctl_meta(tx->fctl, IB_STREAM_FLUSH);

    IB_FTRACE_RET_STATUS(rc);
}


/* -- Transformations -- */

/**
 * @internal
 * Simple ASCII lowercase function.
 *
 * @note For non-ASCII (utf8, etc) you should use case folding.
 */
static ib_status_t core_tfn_lowercase(void *fndata,
                                      ib_mpool_t *pool,
                                      uint8_t *data_in,
                                      size_t dlen_in,
                                      uint8_t **data_out,
                                      size_t *dlen_out,
                                      ib_flags_t *pflags)
{
    size_t i = 0;
    int modified = 0;

    /* This is an in-place transformation which does not change
     * the data length.
     */
    *data_out = data_in;
    *dlen_out = dlen_in;
    (*pflags) |= IB_TFN_FINPLACE;

    while(i < dlen_in) {
        int c = data_in[i];
        (*data_out)[i] = tolower(c);
        if (c != (*data_out)[i]) {
            modified++;
        }
        i++;
    }

    if (modified != 0) {
        (*pflags) |= IB_TFN_FMODIFIED;
        (*data_out)[*dlen_out] = '\0';
    }

    return IB_OK;
}

/**
 * @internal
 * Simple ASCII trimLeft function.
 */
static ib_status_t core_tfn_trimleft(void *fndata,
                                     ib_mpool_t *pool,
                                     uint8_t *data_in,
                                     size_t dlen_in,
                                     uint8_t **data_out,
                                     size_t *dlen_out,
                                     ib_flags_t *pflags)
{
    size_t i = 0;

    /* This is an in-place transformation which may change
     * the data length.
     */
    (*pflags) |= IB_TFN_FINPLACE;

    while(i < dlen_in) {
        if (isspace(data_in[i]) == 0) {
            *data_out = data_in + i;
            *dlen_out = dlen_in - i;
            (*pflags) |= IB_TFN_FMODIFIED;
            return IB_OK;
        }
        i++;
    }
    *dlen_out = 0;
    *data_out = data_in;

    return IB_OK;
}

/**
 * @internal
 * Simple ASCII trimRight function.
 */
static ib_status_t core_tfn_trimright(void *fndata,
                                      ib_mpool_t *pool,
                                      uint8_t *data_in,
                                      size_t dlen_in,
                                      uint8_t **data_out,
                                      size_t *dlen_out,
                                      ib_flags_t *pflags)
{
    size_t i = dlen_in - 1;

    /* This is an in-place transformation which may change
     * the data length.
     */
    *data_out = data_in;
    (*pflags) |= IB_TFN_FINPLACE;

    while(i > 0) {
        if (isspace(data_in[i]) == 0) {
            (*pflags) |= IB_TFN_FMODIFIED;
            (*data_out)[*dlen_out] = '\0';
            *dlen_out = i + 1;
            return IB_OK;
        }
        i--;
    }
    *dlen_out = 0;

    return IB_OK;
}

/**
 * @internal
 * Simple ASCII trim function.
 */
static ib_status_t core_tfn_trim(void *fndata,
                                 ib_mpool_t *pool,
                                 uint8_t *data_in,
                                 size_t dlen_in,
                                 uint8_t **data_out,
                                 size_t *dlen_out,
                                 ib_flags_t *pflags)
{
    ib_status_t rc;

    /* Just call the other trim functions. */
    rc = core_tfn_trimleft(fndata, pool, data_in, dlen_in, data_out, dlen_out,
                           pflags);
    if (IB_OK != IB_OK) {
        return rc;
    }
    rc = core_tfn_trimleft(fndata, pool, *data_out, *dlen_out, data_out, dlen_out,
                           pflags);
    return rc;
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

    ib_log_debug(ib, 6, "Creating site \"%s\"", p1);
    rc = ib_site_create(&site, ib, p1);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to create site \"%s\": %d", rc);
    }

    ib_log_debug(ib, 6, "Creating default location for site \"%s\"", p1);
    rc = ib_site_loc_create_default(site, &loc);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to create default location for site \"%s\": %d", p1, rc);
    }

    ib_log_debug(ib, 6, "Creating context for \"%s:%s\"", p1, loc->path);
    rc = ib_context_create(&ctx, ib, cp->cur_ctx,
                           ib_context_siteloc_chooser, loc);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to create context for \"%s:%s\": %d", p1, loc->path, rc);
    }
    ib_cfgparser_context_push(cp, ctx);

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

    ib_log_debug(ib, 8, "Processing site block \"%s\"", name);

    /* Pop the current items off the stack */
    rc = ib_cfgparser_context_pop(cp, &ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to pop context for \"%s\": %d", name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug(ib, 8, "Initializing context %p for \"%s\"", ctx, name);
    rc = ib_context_init(ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Error initializing context for \"%s\": %d",
                     name, rc);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Handle the start of a Location block.
 *
 * This function sets up the new location and pushes it onto the parser stack.
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

    ib_log_debug(ib, 6, "Creating location \"%s\" for site \"%s\"", p1, site->name);
    rc = ib_site_loc_create(site, &loc, p1);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to create location \"%s:%s\": %d", site->name, p1, rc);
    }

    ib_log_debug(ib, 6, "Creating context for \"%s:%s\"", site->name, loc->path);
    rc = ib_context_create(&ctx, ib, cp->cur_ctx,
                           ib_context_siteloc_chooser, loc);
    if (rc != IB_OK) {
        ib_log_debug(ib, 6, "Failed to create context for \"%s:%s\": %d", site->name, loc->path, rc);
    }
    ib_cfgparser_context_push(cp, ctx);

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

    ib_log_debug(ib, 8, "Processing location block \"%s\"", name);

    /* Pop the current items off the stack */
    rc = ib_cfgparser_context_pop(cp, &ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, 4, "Failed to pop context for \"%s\": %d", name, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug(ib, 8, "Initializing context %p for \"%s\"", ctx, name);
    rc = ib_context_init(ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, 1, "Error initializing context for \"%s\": %d",
                     name, rc);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

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
            ib_log_debug(ib, 7, "Adding IP \"%s\" to site \"%s\"",
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
            ib_log_debug(ib, 7, "Adding host \"%s\" to site \"%s\"",
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
        ib_log_debug(ib, 7, "%s: \"%s\" ctx=%p", name, p1, ctx);
        if (strcasecmp("RelevantOnly", p1) == 0) {
            rc = ib_context_set_num(ctx, "audit_engine", 2);
            IB_FTRACE_RET_STATUS(rc);
        }
        else if (strcasecmp("On", p1) == 0) {
            rc = ib_context_set_num(ctx, "audit_engine", 1);
            IB_FTRACE_RET_STATUS(rc);
        }
        else if (strcasecmp("Off", p1) == 0) {
            rc = ib_context_set_num(ctx, "audit_engine", 0);
            IB_FTRACE_RET_STATUS(rc);
        }

        ib_log_error(ib, 1, "Failed to parse directive: %s \"%s\"", name, p1);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    else if (strcasecmp("AuditLogIndex", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        ib_log_debug(ib, 7, "%s: \"%s\" ctx=%p", name, p1, ctx);
        rc = ib_context_set_string(ctx, "auditlog_index", p1);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("AuditLogDirMode", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        long lmode = strtol(p1, NULL, 0);

        if ((lmode > 0777) || (lmode <= 0)) {
            ib_log_error(ib, 1, "Invalid mode: %s \"%s\"", name, p1);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        ib_log_debug(ib, 7, "%s: \"%s\" ctx=%p", name, p1, ctx);
        rc = ib_context_set_num(ctx, "auditlog_dmode", lmode);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("AuditLogFileMode", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        long lmode = strtol(p1, NULL, 0);

        if ((lmode > 0777) || (lmode <= 0)) {
            ib_log_error(ib, 1, "Invalid mode: %s \"%s\"", name, p1);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        ib_log_debug(ib, 7, "%s: \"%s\" ctx=%p", name, p1, ctx);
        rc = ib_context_set_num(ctx, "auditlog_fmode", lmode);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("AuditLogBaseDir", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        ib_log_debug(ib, 7, "%s: \"%s\" ctx=%p", name, p1, ctx);
        rc = ib_context_set_string(ctx, "auditlog_dir", p1);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("AuditLogSubDirFormat", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        ib_log_debug(ib, 7, "%s: \"%s\" ctx=%p", name, p1, ctx);
        rc = ib_context_set_string(ctx, "auditlog_sdir_fmt", p1);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("DebugLogLevel", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        ib_log_debug(ib, 7, "%s: %d", name, atol(p1));
        rc = ib_context_set_num(ctx, "logger.log_level", atol(p1));
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("LoadModule", name) == 0) {
        char *absfile;
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
        }
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("RequestBuffering", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);

        ib_log_debug(ib, 7, "%s: %s", name, p1);
        if (strcasecmp("On", p1) == 0) {
            rc = ib_context_set_num(ctx, "buffer_req", 1);
            IB_FTRACE_RET_STATUS(rc);
        }

        rc = ib_context_set_num(ctx, "buffer_req", 0);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("ResponseBuffering", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);

        ib_log_debug(ib, 7, "%s: %s", name, p1);
        if (strcasecmp("On", p1) == 0) {
            rc = ib_context_set_num(ctx, "buffer_res", 1);
            IB_FTRACE_RET_STATUS(rc);
        }

        rc = ib_context_set_num(ctx, "buffer_res", 0);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("SensorId", name) == 0) {
        ib->sensor_id = htonl(strtol(p1, NULL, 0));
        ib_log_debug(ib, 7, "%s: %08x", name, ib->sensor_id);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (strcasecmp("SensorName", name) == 0) {
        ib->sensor_name =
            (const char *)ib_mpool_memdup(ib_engine_pool_config_get(ib),
                                          p1, strlen(p1));
        ib_log_debug(ib, 7, "%s: %s", name, ib->sensor_name);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (strcasecmp("SensorHostname", name) == 0) {
        ib->sensor_hostname =
            (const char *)ib_mpool_memdup(ib_engine_pool_config_get(ib),
                                          p1, strlen(p1));
        ib_log_debug(ib, 7, "%s: %s", name, ib->sensor_hostname);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_error(ib, 1, "Unhandled directive: %s %s", name, p1);
    IB_FTRACE_RET_STATUS(IB_EINVAL);
}

/**
 * @internal
 * Handle single parameter directives.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param flags Flags
 * @param fmask Flags mask (which bits were actually set)
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_auditlogparts(ib_cfgparser_t *cp,
                                          const char *name,
                                          ib_flags_t flags,
                                          ib_flags_t fmask,
                                          void *cbdata)
{
    IB_FTRACE_INIT(core_dir_auditlogparts);
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
    ib_num_t parts;
    ib_status_t rc;

    rc = ib_context_get(ctx, "auditlog_parts", &parts, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Merge the set flags with the previous value. */
    parts = (flags & fmask) | (parts & ~fmask);

    ib_log_debug(ib, 4, "AUDITLOG PARTS: 0x%08x", (unsigned long)parts);

    rc = ib_context_set_num(ctx, "auditlog_parts", parts);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Perform any extra duties when certain config parameters are "Set".
 *
 * @param ctx Context
 * @param type Config parameter type
 * @param name Config parameter name
 * @param val Config parameter value
 *
 * @returns Status code
 */
static void core_set_value(ib_context_t *ctx,
                           ib_ftype_t type,
                           const char *name,
                           const char *val)
{
    ib_engine_t *ib = ctx->ib;
    ib_core_cfg_t *corecfg;
    ib_provider_inst_t *pi;
    ib_status_t rc;

    /* Get the core module config. */
    rc = ib_context_module_config(ib->ctx, ib_core_module(),
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        corecfg = &core_global_cfg;
    }

    if (strcasecmp("logger", name) == 0) {
        /* Lookup/set logger provider. */
        rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_LOGGER,
                                         val, &pi,
                                         ib->mp, NULL);
        if (rc != IB_OK) {
            ib_log_error(ib, 0, "Failed to create %s provider instance: %d",
                         IB_PROVIDER_TYPE_LOGGER, rc);
            return;
        }
        ib_log_provider_set_instance(ctx, pi);
    }
    else if (strcasecmp("parser", name) == 0) {
        if (strcmp(MODULE_NAME_STR, corecfg->parser) == 0) {
            return;
        }
        /* Lookup/set parser provider. */
        rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_PARSER,
                                         val, &pi,
                                         ib->mp, NULL);
        if (rc != IB_OK) {
            ib_log_error(ib, 0, "Failed to create %s provider instance: %d",
                         IB_PROVIDER_TYPE_PARSER, rc);
            return;
        }
        ib_parser_provider_set_instance(ctx, pi);
        pi = ib_parser_provider_get_instance(ctx);
    }
    else if (strcasecmp("audit", name) == 0) {
        /* Lookup/set audit provider. */
        rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_AUDIT,
                                         val, &pi,
                                         ib->mp, NULL);
        if (rc != IB_OK) {
            ib_log_error(ib, 0, "Failed to create %s provider instance: %d",
                         IB_PROVIDER_TYPE_AUDIT, rc);
            return;
        }
        ib_audit_provider_set_instance(ctx, pi);
    }
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

        core_set_value(ctx, type, p1, p2);
    }
    else {
        ib_log_error(ib, 1, "Unhandled directive: %s %s %s", name, p1, p2);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
 * @internal
 * Mapping of valid audit log part names to flag values.
 */
static IB_STRVAL_MAP(core_parts_map) = {
    /* Auditlog Part Groups */
    IB_STRVAL_PAIR("none", 0),
    IB_STRVAL_PAIR("minimal", IB_ALPART_HEADER|IB_ALPART_EVENTS),
    IB_STRVAL_PAIR("all", IB_ALPARTS_ALL),
    IB_STRVAL_PAIR("debug", IB_ALPART_DEBUG_FIELDS),
    IB_STRVAL_PAIR("default", IB_ALPARTS_DEFAULT),
    IB_STRVAL_PAIR("request", IB_ALPARTS_REQUEST),
    IB_STRVAL_PAIR("response", IB_ALPARTS_RESPONSE),

    /* AuditLog Individual Parts */
    IB_STRVAL_PAIR("header", IB_ALPART_HEADER),
    IB_STRVAL_PAIR("events", IB_ALPART_EVENTS),
    IB_STRVAL_PAIR("requestmetadata", IB_ALPART_HTTP_REQUEST_METADATA),
    IB_STRVAL_PAIR("requestheaders", IB_ALPART_HTTP_REQUEST_HEADERS),
    IB_STRVAL_PAIR("requestbody", IB_ALPART_HTTP_REQUEST_BODY),
    IB_STRVAL_PAIR("requesttrailers", IB_ALPART_HTTP_REQUEST_TRAILERS),
    IB_STRVAL_PAIR("responsemetadata", IB_ALPART_HTTP_RESPONSE_METADATA),
    IB_STRVAL_PAIR("responseheaders", IB_ALPART_HTTP_RESPONSE_HEADERS),
    IB_STRVAL_PAIR("responsebody", IB_ALPART_HTTP_RESPONSE_BODY),
    IB_STRVAL_PAIR("responsetrailers", IB_ALPART_HTTP_RESPONSE_TRAILERS),
    IB_STRVAL_PAIR("debugfields", IB_ALPART_DEBUG_FIELDS),

    /* End */
    IB_STRVAL_PAIR_LAST
};

/**
 * @internal
 * Directive initialization structure.
 */
static IB_DIRMAP_INIT_STRUCTURE(core_directive_map) = {
    /* Modules */
    IB_DIRMAP_INIT_PARAM1(
        "LoadModule",
        core_dir_param1,
        NULL
    ),

    /* Parameters */
    IB_DIRMAP_INIT_PARAM2(
        "Set",
        core_dir_param2,
        NULL
    ),

    /* Sensor */
    IB_DIRMAP_INIT_PARAM1(
        "SensorId",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "SensorName",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "SensorHostname",
        core_dir_param1,
        NULL
    ),

    /* Buffering */
    IB_DIRMAP_INIT_PARAM1(
        "RequestBuffering",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "ResponseBuffering",
        core_dir_param1,
        NULL
    ),

    /* Logging */
    IB_DIRMAP_INIT_PARAM1(
        "DebugLogLevel",
        core_dir_param1,
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
        NULL
    ),

    /* Inspection Engine */
    IB_DIRMAP_INIT_PARAM1(
        "InspectionEngine",
        core_dir_param1,
        NULL
    ),

    /* Audit Engine */
    IB_DIRMAP_INIT_PARAM1(
        "AuditEngine",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "AuditLogIndex",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "AuditLogBaseDir",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "AuditLogSubDirFormat",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "AuditLogDirMode",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "AuditLogFileMode",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_OPFLAGS(
        "AuditLogParts",
        core_dir_auditlogparts,
        NULL,
        core_parts_map
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
static ib_status_t core_init(ib_engine_t *ib,
                             ib_module_t *m)
{
    IB_FTRACE_INIT(core_init);
    ib_core_cfg_t *corecfg;
    ib_provider_t *core_log_provider;
    ib_provider_t *core_audit_provider;
    ib_provider_t *core_data_provider;
    ib_provider_inst_t *logger;
    ib_provider_inst_t *logevent;
    ib_provider_inst_t *parser;
    ib_filter_t *fbuffer;
    ib_status_t rc;

    /* Get the core module config. */
    rc = ib_context_module_config(ib->ctx, m,
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to fetch core module config: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Define transformations. */
    ib_tfn_create(ib, "lowercase", core_tfn_lowercase, NULL, NULL);
    ib_tfn_create(ib, "trimLeft", core_tfn_trimleft, NULL, NULL);
    ib_tfn_create(ib, "trimRight", core_tfn_trimright, NULL, NULL);
    ib_tfn_create(ib, "trim", core_tfn_trim, NULL, NULL);

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

    /* Filter/Buffer */
    rc = ib_filter_register(&fbuffer,
                            ib,
                            "core-buffer",
                            IB_FILTER_TX,
                            IB_FILTER_OBUF,
                            filter_buffer,
                            NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to register buffer filter: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_hook_register(ib, handle_context_tx_event,
                     (ib_void_fn_t)filter_ctl_config, fbuffer);


    /* Register parser hooks. */
    ib_hook_register(ib, conn_started_event,
                     (ib_void_fn_t)parser_hook_init, NULL);
    ib_hook_register(ib, handle_connect_event,
                     (ib_void_fn_t)parser_hook_connect, NULL);
    ib_hook_register(ib, handle_disconnect_event,
                     (ib_void_fn_t)parser_hook_disconnect, NULL);
    /// @todo Need the parser to parse headers before context, but others after context so that the personality can change based on headers (Host, uri path, etc)
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

    /* Lookup/set default logger provider. */
    rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_LOGGER,
                                     corecfg->logger, &logger,
                                     ib->mp, NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to create %s provider instance: %d", IB_PROVIDER_TYPE_LOGGER, rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_log_provider_set_instance(ib->ctx, logger);

    /* Lookup/set default logevent provider. */
    rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_LOGEVENT,
                                     corecfg->logevent, &logevent,
                                     ib->mp, NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to create %s provider instance: %d", IB_PROVIDER_TYPE_LOGEVENT, rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_logevent_provider_set_instance(ib->ctx, logevent);

    /* Lookup/set default parser provider if not the "core" parser. */
    if (strcmp(MODULE_NAME_STR, corecfg->parser) != 0) {
        rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_PARSER,
                                         corecfg->parser, &parser,
                                         ib->mp, NULL);
        if (rc != IB_OK) {
            ib_log_error(ib, 0, "Failed to create %s provider instance: %d", IB_PROVIDER_TYPE_PARSER, rc);
            IB_FTRACE_RET_STATUS(rc);
        }
        ib_parser_provider_set_instance(ib->ctx, parser);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Finalize the core module on unload.
 *
 * @param ib Engine
 *
 * @returns Status code
 */
static ib_status_t core_fini(ib_engine_t *ib,
                             ib_module_t *m)
{
    IB_FTRACE_INIT(core_fini);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Initialize the core module when it registered with a context.
 *
 * @param ib Engine
 * @param m Module
 * @param ctx Context
 *
 * @returns Status code
 */
static ib_status_t core_context_init(ib_engine_t *ib,
                                     ib_module_t *m,
                                     ib_context_t *ctx)
{
    IB_FTRACE_INIT(core_context_init);
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

    /* Buffering */
    IB_CFGMAP_INIT_ENTRY(
        "buffer_req",
        IB_FTYPE_NUM,
        &core_global_cfg,
        buffer_req,
        0
    ),
    IB_CFGMAP_INIT_ENTRY(
        "buffer_res",
        IB_FTYPE_NUM,
        &core_global_cfg,
        buffer_res,
        0
    ),

    /* Audit Log */
    IB_CFGMAP_INIT_ENTRY(
        "audit_engine",
        IB_FTYPE_NUM,
        &core_global_cfg,
        audit_engine,
        0
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog_index",
        IB_FTYPE_NULSTR,
        &core_global_cfg,
        auditlog_index,
        "ironbee-index.log"
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog_dmode",
        IB_FTYPE_NUM,
        &core_global_cfg,
        auditlog_dmode,
        0700
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog_fmode",
        IB_FTYPE_NUM,
        &core_global_cfg,
        auditlog_fmode,
        0600
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog_parts",
        IB_FTYPE_NUM,
        &core_global_cfg,
        auditlog_parts,
        IB_ALPARTS_DEFAULT
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog_dir",
        IB_FTYPE_NULSTR,
        &core_global_cfg,
        auditlog_dir,
        "/var/log/ironbee"
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog_sdir_fmt",
        IB_FTYPE_NULSTR,
        &core_global_cfg,
        auditlog_sdir_fmt,
        ""
    ),
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_AUDIT,
        IB_FTYPE_NULSTR,
        &core_global_cfg,
        audit,
        MODULE_NAME_STR
    ),

    /* Data Acquisition */
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
    core_context_init,                   /**< Context init function */
);
