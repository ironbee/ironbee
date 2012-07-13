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
 * @brief IronBee &mdash; Core Module
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/core.h>

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif

#include <ironbee/mpool.h>
#include <ironbee/bytestr.h>
#include <ironbee/string.h>
#include <ironbee/cfgmap.h>
#include <ironbee/field.h>
#include <ironbee/rule_defs.h>
#include <ironbee/rule_engine.h>
#include <ironbee/debug.h>
#include <ironbee/util.h>
#include <ironbee/provider.h>
#include <ironbee/clock.h>

#include "rule_engine_private.h"
#include "core_private.h"
#include "engine_private.h"

#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>

#define MODULE_NAME        core
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

IB_MODULE_DECLARE();

/* The default shell to use for piped commands. */
static const char * const ib_pipe_shell = "/bin/sh";

/* The default UUID value */
static const char * const ib_uuid_default_str = "00000000-0000-0000-0000-000000000000";

#ifndef MODULE_BASE_PATH
/* Always define a module base path. */
#define MODULE_BASE_PATH /usr/local/ironbee/lib
#endif

#ifndef RULE_BASE_PATH
/* Always define a rule base path. */
#define RULE_BASE_PATH /usr/local/ironbee/lib
#endif

/// @todo Fix this:
#ifndef X_MODULE_BASE_PATH
#define X_MODULE_BASE_PATH IB_XSTRINGIFY(MODULE_BASE_PATH) "/"
#endif

/// @todo Fix this:
#ifndef X_RULE_BASE_PATH
#define X_RULE_BASE_PATH IB_XSTRINGIFY(RULE_BASE_PATH) "/"
#endif


/* Instantiate a module global configuration. */
static ib_core_cfg_t core_global_cfg;

#define IB_ALPART_HEADER                  (1<< 0)
#define IB_ALPART_EVENTS                  (1<< 1)
#define IB_ALPART_HTTP_REQUEST_METADATA   (1<< 2)
#define IB_ALPART_HTTP_REQUEST_HEADER     (1<< 3)
#define IB_ALPART_HTTP_REQUEST_BODY       (1<< 4)
#define IB_ALPART_HTTP_REQUEST_TRAILER    (1<< 5)
#define IB_ALPART_HTTP_RESPONSE_METADATA  (1<< 6)
#define IB_ALPART_HTTP_RESPONSE_HEADER    (1<< 7)
#define IB_ALPART_HTTP_RESPONSE_BODY      (1<< 8)
#define IB_ALPART_HTTP_RESPONSE_TRAILER   (1<< 9)
#define IB_ALPART_DEBUG_FIELDS            (1<<10)

/* NOTE: Make sure to add new parts from above to any groups below. */

#define IB_ALPARTS_ALL \
    IB_ALPART_HEADER|IB_ALPART_EVENTS| \
    IB_ALPART_HTTP_REQUEST_METADATA|IB_ALPART_HTTP_REQUEST_HEADER |\
    IB_ALPART_HTTP_REQUEST_BODY|IB_ALPART_HTTP_REQUEST_TRAILER | \
    IB_ALPART_HTTP_RESPONSE_METADATA|IB_ALPART_HTTP_RESPONSE_HEADER | \
    IB_ALPART_HTTP_RESPONSE_BODY|IB_ALPART_HTTP_RESPONSE_TRAILER | \
    IB_ALPART_DEBUG_FIELDS

#define IB_ALPARTS_DEFAULT \
    IB_ALPART_HEADER|IB_ALPART_EVENTS| \
    IB_ALPART_HTTP_REQUEST_METADATA|IB_ALPART_HTTP_REQUEST_HEADER |\
    IB_ALPART_HTTP_REQUEST_TRAILER | \
    IB_ALPART_HTTP_RESPONSE_METADATA|IB_ALPART_HTTP_RESPONSE_HEADER | \
    IB_ALPART_HTTP_RESPONSE_TRAILER

#define IB_ALPARTS_REQUEST \
    IB_ALPART_HTTP_REQUEST_METADATA|IB_ALPART_HTTP_REQUEST_HEADER |\
    IB_ALPART_HTTP_REQUEST_BODY|IB_ALPART_HTTP_REQUEST_TRAILER

#define IB_ALPARTS_RESPONSE \
    IB_ALPART_HTTP_RESPONSE_METADATA|IB_ALPART_HTTP_RESPONSE_HEADER | \
    IB_ALPART_HTTP_RESPONSE_BODY|IB_ALPART_HTTP_RESPONSE_TRAILER

/* -- Utilities -- */

/**
 * Duplicate a file handle
 *
 * This is a simple function which basically does fdopen(dup(fileno(fp)))
 * with some error checking.  This code takes care to make sure that
 * a file handle isn't leaked in the process.
 *
 * @param[in] fh File handle
 *
 * @returns New file handle (or NULL).
 */
static FILE *fdup( FILE *fh )
{
    int      fd;
    int      new_fd = -1;
    FILE    *new_fh = NULL;

    // Step 1: Get the file descriptor of the file handle
    fd = fileno(fh);
    if ( fd < 0 ) {
        return NULL;
    }

    // Step 2: Get a new file descriptor (via dup(2) )
    new_fd = dup(fd);
    if ( new_fd < 0 ) {
        return NULL;
    }

    // Step 3: Create a new file handle from the new file descriptor
    new_fh = fdopen(new_fd, "a");
    if ( new_fh == NULL ) {
        // Close the file descriptor if fdopen() fails!!
        close( new_fd );
    }

    // Done
    return new_fh;
}

/**
 * Unescape a value using ib_util_unescape_string.
 *
 * It is guaranteed that @a dst will not be populated with a string
 * containing a premature EOL.
 *
 * @param[in,out] ib The ib->mp will be used to allocated @a *dst. Logging
 *                will also be done through this.
 * @param[out] dst The resultant unescaped string will be stored at @a *dst.
 * @param[in] src The source string to be escaped
 * @return IB_OK, IB_EALLOC on malloc failures, or IB_EINVAL or IB_ETRUNC on
 *         unescaping failures.
 */
static ib_status_t core_unescape(ib_engine_t *ib, char **dst, const char *src)
{
    IB_FTRACE_INIT();
    size_t src_len = strlen(src);
    char *dst_tmp = ib_mpool_alloc(ib->mp, src_len+1);
    size_t dst_len;
    ib_status_t rc;

    if ( dst_tmp == NULL ) {
        ib_log_debug(ib, "Failed to allocate memory for unescaping.");
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ib_util_unescape_string(dst_tmp,
                                 &dst_len,
                                 src,
                                 src_len,
                                 IB_UTIL_UNESCAPE_NULTERMINATE |
                                 IB_UTIL_UNESCAPE_NONULL);

    if (rc != IB_OK) {
        const char *msg = (rc == IB_EBADVAL) ?
            "Failed to unescape string \"%s\" because resultant unescaped "
                "string contains a NULL character." :
            "Failed to unescape string \"%s\"";
        ib_log_debug(ib, msg, src);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Success! */
    *dst = dst_tmp;

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Core Logger Provider -- */

/**
 * Core debug logger.
 *
 * This is just a simple default logger that prints to stderr. Typically
 * a plugin will register a more elaborate logger and this will not be used,
 * except during startup prior to the registration of another logger.
 *
 * @param data Logger data (FILE *)
 * @param level Log level
 * @param ib IronBee engine
 * @param file Source code filename (typically __FILE__) or NULL
 * @param line Source code line number (typically __LINE__) or NULL
 * @param fmt Printf like format string
 * @param ap Variable length parameter list
 */
static void core_logger(void *data, ib_log_level_t level,
                        const ib_engine_t *ib,
                        const char *file, int line,
                        const char *fmt, va_list ap)
{
    IB_FTRACE_INIT();

    char *new_fmt;
    char time_info[31];
    FILE *fp = (FILE *)data;

    ib_clock_timestamp(time_info, NULL);

    /* 100 is more than sufficient. */
    new_fmt = (char *)malloc(strlen(time_info) + strlen(fmt) + 100);
    sprintf(new_fmt, "%s %-10s- ", time_info, ib_log_level_to_string(level));

    if ( (file != NULL) && (line > 0) ) {
        ib_core_cfg_t *corecfg = NULL;
        ib_status_t rc = ib_context_module_config(ib_context_main(ib),
                                                  ib_core_module(),
                                                  (void *)&corecfg);
        if ( (rc == IB_OK) && ((int)corecfg->log_level >= IB_LOG_DEBUG) ) {
            while ( (file != NULL) && (strncmp(file, "../", 3) == 0) ) {
                file += 3;
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
    }

    strcat(new_fmt, fmt);
    strcat(new_fmt, "\n");

    vfprintf(fp, new_fmt, ap);
    fflush(fp);

    free(new_fmt);

    IB_FTRACE_RET_VOID();
}

/**
 * Logger provider interface mapping for the core module.
 */
static IB_PROVIDER_IFACE_TYPE(logger) core_logger_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,
    core_logger
};


/* -- Core Log Event Provider -- */

static ib_status_t core_logevent_write(ib_provider_inst_t *epi, ib_logevent_t *e)
{
    ib_log_notice(epi->pr->ib, "Event [id %016" PRIx32 "][type %d]: %s",
                 e->event_id, e->type, e->msg);
    return IB_OK;
}

static IB_PROVIDER_IFACE_TYPE(logevent) core_logevent_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,
    core_logevent_write
};


/* -- Audit Provider -- */

typedef struct core_audit_cfg_t core_audit_cfg_t;
/**
 * Core audit configuration structure
 */
struct core_audit_cfg_t {
    FILE           *index_fp;       /**< Index file pointer */
    FILE           *fp;             /**< Audit log file pointer */
    const char     *fn;             /**< Audit log file name */
    const char     *full_path;      /**< Audit log full path */
    const char     *temp_path;      /**< Full path to temporary filename */
    int             parts_written;  /**< Parts written so far */
    const char     *boundary;       /**< Audit log boundary */
    ib_tx_t        *tx;             /**< Transaction being logged */
};

/// @todo Make this public
static ib_status_t ib_auditlog_part_add(ib_auditlog_t *log,
                                        const char *name,
                                        const char *type,
                                        void *data,
                                        ib_auditlog_part_gen_fn_t generator,
                                        void *gen_data)
{
    IB_FTRACE_INIT();
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

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Set cfg->fn to the file name and cfg->fp to the FILE* of the audit log.
 *
 * @param[in] lpi Log provider instance.
 * @param[in] log Audit Log that will be written. Contains the context and
 *            other information.
 * @param[in] cfg The configuration.
 * @param[in] corecfg The core configuration.
 */
static ib_status_t core_audit_open_auditfile(ib_provider_inst_t *lpi,
                                             ib_auditlog_t *log,
                                             core_audit_cfg_t *cfg,
                                             ib_core_cfg_t *corecfg)
{
    IB_FTRACE_INIT();

    const int dtmp_sz = 64;
    const int dn_sz = 512;
    char *dtmp = (char *)malloc(dtmp_sz);
    char *dn = (char *)malloc(dn_sz);
    char *audit_filename;
    int audit_filename_sz;
    char *temp_filename;
    int temp_filename_sz;
    const time_t log_seconds = IB_CLOCK_SECS(log->tx->t.logtime);
    int sys_rc;
    ib_status_t ib_rc;
    struct tm gmtime_result;
    ib_site_t *site;

    if (dtmp == NULL || dn == NULL) {
        ib_log_error(log->ib,  "Failed to allocate internal buffers.");
        if (dtmp != NULL) {
            free(dtmp);
        }
        if (dn != NULL) {
            free(dn);
        }
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    gmtime_r(&log_seconds, &gmtime_result);

    /* Generate the audit log filename template. */
    if (*(corecfg->auditlog_sdir_fmt) != 0) {
        size_t ret = strftime(dtmp, dtmp_sz,
                              corecfg->auditlog_sdir_fmt, &gmtime_result);
        if (ret == 0) {
            /// @todo Better error - probably should validate at cfg time
            ib_log_error(log->ib,
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
    sys_rc = snprintf(dn, dn_sz, "%s%s%s",
                  corecfg->auditlog_dir, (*dtmp)?"/":"", dtmp);
    if (sys_rc >= dn_sz) {
        /// @todo Better error.
        ib_log_error(log->ib,
                     "Could not create audit log directory: too long");
        free(dtmp);
        free(dn);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Generate the full audit log filename. */
    site = ib_context_site_get(log->ctx);
    if (site != NULL) {
        audit_filename_sz = strlen(dn) + strlen(cfg->tx->id) +
            strlen(site->id_str) + 7;
        audit_filename = (char *)ib_mpool_alloc(cfg->tx->mp, audit_filename_sz);
        sys_rc = snprintf(audit_filename,
                          audit_filename_sz,
                          "%s/%s_%s.log", dn, cfg->tx->id,site->id_str);
    }
    else {
        audit_filename_sz = strlen(dn) + strlen(cfg->tx->id) + 6;
        audit_filename = (char *)ib_mpool_alloc(cfg->tx->mp, audit_filename_sz);
        sys_rc = snprintf(audit_filename,
                          audit_filename_sz,
                          "%s/%s.log", dn, cfg->tx->id);
    }
    if (sys_rc >= (int)audit_filename_sz) {
        /// @todo Better error.
        ib_log_error(log->ib,
                     "Could not create audit log filename: too long");
        free(dtmp);
        free(dn);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_rc = ib_util_mkpath(dn, corecfg->auditlog_dmode);
    if (ib_rc != IB_OK) {
        ib_log_error(log->ib,
                     "Could not create audit log dir: %s", dn);
        free(dtmp);
        free(dn);
        IB_FTRACE_RET_STATUS(ib_rc);
    }

    // Create temporary filename to use while writing the audit log
    temp_filename_sz = strlen(audit_filename) + 6;
    temp_filename = (char *)ib_mpool_alloc(cfg->tx->mp, temp_filename_sz);
    sys_rc = snprintf(temp_filename,
                      temp_filename_sz,
                      "%s.part", audit_filename);
    if (sys_rc >= (int)temp_filename_sz) {
        /// @todo Better error.
        ib_log_error(log->ib,
                     "Could not create temporary audit log filename: too long");
        free(dtmp);
        free(dn);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /// @todo Use corecfg->auditlog_fmode as file mode for new file
    cfg->fp = fopen(temp_filename, "ab");
    if (cfg->fp == NULL) {
        sys_rc = errno;
        /// @todo Better error.
        ib_log_error(log->ib,
                     "Could not open audit log \"%s\": %s (%d)",
                     temp_filename, strerror(sys_rc), sys_rc);
        free(dtmp);
        free(dn);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Track the relative audit log filename. */
    cfg->fn = audit_filename + (strlen(corecfg->auditlog_dir) + 1);
    cfg->full_path = audit_filename;
    cfg->temp_path = temp_filename;

    free(dtmp);
    free(dn);

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t core_audit_open_auditindexfile(ib_provider_inst_t *lpi,
                                                  ib_auditlog_t *log,
                                                  core_audit_cfg_t *cfg,
                                                  ib_core_cfg_t *corecfg)
{
    IB_FTRACE_INIT();

    char* index_file;
    int index_file_sz;
    ib_status_t ib_rc;
    int sys_rc;

    /* Lock the auditlog configuration for the context.
     * We lock up here to ensure that external resources are not
     * double-opened instead of locking only the assignment to
     * log->ctx->auditlog->index_fp at the bottom of this block. */
    ib_lock_lock(&log->ctx->auditlog->index_fp_lock);

    if (log->ctx->auditlog->index[0] == '/') {
        index_file_sz = strlen(log->ctx->auditlog->index) + 1;

        index_file = (char *)ib_mpool_alloc(cfg->tx->mp, index_file_sz);
        if (index_file == NULL) {
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        memcpy(index_file, log->ctx->auditlog->index, index_file_sz);
    }
    else if (log->ctx->auditlog->index[0] == '|') {
        /// @todo Probably should skip whitespace???
        index_file_sz = strlen(log->ctx->auditlog->index + 1) + 1;

        index_file = (char *)ib_mpool_alloc(cfg->tx->mp, index_file_sz);
        if (index_file == NULL) {
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        memcpy(index_file, log->ctx->auditlog->index + 1, index_file_sz);
    }
    else {
        ib_rc = ib_util_mkpath(corecfg->auditlog_dir, corecfg->auditlog_dmode);
        if (ib_rc != IB_OK) {
            ib_log_error(log->ib,
                         "Could not create audit log dir: %s",
                         corecfg->auditlog_dir);
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(ib_rc);
        }

        index_file_sz = strlen(corecfg->auditlog_dir) +
                        strlen(log->ctx->auditlog->index) + 2;

        index_file = (char *)ib_mpool_alloc(cfg->tx->mp, index_file_sz);
        if (index_file == NULL) {
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        sys_rc = snprintf(index_file, index_file_sz, "%s/%s",
                          corecfg->auditlog_dir,
                          log->ctx->auditlog->index);
        if (sys_rc >= index_file_sz) {
            ib_log_error(log->ib,
                         "Could not create audit log index \"%s/%s\":"
                         " too long",
                         corecfg->auditlog_dir,
                         log->ctx->auditlog->index);
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    if (log->ctx->auditlog->index[0] == '|') {
        int p[2];
        pid_t pipe_pid;

        /// @todo Handle exit of pipe_pid???

        sys_rc = pipe(p);
        if (sys_rc != 0) {
            ib_log_error(log->ib,
                         "Could not create piped audit log index: %s (%d)",
                         strerror(sys_rc), sys_rc);
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Create a new process for executing the piped command. */
        pipe_pid = fork();
        if (pipe_pid == 0) {
            /* Child - piped audit log index process */
            char *parg[4];

            /// @todo Reset SIGCHLD in child???

            /* Setup the filehandles to read from pipe. */
            close(3); /// @todo stderr
            close(p[1]);
            dup2(p[0], 0);

            /* Execute piped command. */
            parg[0] = (char *)ib_pipe_shell;
            parg[1] = (char *)"-c";
            parg[2] = index_file;
            parg[3] = NULL;
            ib_log_debug(log->ib,
                         "Executing piped audit log index: %s %s \"%s\"",
                         parg[0], parg[1], parg[2]);
            execvp(ib_pipe_shell, (char * const *)parg); /// @todo define shell
            sys_rc = errno;
            ib_log_error(log->ib,
                         "Could not execute piped audit log index "
                         "\"%s\": %s (%d)",
                         index_file, strerror(sys_rc), sys_rc);
            exit(1);
        }
        else if (pipe_pid == -1) {
            /* Error - no process created */
            sys_rc = errno;
            ib_log_error(log->ib,
                         "Could not create piped audit log index process: "
                         "%s (%d)",
                         strerror(sys_rc), sys_rc);
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Parent - IronBee process */

        /* Setup the filehandles to write to the pipe. */
        close(p[0]);
        cfg->index_fp = fdopen(p[1], "w");
        if (cfg->index_fp == NULL) {
            sys_rc = errno;
            ib_log_error(log->ib,
                         "Could not open piped audit log index: %s (%d)",
                         strerror(sys_rc), sys_rc);
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }
    else {
        /// @todo Use corecfg->auditlog_fmode as file mode for new file
        cfg->index_fp = fopen(index_file, "ab");
        if (cfg->index_fp == NULL) {
            sys_rc = errno;
            ib_log_error(log->ib,
                         "Could not open audit log index \"%s\": %s (%d)",
                         index_file, strerror(sys_rc), sys_rc);
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    log->ctx->auditlog->index_fp = cfg->index_fp;
    ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);

    ib_log_debug(log->ib, "AUDITLOG INDEX%s: %s",
                 (log->ctx->auditlog->index[0] == '|'?" (piped)":""),
                 index_file);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * If required, open the log files.
 *
 * There are two files opened. One is a single file to store the audit log.
 * The other is the shared audit log index file. This index file is
 * protected by a lock during open and close calls but not writes.
 *
 * This and core_audit_close are thread-safe.
 *
 * @param[in] lpi Log provider interface.
 * @param[in] log The log record.
 * @return IB_OK or other. See log file for details of failure.
 */
static ib_status_t core_audit_open(ib_provider_inst_t *lpi,
                                   ib_auditlog_t *log)
{
    IB_FTRACE_INIT();
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;
    ib_core_cfg_t *corecfg;
    ib_status_t rc;

    /* Non const struct we will build and then assign to
     * corecfg->auditlog_index_hp. */
    ib_logformat_t *auditlog_index_hp;

    assert(NULL != lpi);
    assert(NULL != log);
    assert(NULL != log->ctx);
    assert(NULL != log->ctx->auditlog);
    assert(NULL != log->ctx->auditlog->index);

    rc = ib_context_module_config(log->ctx, ib_core_module(),
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        ib_log_error(log->ib,  "Could not fetch core configuration: %s", ib_status_to_string(rc) );
        IB_FTRACE_RET_STATUS(rc);
    }

    assert(NULL != corecfg);

    /* Copy the FILE* into the core_audit_cfg_t. */
    if (log->ctx->auditlog->index_fp != NULL) {
        cfg->index_fp = log->ctx->auditlog->index_fp;
    }

    /* If we have a file name but no file pointer, assign cfg->index_fp. */
    else if ((log->ctx->auditlog->index != NULL) && (cfg->index_fp == NULL)) {

        /**
         * Open the audit log index file. If the file name starts with
         * a | a pipe is opened to a subprocess, etc...
         */
        rc = core_audit_open_auditindexfile(lpi, log, cfg, corecfg);

        if (rc != IB_OK) {
            ib_log_error(log->ib,  "Could not open auditlog index.");
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Open audit file that contains the record identified by the line
     * written in index_fp. */
    if (cfg->fp == NULL) {
        rc = core_audit_open_auditfile(lpi, log, cfg, corecfg);

        if (rc!=IB_OK) {
            ib_log_error(log->ib,  "Failed to open audit log file.");
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Set the Audit Log index format */
    if (corecfg->auditlog_index_hp == NULL) {
        rc = ib_logformat_create(log->ib->mp, &auditlog_index_hp);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
        if (corecfg->auditlog_index_fmt != NULL) {
            rc = ib_logformat_set(auditlog_index_hp,
                                  corecfg->auditlog_index_fmt);
        }
        else {
            rc = ib_logformat_set(auditlog_index_hp, IB_LOGFORMAT_DEFAULT);
        }
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Commit built struct. */
        corecfg->auditlog_index_hp = auditlog_index_hp;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Write audit log header. This is not thread-safe and should be protected
 * with a lock.
 *
 * @param[in] lpi Log provider interface.
 * @param[in] log The log record.
 * @return IB_OK or IB_EUNKNOWN.
 */
static ib_status_t core_audit_write_header(ib_provider_inst_t *lpi,
                                           ib_auditlog_t *log)
{
    IB_FTRACE_INIT();
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;
    char header[256];
    size_t hlen;
    int ret = snprintf(header, sizeof(header),
                       "MIME-Version: 1.0\r\n"
                       "Content-Type: multipart/mixed; boundary=%s\r\n"
                       "\r\n"
                       "This is a multi-part message in MIME format.\r\n"
                       "\r\n",
                       cfg->boundary);
    if ((size_t)ret >= sizeof(header)) {
        /* Did not fit in buffer.  Since this is currently a more-or-less
         * fixed size, we abort here as this is a programming error.
         */
        abort();
    }

    hlen = strlen(header);
    if (fwrite(header, hlen, 1, cfg->fp) != 1) {
        ib_log_error(lpi->pr->ib,  "Failed to write audit log header");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }
    fflush(cfg->fp);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Write part of a audit log. This call should be protected by a lock.
 *
 * @param[in] lpi Log provider interface.
 * @param[in] part The log record.
 * @return IB_OK or other. See log file for details of failure.
 */
static ib_status_t core_audit_write_part(ib_provider_inst_t *lpi,
                                         ib_auditlog_part_t *part)
{
    IB_FTRACE_INIT();
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
            ib_log_error(lpi->pr->ib,  "Failed to write audit log part");
            fflush(cfg->fp);
            IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
        }
        cfg->parts_written++;
    }

    /* Finish the part. */
    fflush(cfg->fp);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Write an audit log footer. This call should be protected by a lock.
 *
 * @param[in] lpi Log provider interface.
 * @param[in] log The log record.
 * @return IB_OK or other. See log file for details of failure.
 */
static ib_status_t core_audit_write_footer(ib_provider_inst_t *lpi,
                                           ib_auditlog_t *log)
{
    IB_FTRACE_INIT();
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;

    if (cfg->parts_written > 0) {
        fprintf(cfg->fp, "\r\n--%s--\r\n", cfg->boundary);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
 * Render the log index line. Line must have a size of
 * IB_LOGFORMAT_MAX_INDEX_LENGTH + 1
 *
 * @param lpi provider instance
 * @param log audit log instance
 * @param line buffer to store the line before writing to disk/pipe.
 * @param line_size Size of @a line.
 *
 * @returns Status code
 */
static ib_status_t core_audit_get_index_line(ib_provider_inst_t *lpi,
                                             ib_auditlog_t *log,
                                             char *line,
                                             int *line_size)
{
    IB_FTRACE_INIT();
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;
    ib_core_cfg_t *corecfg;
    ib_tx_t *tx = log->tx;
    ib_conn_t *conn = tx->conn;
    ib_site_t *site = ib_context_site_get(log->ctx);
    const ib_logformat_t *lf;
    ib_status_t rc;
    char *ptr = line;
    char *tstamp = NULL;
    uint8_t which;
    int i = 0;
    int l = 0;
    int used = 0;
    const char *aux = NULL;

    /* Retrieve corecfg to get the AuditLogIndexFormat */
    rc = ib_context_module_config(log->ctx, ib_core_module(),
                                  (void *)&corecfg);

    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    lf = corecfg->auditlog_index_hp;
    which = lf->literal_starts ? 1 : 0;

    for (; (i < lf->field_cnt || l < lf->literal_cnt) &&
            used < IB_LOGFORMAT_MAXLINELEN;)
    {
        if (which++ % 2 == 0) {
            int aux_i = 0;

            switch (lf->fields[i]) {
                case IB_LOG_FIELD_REMOTE_ADDR:
                    aux = tx->er_ipstr;
                    break;
                case IB_LOG_FIELD_LOCAL_ADDR:
                    aux = conn->local_ipstr;
                    break;
                case IB_LOG_FIELD_HOSTNAME:
                     aux = tx->hostname;
                    break;
                case IB_LOG_FIELD_SITE_ID:
                    if (site == NULL) {
                         aux = (char *)"-";
                    }
                    else {
                         aux = site->id_str;
                    }
                    break;
                case IB_LOG_FIELD_SENSOR_ID:
                     aux = log->ib->sensor_id_str;
                    break;
                case IB_LOG_FIELD_TRANSACTION_ID:
                     aux = tx->id;
                    break;
                case IB_LOG_FIELD_TIMESTAMP:
                    /* Prepare timestamp (only if needed) */
                    tstamp = (char *)ib_mpool_alloc(log->mp, 31);
                    if (tstamp == NULL) {
                        IB_FTRACE_RET_STATUS(IB_EALLOC);
                    }

                    ib_clock_timestamp(tstamp, &tx->tv_created);
                    aux = tstamp;
                    break;
                case IB_LOG_FIELD_LOG_FILE:
                    aux = cfg->fn;
                    break;
                default:
                    ptr[used++] = '\n';
                    /* Not understood */
                    IB_FTRACE_RET_STATUS(IB_EINVAL);
                    break;
            }

            for (; aux != NULL && aux[aux_i] != '\0';) {
                if (used < IB_LOGFORMAT_MAXLINELEN) {
                    ptr[used++] = aux[aux_i++];
                }
                else {
                    ptr[used++] = '\n';
                    IB_FTRACE_RET_STATUS(IB_ETRUNC);
                }
            }
            ++i;
        }
        else {
            /* Use literals */
            if (used + lf->literals_len[l] < IB_LOGFORMAT_MAXLINELEN) {
                memcpy(&ptr[used], lf->literals[l], lf->literals_len[l]);
                used += lf->literals_len[l];
                ++l;
            }
            else {
                /* Truncated.. */
                ptr[used++] = '\n';
                IB_FTRACE_RET_STATUS(IB_ETRUNC);
            }
        }
    }
    ptr[used++] = '\n';
    *line_size = used;

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t core_audit_close(ib_provider_inst_t *lpi,
                                    ib_auditlog_t *log)
{
    IB_FTRACE_INIT();
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;
    ib_core_cfg_t *corecfg;
    ib_status_t ib_rc;
    int sys_rc;
    char line[IB_LOGFORMAT_MAXLINELEN + 2];
    int line_size = 0;

    /* Retrieve corecfg to get the AuditLogIndexFormat */
    ib_rc = ib_context_module_config(log->ctx, ib_core_module(),
                                     &corecfg);
    if (ib_rc != IB_OK) {
        ib_log_alert(log->ib,  "Failure accessing core module: %s", ib_status_to_string(ib_rc));
        IB_FTRACE_RET_STATUS(ib_rc);
    }

    /* Close the audit log. */
    if (cfg->fp != NULL) {
        fclose(cfg->fp);
        //rename temp to real
        sys_rc = rename(cfg->temp_path, cfg->full_path);
        if (sys_rc != 0) {
            sys_rc = errno;
            ib_log_error(log->ib,
                         "Error renaming auditlog %s: %s (%d)",
                         cfg->temp_path,
                         strerror(sys_rc), sys_rc);
            IB_FTRACE_RET_STATUS(IB_EOTHER);
        }
        ib_log_debug(log->ib, "AUDITLOG: %s", cfg->full_path);
        cfg->fp = NULL;
    }

    /* Write to the index file if using one. */
    if ((cfg->index_fp != NULL) && (cfg->parts_written > 0)) {

        ib_lock_lock(&log->ctx->auditlog->index_fp_lock);

        ib_rc = core_audit_get_index_line(lpi, log, line, &line_size);

        if (ib_rc != IB_OK) {
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(ib_rc);
        }

        sys_rc = fwrite(line, line_size, 1, cfg->index_fp);

        if (sys_rc < 0) {
            sys_rc = errno;
            ib_log_error(log->ib,
                         "Could not write to audit log index: %s (%d)",
                         strerror(sys_rc), sys_rc);

            /// @todo Should retry (a piped logger may have died)
            fclose(cfg->index_fp);
            cfg->index_fp = NULL;

            log->ctx->auditlog->index_fp = cfg->index_fp;

            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(IB_OK);
        }

        fflush(cfg->index_fp);
        ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
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
 * Core data provider implementation to add a data field.
 *
 * @param dpi Data provider instance.
 * @param f Field.
 * @param name Name of field.
 * @param nlen Length of @a name.
 *
 * @returns Status code
 */
static ib_status_t core_data_add(ib_provider_inst_t *dpi,
                                 ib_field_t *f,
                                 const char *name,
                                 size_t nlen)
{
    IB_FTRACE_INIT();
    /// @todo Needs to be more field-aware (handle lists, etc)
    /// @todo Needs to not allow adding if already exists (except list items)
    ib_status_t rc = ib_hash_set_ex((ib_hash_t *)dpi->data,
                                    (void *)name, nlen, f);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Core data provider implementation to set a data field.
 *
 * @param dpi Data provider instance.
 * @param f Field.
 * @param name Name of field.
 * @param nlen Length of @a name.
 *
 * @returns Status code
 */
static ib_status_t core_data_set(ib_provider_inst_t *dpi,
                                 ib_field_t *f,
                                 const char *name,
                                 size_t nlen)
{
    IB_FTRACE_INIT();
    /// @todo Needs to be more field-aware (handle lists, etc)
    ib_status_t rc = ib_hash_set_ex((ib_hash_t *)dpi->data,
                                    (void *)name, nlen, f);
    IB_FTRACE_RET_STATUS(rc);
}

/**
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
    IB_FTRACE_INIT();
    ib_field_t *f;
    ib_status_t rc;
    ib_num_t num;
    ib_unum_t unum;

    rc = ib_hash_get_ex(
        (ib_hash_t *)dpi->data,
        &f,
        (void *)name, nlen
    );
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    switch (f->type) {
        case IB_FTYPE_NUM:
            /// @todo Make sure this is atomic
            /// @todo Check for overflow
            rc = ib_field_value(f, ib_ftype_num_out(&num));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
            num += adjval;
            rc = ib_field_setv(f, ib_ftype_num_in(&num));
            break;
        case IB_FTYPE_UNUM:
            /// @todo Make sure this is atomic
            /// @todo Check for overflow
            rc = ib_field_value(f, ib_ftype_unum_out(&unum));
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
            unum += adjval;
            rc = ib_field_setv(f, ib_ftype_unum_in(&unum));
            break;
        default:
            IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Core data provider implementation to get a data field.
 *
 * @param dpi Data provider instance
 * @param name Field name
 * @param nlen Field name length
 * @param pf Address which field will be written
 *
 * @returns Status code
 */
static ib_status_t core_data_get(const ib_provider_inst_t *dpi,
                                 const char *name,
                                 size_t nlen,
                                 ib_field_t **pf)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_hash_get_ex(
        (const ib_hash_t *)dpi->data,
        pf,
        (void *)name, nlen
    );

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Core data provider implementation to get all data fields.
 *
 * @param dpi Data provider instance
 * @param list List which fields will be pushed
 *
 * @returns Status code
 */
static ib_status_t core_data_get_all(const ib_provider_inst_t *dpi,
                                     ib_list_t *list)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_hash_get_all((const ib_hash_t *)dpi->data, list);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Core data provider implementation to remove a data field.
 *
 * The data field which is removed is written to @a pf if it
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
    IB_FTRACE_INIT();
    ib_status_t rc = ib_hash_remove_ex(
        (ib_hash_t *)dpi->data,
        pf,
        (void *)name, nlen
    );
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Core data provider implementation to clear the data store.
 *
 * @param dpi Data provider instance
 *
 * @returns Status code
 */
static ib_status_t core_data_clear(ib_provider_inst_t *dpi)
{
    IB_FTRACE_INIT();
    ib_hash_clear((ib_hash_t *)dpi->data);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
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
 * Core data provider API implementation to log data via va_list args.
 *
 * @param lpi Logger provider instance
 * @param level Log level
 * @param ib IronBee engine
 * @param file Source code filename (typically __FILE__)
 * @param line Source code line number (typically __LINE__)
 * @param fmt Printf like format string
 * @param ap Variable length parameter list
 *
 * @returns Status code
 */
static void logger_api_vlogmsg(ib_provider_inst_t *lpi,
                               int level,
                               const ib_engine_t *ib,
                               const char *file, int line,
                               const char *fmt, va_list ap)
{
    IB_PROVIDER_IFACE_TYPE(logger) *iface;
    ib_core_cfg_t *main_core_config = NULL;
    ib_context_t  *main_ctx = ib_context_main(ib);
    ib_provider_t *main_lp;
    ib_status_t rc;
    const char *uri = NULL;
    FILE *fp = NULL;            /* The file pointer to write to. */
    size_t new_fmt_length = 0;
    char *new_fmt = NULL;
    const char *which_fmt = NULL;

    /* Get the core context core configuration. */
    rc = ib_context_module_config(
        main_ctx,
        ib_core_module(),
        (void *)&main_core_config
    );

    /* If not available, fall back to the core global configuration. */
    if (rc != IB_OK) {
        main_core_config = &core_global_cfg;
    }

    /* Check the log level, return if we're not interested. */
    if (level > (int)main_core_config->log_level) {
        return;
    }

    /* Prefix pid.*/
    new_fmt_length = strlen(fmt) + 10;
    new_fmt = NULL;
    which_fmt = NULL;

    new_fmt = (char *)malloc(new_fmt_length);
    if (new_fmt == NULL) {
        which_fmt = fmt;
    }
    else {
        which_fmt = new_fmt;
        snprintf(new_fmt, new_fmt_length, "[%d] %s", getpid(), fmt);
    }

    /* Get the current 'logger' provider interface. */
    iface = (IB_PROVIDER_IFACE_TYPE(logger) *)lpi->pr->iface;

    /* If it's not the core log provider, we're done: we know nothing.
     * about it's data, so don't try to treat it as a file handle! */
    main_lp = main_core_config->pi.logger->pr;
    if ( (main_lp != lpi->pr)
         || (iface->logger != core_logger) ) {
        iface->logger(lpi->data, level, ib, file, line, which_fmt, ap);
        goto done;
    }

    /* If no interface, do *something*.
     *  Note that this should be the same as the default case. */
    if (iface == NULL) {
        core_logger(stderr, level, ib, file, line, which_fmt, ap);
        goto done;
    }

    /* Get the current file pointer. */
    fp = (FILE *) lpi->data;

    /* Pull the log URI from the core config. */
    if (fp == NULL) {
        uri = main_core_config->log_uri;

        /* If the URI looks like a file, try to open it. */
        if ((uri != NULL) && (strncmp(uri, "file://", 7) == 0)) {
            const char *path = uri+7;
            fp = fopen( path, "a" );
            if (fp == NULL) {
                fprintf(stderr,
                        "Failed to open log file '%s' for writing: %s\n",
                        path, strerror(errno));
            }
        }
        /* Else no log URI specified.  Will use stderr below. */
    }

    /* Finally, use stderr as a fallback. */
    if (fp == NULL) {
        fp = fdup(stderr);
    }

    /* Copy the file pointer to the interface data.  We do this to
     * cache the file handle so we don't open it each time. */
    lpi->data = fp;

    /* Just calls the interface logger with the provider instance data as
     * the first parameter (if the interface is implemented and not
     * just abstract).
     */
    iface->logger(
         fp,
         level,
         ib,
         file,
         line,
         which_fmt,
         ap
     );

done:
    if (new_fmt) {
        free(new_fmt);
    }
}

/**
 * Core data provider API implementation to log data via variable args.
 *
 * @param lpi Logger provider instance
 * @param level Log level
 * @param ib IronBee engine
 * @param file Source code filename (typically __FILE__)
 * @param line Source code line number (typically __LINE__)
 * @param fmt Printf like format string
 *
 * @returns Status code
 */
static void logger_api_logmsg(ib_provider_inst_t *lpi,
                              int level,
                              const ib_engine_t *ib,
                              const char *file, int line,
                              const char *fmt, ...)
{
    IB_PROVIDER_IFACE_TYPE(logger) *iface;
    ib_core_cfg_t *corecfg;
    ib_status_t rc;
    va_list ap;

    rc = ib_context_module_config(
        ib_context_main(lpi->pr->ib),
        ib_core_module(),
        (void *)&corecfg
    );
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
                      level, ib, file, line, fmt, ap);
    }

    va_end(ap);
}

/**
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
    IB_FTRACE_INIT();
    IB_PROVIDER_IFACE_TYPE(logger) *iface = (IB_PROVIDER_IFACE_TYPE(logger) *)lpr->iface;

    /* Check that versions match. */
    if (iface->version != IB_PROVIDER_VERSION_LOGGER) {
        IB_FTRACE_RET_STATUS(IB_EINCOMPAT);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
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
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Logger provider API mapping for core module.
 */
static IB_PROVIDER_API_TYPE(logger) logger_api = {
    logger_api_vlogmsg,
    logger_api_logmsg
};


/* -- Audit API Implementations -- */

/**
 * Write an audit log.
 *
 * @param lpi Audit provider
 *
 * @returns Status code
 */
static ib_status_t audit_api_write_log(ib_provider_inst_t *lpi)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_IFACE_TYPE(audit) *iface = (IB_PROVIDER_IFACE_TYPE(audit) *)lpi->pr->iface;
    ib_auditlog_t *log = (ib_auditlog_t *)lpi->data;
    ib_list_node_t *node;
    ib_status_t rc;

    if (ib_list_elements(log->parts) == 0) {
        ib_log_error(lpi->pr->ib,  "No parts to write to audit log");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Open the log if required. This is thread safe. */
    if (iface->open != NULL) {
        rc = iface->open(lpi, log);
        if (rc != IB_OK) {
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Lock to write. */
    rc = ib_lock_lock(&log->ctx->auditlog->index_fp_lock);

    if (rc!=IB_OK) {
        ib_log_error(lpi->pr->ib,
                     "Cannot lock %s for write.",
                     log->ctx->auditlog->index);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Write the header if required. */
    if (iface->write_header != NULL) {
        rc = iface->write_header(lpi, log);
        if (rc != IB_OK) {
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Write the parts. */
    IB_LIST_LOOP(log->parts, node) {
        ib_auditlog_part_t *part =
            (ib_auditlog_part_t *)ib_list_node_data(node);
        rc = iface->write_part(lpi, part);
        if (rc != IB_OK) {
            ib_log_error(log->ib,  "Failed to write audit log part: %s",
                         part->name);
        }
    }

    /* Write the footer if required. */
    if (iface->write_footer != NULL) {
        rc = iface->write_footer(lpi, log);
        if (rc != IB_OK) {
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Writing is done. Unlock. Close is thread-safe. */
    ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);

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
    IB_FTRACE_INIT();
    IB_PROVIDER_IFACE_TYPE(audit) *iface = (IB_PROVIDER_IFACE_TYPE(audit) *)lpr->iface;

    /* Check that versions match. */
    if (iface->version != IB_PROVIDER_VERSION_AUDIT) {
        IB_FTRACE_RET_STATUS(IB_EINCOMPAT);
    }

    /* Verify that required interface functions are implemented. */
    if (iface->write_part == NULL) {
        ib_log_alert(ib, "The write_part function "
                     "MUST be implemented by a audit provider");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Audit provider API mapping for core module.
 */
static IB_PROVIDER_API_TYPE(audit) audit_api = {
    audit_api_write_log
};


/* -- Logevent API Implementations -- */

/**
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
    IB_FTRACE_INIT();
    ib_list_t *events = (ib_list_t *)epi->data;

    ib_list_push(events, e);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
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
    IB_FTRACE_INIT();
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
 * Core logevent provider API implementation to fetch events.
 *
 * @param epi Logevent provider instance
 * @param pevents Event ID to remove
 *
 * @returns Status code
 */
static ib_status_t logevent_api_fetch_events(ib_provider_inst_t *epi,
                                             ib_list_t **pevents)
{
    IB_FTRACE_INIT();
    *pevents = (ib_list_t *)epi->data;
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Core logevent provider API implementation to write out (and remove)
 * all the pending events.
 *
 * @param epi Logevent provider instance
 *
 * @returns Status code
 */
static ib_status_t logevent_api_write_events(ib_provider_inst_t *epi)
{
    IB_FTRACE_INIT();
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

static size_t ib_auditlog_gen_raw_stream(ib_auditlog_part_t *part,
                                         const uint8_t **chunk)
{
    ib_sdata_t *sdata;
    size_t dlen;

    if (part->gen_data == NULL) {
        ib_stream_t *stream = (ib_stream_t *)part->part_data;

        /* No data. */
        if (stream->slen == 0) {
            *chunk = NULL;
            part->gen_data = (void *)-1;
            return 0;
        }

        sdata = (ib_sdata_t *)IB_LIST_FIRST(stream);
        dlen = sdata->dlen;
        *chunk = (const uint8_t *)sdata->data;

        sdata = IB_LIST_NODE_NEXT(sdata);
        if (sdata != NULL) {
            part->gen_data = sdata;
        }
        else {
            part->gen_data = (void *)-1;
        }

        return dlen;
    }
    else if (part->gen_data == (void *)-1) {
        part->gen_data = NULL;
        return 0;
    }

    sdata = (ib_sdata_t *)part->gen_data;
    dlen = sdata->dlen;
    *chunk = (const uint8_t *)sdata->data;

    sdata = IB_LIST_NODE_NEXT(sdata);
    if (sdata != NULL) {
        part->gen_data = sdata;
    }
    else {
        part->gen_data = (void *)-1;
    }

    return dlen;
}

static size_t ib_auditlog_gen_json_flist(ib_auditlog_part_t *part,
                                         const uint8_t **chunk)
{
    ib_engine_t *ib = part->log->ib;
    ib_field_t *f;
    uint8_t *rec;
    ib_status_t rc;

#define CORE_JSON_MAX_FIELD_LEN 256

    /* The gen_data field is used to store the current state. NULL
     * means the part has not started yet and a -1 value
     * means it is done. Anything else is a node in the event list.
     */
    if (part->gen_data == NULL) {
        ib_list_t *list = (ib_list_t *)part->part_data;

        /* No data. */
        if (ib_list_elements(list) == 0) {
            ib_log_error(ib, "No data in audit log part: %s", part->name);
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
        size_t rlen;

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
        {
            const char *ns;
            rc = ib_field_value(f, ib_ftype_nulstr_out(&ns));
            if (rc != IB_OK) {
                return 0;
            }

            rlen = snprintf((char *)rec, CORE_JSON_MAX_FIELD_LEN,
                            "  \"%" IB_BYTESTR_FMT "\": \"%s\"%s\r\n",
                            IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                            ns,
                            comma);
            break;
        }
        case IB_FTYPE_BYTESTR:
        {
            const ib_bytestr_t *bs;
            rc = ib_field_value(f, ib_ftype_bytestr_out(&bs));
            if (rc != IB_OK) {
                return 0;
            }

            rlen = snprintf((char *)rec, CORE_JSON_MAX_FIELD_LEN,
                            "  \"%" IB_BYTESTR_FMT "\": "
                            "\"%" IB_BYTESTR_FMT "\"%s\r\n",
                            IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                            IB_BYTESTR_FMT_PARAM(bs),
                            comma);
            break;
        }
        case IB_FTYPE_NUM:
        {
            ib_num_t n;
            rc = ib_field_value(f, ib_ftype_num_out(&n));
            if (rc != IB_OK) {
                return 0;
            }

            rlen = snprintf((char *)rec, CORE_JSON_MAX_FIELD_LEN,
                            "  \"%" IB_BYTESTR_FMT "\": "
                            "%" PRId64 "%s\r\n",
                            IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                            n,
                            comma);
            break;
        }
        case IB_FTYPE_UNUM:
        {
            ib_unum_t u;
            rc = ib_field_value(f, ib_ftype_unum_out(&u));
            if (rc != IB_OK) {
                return 0;
            }

            rlen = snprintf((char *)rec, CORE_JSON_MAX_FIELD_LEN,
                            "  \"%" IB_BYTESTR_FMT "\": "
                            "%" PRIu64 "%s\r\n",
                            IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                            u,
                            comma);
            break;
        }
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
            ib_log_notice(ib, "Item too large to log in part %s: %zd",
                         part->name, rlen);
            *chunk = (const uint8_t *)"\r\n";
            part->gen_data = (void *)-1;
            return strlen(*(const char **)chunk);
        }

        *chunk = rec;
    }
    else {
        ib_log_error(ib, "NULL field in part: %s", part->name);
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

static size_t ib_auditlog_gen_header_flist(ib_auditlog_part_t *part,
                                            const uint8_t **chunk)
{
    ib_engine_t *ib = part->log->ib;
    ib_field_t *f;
    uint8_t *rec;
    size_t rlen;
    ib_status_t rc;

#define CORE_HEADER_MAX_FIELD_LEN 8192

    /* The gen_data field is used to store the current state. NULL
     * means the part has not started yet and a -1 value
     * means it is done. Anything else is a node in the event list.
     */
    if (part->gen_data == NULL) {
        ib_list_t *list = (ib_list_t *)part->part_data;

        /* No data. */
        if (ib_list_elements(list) == 0) {
            ib_log_error(ib, "No data in audit log part: %s", part->name);
            part->gen_data = NULL;
            return 0;
        }

        /* First should be a request/response line. */
        part->gen_data = ib_list_first(list);
        f = (ib_field_t *)ib_list_node_data((ib_list_node_t *)part->gen_data);
        if ((f != NULL) && (f->type == IB_FTYPE_BYTESTR)) {
            const ib_bytestr_t *bs;
            rec = (uint8_t *)ib_mpool_alloc(part->log->mp, CORE_HEADER_MAX_FIELD_LEN);

            rc = ib_field_value(f, ib_ftype_bytestr_out(&bs));
            if (rc != IB_OK) {
                return 0;
            }

            rlen = snprintf((char *)rec, CORE_HEADER_MAX_FIELD_LEN,
                            "%" IB_BYTESTR_FMT "\r\n",
                            IB_BYTESTR_FMT_PARAM(bs));

            /* Verify size. */
            if (rlen >= CORE_HEADER_MAX_FIELD_LEN) {
                ib_log_notice(ib, "Item too large to log in part %s: %zd",
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
        ib_log_error(ib, "NULL field in part: %s", part->name);
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
    {
        const char *s;
        rc = ib_field_value(f, ib_ftype_nulstr_out(&s));
        if (rc != IB_OK) {
            return 0;
        }

        rlen = snprintf((char *)rec, CORE_HEADER_MAX_FIELD_LEN,
                        "%" IB_BYTESTR_FMT ": %s\r\n",
                        IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                        s);
        break;
    }
    case IB_FTYPE_BYTESTR:
    {
        const ib_bytestr_t *bs;
        rc = ib_field_value(f, ib_ftype_bytestr_out(&bs));
        if (rc != IB_OK) {
            return 0;
        }
        rlen = snprintf((char *)rec, CORE_HEADER_MAX_FIELD_LEN,
                        "%" IB_BYTESTR_FMT ": "
                        "%" IB_BYTESTR_FMT "\r\n",
                        IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                        IB_BYTESTR_FMT_PARAM(bs));
        break;
    }
    default:
        rlen = snprintf((char *)rec, CORE_HEADER_MAX_FIELD_LEN,
                        "%" IB_BYTESTR_FMT ": IronBeeError - unhandled header type %d\r\n",
                        IB_BYTESTRSL_FMT_PARAM(f->name, f->nlen),
                        f->type);
        break;
    }

    /* Verify size. */
    if (rlen >= CORE_HEADER_MAX_FIELD_LEN) {
        ib_log_error(ib, "Item too large to log in part %s: %zd",
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

/**
 * Placeholder function to escape data.
 *
 * @todo This is a placeholder!
 *
 * @param[in] data Data to escape
 * @param[in] dlen Length of data
 *
 * @returns Escaped string form of data
 */
static const char *ib_data_escape(const void *data, size_t dlen)
{
    if (data == NULL) {
        return "";
    }
    else {
        return (const char *) data;
    }
}

static size_t ib_auditlog_gen_json_events(ib_auditlog_part_t *part,
                                          const uint8_t **chunk)
{
    ib_engine_t *ib = part->log->ib;
    ib_list_t *list = (ib_list_t *)part->part_data;
    void *list_first;
    ib_logevent_t *e;
    uint8_t *rec;

#define CORE_JSON_MAX_REC_LEN 1024

    /* The gen_data field is used to store the current state. NULL
     * means the part has not started yet and a -1 value
     * means it is done. Anything else is a node in the event list.
     */
    if (part->gen_data == NULL) {
        /* No events. */
        if (ib_list_elements(list) == 0) {
            ib_log_error(ib, "No events in audit log");
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

    /* Used to detect the first event. */
    list_first = ib_list_first(list);

    e = (ib_logevent_t *)ib_list_node_data((ib_list_node_t *)part->gen_data);
    if (e != NULL) {
        size_t rlen;

        /* Turn tag list into JSON list, limiting the size. */
        char tags[128] = "\0";

        if (e->tags != NULL) {
            ib_list_node_t *tnode;
            size_t tags_len = sizeof(tags);
            char *tag_ptr = tags;

            IB_LIST_LOOP(e->tags, tnode) {
                char *tag = (char *)ib_list_node_data(tnode);
                int wrote = snprintf(tag_ptr, tags_len,
                                     "%s\"%s\"",
                                     (tag_ptr == tags ? "" : ", "), tag);


                /* Check that data was written, terminating if not. */
                if (wrote >= (int)tags_len) {
                    /* Not enough room. */
                    *tag_ptr = '\0';
                    break;
                }


                /* Adjust the length that remains in the tags buffer. */
                tags_len -= wrote;
                if (tags_len <= 0) {
                    break;
                }

                tag_ptr += wrote;
            }
        }

        rec = (uint8_t *)ib_mpool_alloc(part->log->mp, CORE_JSON_MAX_REC_LEN);

        /* Error. */
        if (rec == NULL) {
            *chunk = (const uint8_t *)"  ]\r\n}";
            return strlen(*(const char **)chunk);
        }

        ib_log_debug(ib, "TODO: Data escaping not implemented!");
        rlen = snprintf((char *)rec, CORE_JSON_MAX_REC_LEN,
                        "%s"
                        "    {\r\n"
                        "      \"event-id\": %" PRIu32 ",\r\n"
                        "      \"rule-id\": \"%s\",\r\n"
                        "      \"type\": \"%s\",\r\n"
                        "      \"rec-action\": \"%s\",\r\n"
                        "      \"action\": \"%s\",\r\n"
                        "      \"confidence\": %u,\r\n"
                        "      \"severity\": %u,\r\n"
                        "      \"tags\": [%s],\r\n"
                        // TODO Add fields
                        "      \"fields\": [],\r\n"
                        "      \"msg\": \"%s\",\r\n"
                        // TODO Add properly escaped (binary) data
                        "      \"data\": \"%s\"\r\n"
                        "    }",
                        (list_first == part->gen_data ? "" : ",\r\n"),
                        e->event_id,
                        e->rule_id ? e->rule_id : "-",
                        ib_logevent_type_name(e->type),
                        ib_logevent_action_name(e->rec_action),
                        ib_logevent_action_name(e->action),
                        e->confidence,
                        e->severity,
                        tags,
                        e->msg ? e->msg : "-",
                        ib_data_escape(e->data, e->data_len));

        /* Verify size. */
        if (rlen >= CORE_JSON_MAX_REC_LEN) {
            ib_log_error(ib, "Event too large to log: %zd", rlen);
            *chunk = (const uint8_t *)"    {}";
            part->gen_data = (void *)-1;
            return strlen(*(const char **)chunk);
        }

        *chunk = rec;
    }
    else {
        ib_log_error(ib, "NULL event");
        *chunk = (const uint8_t *)"    {}";
        part->gen_data = (void *)-1;
        return strlen(*(const char **)chunk);
    }
    part->gen_data = ib_list_node_next((ib_list_node_t *)part->gen_data);

    /* Close the json structure. */
    if (part->gen_data == NULL) {
        size_t clen = strlen(*(const char **)chunk);

        part->gen_data = (void *)-1;

        if (clen+8 > CORE_JSON_MAX_REC_LEN) {
            if (clen+2 > CORE_JSON_MAX_REC_LEN) {
                ib_log_error(ib, "Event too large to fit in buffer");
                *chunk = (const uint8_t *)"    {}\r\n  ]\r\n}";
            }
            memcpy(*(uint8_t **)chunk + clen, "]}", 2);
            return clen + 2;
        }
        memcpy(*(uint8_t **)chunk + clen, "\r\n  ]\r\n}", 8);
        return clen + 8;
    }

    return strlen(*(const char **)chunk);
}

#define CORE_AUDITLOG_FORMAT "http-message/1"

static ib_status_t ib_auditlog_add_part_header(ib_auditlog_t *log)
{
    IB_FTRACE_INIT();
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;
    ib_engine_t *ib = log->ib;
    ib_site_t *site;
    ib_mpool_t *pool = log->mp;
    ib_field_t *f;
    ib_list_t *list;
    char *tstamp;
    char *txtime;
    char *log_format;
    ib_status_t rc;

    /* Timestamp */
    tstamp = (char *)ib_mpool_alloc(pool, 30);
    if (tstamp == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    ib_clock_relative_timestamp(tstamp, &log->tx->tv_created,
                                (log->tx->t.logtime - log->tx->t.started));

    /* TX Time */
    txtime = (char *)ib_mpool_alloc(pool, 30);
    if (txtime == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    snprintf(txtime, 30, "%d",
             (int)(log->tx->t.response_finished - log->tx->t.request_started));

    /* Log Format */
    log_format = ib_mpool_strdup(pool, CORE_AUDITLOG_FORMAT);
    if (log_format == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, pool);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_field_create_bytestr_alias(&f, pool,
                       IB_FIELD_NAME("tx-time"),
                       (uint8_t *)txtime,
                       strlen(txtime));
    ib_list_push(list, f);

    ib_field_create_bytestr_alias(&f, pool,
                       IB_FIELD_NAME("log-timestamp"),
                       (uint8_t *)tstamp,
                       strlen(tstamp));
    ib_list_push(list, f);

    ib_field_create_bytestr_alias(&f, pool,
                       IB_FIELD_NAME("log-format"),
                       (uint8_t *)log_format,
                       strlen(log_format));
    ib_list_push(list, f);

    ib_field_create_bytestr_alias(&f, pool,
                       IB_FIELD_NAME("log-id"),
                       (uint8_t *)cfg->boundary,
                       strlen(cfg->boundary));
    ib_list_push(list, f);

    ib_field_create_bytestr_alias(&f, pool,
                       IB_FIELD_NAME("sensor-id"),
                       (uint8_t *)ib->sensor_id_str,
                       strlen(ib->sensor_id_str));
    ib_list_push(list, f);

    ib_field_create_bytestr_alias(&f, pool,
                       IB_FIELD_NAME("sensor-name"),
                       (uint8_t *)ib->sensor_name,
                       strlen(ib->sensor_name));
    ib_list_push(list, f);

    ib_field_create_bytestr_alias(&f, pool,
                       IB_FIELD_NAME("sensor-version"),
                       (uint8_t *)ib->sensor_version,
                       strlen(ib->sensor_version));
    ib_list_push(list, f);

    ib_field_create_bytestr_alias(&f, pool,
                       IB_FIELD_NAME("sensor-hostname"),
                       (uint8_t *)ib->sensor_hostname,
                       strlen(ib->sensor_hostname));
    ib_list_push(list, f);

    site = ib_context_site_get(log->ctx);
    if (site != NULL) {
        ib_field_create_bytestr_alias(&f, pool,
                           IB_FIELD_NAME("site-id"),
                           (uint8_t *)site->id_str,
                           strlen(site->id_str));
        ib_list_push(list, f);

        ib_field_create_bytestr_alias(&f, pool,
                           IB_FIELD_NAME("site-name"),
                           (uint8_t *)site->name,
                           strlen(site->name));
        ib_list_push(list, f);
    }

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
    IB_FTRACE_INIT();
    ib_list_t *list;
    ib_status_t rc;

    /* Get the list of events. */
    rc = ib_event_get_all(log->tx->epi, &list);
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
    IB_FTRACE_INIT();
    ib_tx_t *tx = log->tx;
    ib_unum_t tx_num = tx ? tx->conn->tx_count : 0;
    ib_mpool_t *pool = log->mp;
    ib_field_t *f;
    ib_list_t *list;
    char *tstamp;
    ib_status_t rc;

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, pool);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_field_create(&f, pool,
                    IB_FIELD_NAME("tx-num"),
                    IB_FTYPE_UNUM,
                    ib_ftype_unum_in(&tx_num));
    ib_list_push(list, f);

    if (tx != NULL) {
        ib_unum_t unum;

        /* Timestamp */
        tstamp = (char *)ib_mpool_alloc(pool, 30);
        if (tstamp == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
        ib_clock_relative_timestamp(tstamp, &tx->tv_created,
                                    (tx->t.request_started - tx->t.started));

        ib_field_create_bytestr_alias(&f, pool,
                           IB_FIELD_NAME("request-timestamp"),
                           (uint8_t *)tstamp,
                           strlen(tstamp));
        ib_list_push(list, f);

        ib_field_create_bytestr_alias(&f, pool,
                           IB_FIELD_NAME("tx-id"),
                           (uint8_t *)tx->id,
                           strlen(tx->id));
        ib_list_push(list, f);

        ib_field_create_bytestr_alias(&f, pool,
                           IB_FIELD_NAME("remote-addr"),
                           (uint8_t *)tx->er_ipstr,
                           strlen(tx->er_ipstr));
        ib_list_push(list, f);

        unum = tx->conn->remote_port;
        ib_field_create(&f, pool,
                        IB_FIELD_NAME("remote-port"),
                        IB_FTYPE_UNUM,
                        ib_ftype_unum_in(&unum));
        ib_list_push(list, f);

        ib_field_create_bytestr_alias(&f, pool,
                           IB_FIELD_NAME("local-addr"),
                           (uint8_t *)tx->conn->local_ipstr,
                           strlen(tx->conn->local_ipstr));
        ib_list_push(list, f);

        unum = tx->conn->local_port;
        ib_field_create(&f, pool,
                        IB_FIELD_NAME("local-port"),
                        IB_FTYPE_UNUM,
                        ib_ftype_unum_in(&unum));
        ib_list_push(list, f);

        /// @todo If this is NULL, parser failed - what to do???
        if (tx->path != NULL) {
            ib_field_create_bytestr_alias(&f, pool,
                               IB_FIELD_NAME("request-uri-path"),
                               (uint8_t *)tx->path,
                               strlen(tx->path));
            ib_list_push(list, f);
        }

        rc = ib_data_get_ex(tx->dpi, IB_S2SL("request_protocol"), &f);
        if (rc == IB_OK) {
            ib_list_push(list, f);
        }
        else {
            ib_log_error_tx(tx, "Failed to get request_protocol: %s", ib_status_to_string(rc));
        }

        rc = ib_data_get_ex(tx->dpi, IB_S2SL("request_method"), &f);
        if (rc == IB_OK) {
            ib_list_push(list, f);
        }
        else {
            ib_log_error_tx(tx, "Failed to get request_method: %s", ib_status_to_string(rc));
        }

        /// @todo If this is NULL, parser failed - what to do???
        if (tx->hostname != NULL) {
            ib_field_create_bytestr_alias(&f, pool,
                               IB_FIELD_NAME("request-hostname"),
                               (uint8_t *)tx->hostname,
                               strlen(tx->hostname));
            ib_list_push(list, f);
        }
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
    IB_FTRACE_INIT();
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
    ib_clock_relative_timestamp(tstamp, &tx->tv_created,
                                (tx->t.response_started - tx->t.started));

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, pool);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_field_create_bytestr_alias(&f, pool,
                       IB_FIELD_NAME("response-timestamp"),
                       (uint8_t *)tstamp,
                       strlen(tstamp));
    ib_list_push(list, f);

    rc = ib_data_get_ex(tx->dpi, IB_S2SL("response_status"), &f);
    if (rc == IB_OK) {
        ib_list_push(list, f);
    }
    else {
        ib_log_error_tx(tx, "Failed to get response_status: %s", ib_status_to_string(rc));
    }

    rc = ib_data_get_ex(tx->dpi, IB_S2SL("response_protocol"), &f);
    if (rc == IB_OK) {
        ib_list_push(list, f);
    }
    else {
        ib_log_error_tx(tx, "Failed to get response_protocol: %s", ib_status_to_string(rc));
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

/**
 * Add request/response header fields to the audit log
 *
 * @param[in] tx Transaction
 * @param[in] mpool Memory pool to user for allocations
 * @param[in,out] list List to add the fields to
 * @param[in] label Label string ("request"/"response")
 * @param[in] header  Parsed header fields data
 *
 * @return Status code
 */
static ib_status_t ib_auditlog_add_part_http_head_fields(
    ib_tx_t *tx,
    ib_mpool_t *mpool,
    ib_list_t *list,
    const char *label,
    ib_parsed_header_wrapper_t *header )
{
    IB_FTRACE_INIT();
    ib_parsed_name_value_pair_list_t *nvpair;
    ib_status_t rc;
    ib_field_t *f;

    /* Loop through all of the header name/value pairs */
    for (nvpair = header ->head;
         nvpair != NULL;
         nvpair = nvpair->next)
    {
        /* Create a field to hold the name/value pair. */
        rc = ib_field_create(&f, mpool,
                             (char *)ib_bytestr_const_ptr(nvpair->name),
                             ib_bytestr_length(nvpair->name),
                             IB_FTYPE_BYTESTR,
                             ib_ftype_bytestr_mutable_in(nvpair->value));
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Failed to create %s header field: %s",
                            label, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Add the new field to the list */
        rc = ib_list_push(list, f);
        if (rc != IB_OK) {
            ib_log_error_tx(tx,
                            "Failed to add %s field '%.*s': %s",
                            label,
                            (int) ib_bytestr_length(nvpair->name),
                            ib_bytestr_ptr(nvpair->name),
                            ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Add request header to the audit log
 *
 * @param[in,out] log Audit log to log to
 *
 * @return Status code
 */
static ib_status_t ib_auditlog_add_part_http_request_head(ib_auditlog_t *log)
{
    IB_FTRACE_INIT();
    ib_mpool_t *mpool = log->mp;
    ib_tx_t *tx = log->tx;
    ib_list_t *list;
    ib_field_t *f;
    ib_status_t rc;

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, mpool);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Add the raw request line */
    // FIXME: Why would this be NULL?  Should this ever happen?
    if (tx->request_line != NULL) {
        rc = ib_field_create(&f, mpool,
                             IB_FIELD_NAME("request_line"),
                             IB_FTYPE_BYTESTR,
                             tx->request_line->raw);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Failed to create request line field: %s",
                            ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        rc = ib_list_push(list, f);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Failed to add request line field: %s",
                            ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Add the request header fields */
    if (tx->request_header != NULL) {
        rc = ib_auditlog_add_part_http_head_fields(tx, mpool,
                                                   list, "request",
                                                   tx->request_header);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Add the part to the auditlog. */
    rc = ib_auditlog_part_add(log,
                              "http-request-header",
                              "application/octet-stream",
                              list,
                              ib_auditlog_gen_header_flist,
                              NULL);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_auditlog_add_part_http_request_body(ib_auditlog_t *log)
{
    IB_FTRACE_INIT();
    ib_tx_t *tx = log->tx;
    ib_status_t rc;

    rc = ib_auditlog_part_add(log,
                              "http-request-body",
                              "application/octet-stream",
                              tx->request_body,
                              ib_auditlog_gen_raw_stream,
                              NULL);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Add response header to the audit log
 *
 * @param[in,out] log Audit log to log to
 *
 * @return Status code
 */
static ib_status_t ib_auditlog_add_part_http_response_head(ib_auditlog_t *log)
{
    IB_FTRACE_INIT();
    ib_mpool_t *mpool = log->mp;
    ib_tx_t *tx = log->tx;
    ib_list_t *list;
    ib_field_t *f;
    ib_status_t rc;

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, mpool);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Add the raw response line
     *
     * The response_line may be NULL for HTTP/0.9 requests.
     */
    if (tx->response_line != NULL) {
        rc = ib_field_create(&f, mpool,
                             IB_FIELD_NAME("response_line"),
                             IB_FTYPE_BYTESTR,
                             tx->response_line->raw);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Failed to create response line field: %s",
                            ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
        rc = ib_list_push(list, f);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Failed to add response line field: %s",
                            ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Add the response header fields */
    if (tx->response_header != NULL) {
        rc = ib_auditlog_add_part_http_head_fields(tx, mpool,
                                                   list, "response",
                                                   tx->response_header);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Add the part to the auditlog. */
    rc = ib_auditlog_part_add(log,
                              "http-response-header",
                              "application/octet-stream",
                              list,
                              ib_auditlog_gen_header_flist,
                              NULL);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_auditlog_add_part_http_response_body(ib_auditlog_t *log)
{
    IB_FTRACE_INIT();
    ib_tx_t *tx = log->tx;
    ib_status_t rc;

    rc = ib_auditlog_part_add(log,
                              "http-response-body",
                              "application/octet-stream",
                              tx->response_body,
                              ib_auditlog_gen_raw_stream,
                              NULL);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Handle writing the logevents.
 *
 * @param ib Engine.
 * @param tx Transaction.
 * @param event Event type.
 * @param cbdata Callback data.
 *
 * @returns Status code.
 */
static ib_status_t logevent_hook_postprocess(ib_engine_t *ib,
                                             ib_tx_t *tx,
                                             ib_state_event_type_t event,
                                             void *cbdata)
{
    IB_FTRACE_INIT();

    assert(event == handle_postprocess_event);

    ib_auditlog_t *log;
    ib_core_cfg_t *corecfg;
    core_audit_cfg_t *cfg;
    ib_provider_inst_t *audit;
    ib_list_t *events;
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
            rc = ib_event_get_all(tx->epi, &events);
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

    /* Mark time. */
    tx->t.logtime = ib_clock_get_time();

    /* Auditing */
    /// @todo Only create if needed
    log = (ib_auditlog_t *)ib_mpool_calloc(tx->mp, 1, sizeof(*log));
    if (log == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

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
    if (corecfg->auditlog_parts & IB_ALPART_HTTP_REQUEST_HEADER) {
        ib_auditlog_add_part_http_request_head(log);
    }
    if (corecfg->auditlog_parts & IB_ALPART_HTTP_REQUEST_BODY) {
        ib_auditlog_add_part_http_request_body(log);
    }
    if (corecfg->auditlog_parts & IB_ALPART_HTTP_RESPONSE_HEADER) {
        ib_auditlog_add_part_http_response_head(log);
    }
    if (corecfg->auditlog_parts & IB_ALPART_HTTP_RESPONSE_BODY) {
        ib_auditlog_add_part_http_response_body(log);
    }

    /* Audit Log Provider Instance */
    rc = ib_provider_instance_create_ex(ib, corecfg->pr.audit, &audit,
                                        tx->mp, log);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx,
                        "Failed to create audit log provider instance: %s",
                        ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_auditlog_write(audit);

    /* Events */
    ib_event_write_all(tx->epi);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
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
    IB_FTRACE_INIT();
    IB_PROVIDER_IFACE_TYPE(logevent) *iface = (IB_PROVIDER_IFACE_TYPE(logevent) *)lpr->iface;

    /* Check that versions match. */
    if (iface->version != IB_PROVIDER_VERSION_LOGEVENT) {
        IB_FTRACE_RET_STATUS(IB_EINCOMPAT);
    }

    /* Verify that required interface functions are implemented. */
    if (iface->write == NULL) {
        ib_log_alert(ib, "The write function "
                     "MUST be implemented by a logevent provider");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
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
    IB_FTRACE_INIT();
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
 * Logevent provider API mapping for core module.
 */
static IB_PROVIDER_API_TYPE(logevent) logevent_api = {
    logevent_api_add_event,
    logevent_api_remove_event,
    logevent_api_fetch_events,
    logevent_api_write_events
};



/**
 * Handle the connection starting.
 *
 * Create the data provider instance and initialize the parser.
 *
 * @param ib Engine.
 * @param event Event type.
 * @param conn Connection.
 * @param cbdata Callback data.
 *
 * @returns Status code.
 */
static ib_status_t core_hook_conn_started(ib_engine_t *ib,
                                          ib_state_event_type_t event,
                                          ib_conn_t *conn,
                                          void *cbdata)
{
    IB_FTRACE_INIT();

    assert(event == conn_started_event);

    ib_core_cfg_t *corecfg;
    ib_status_t rc;

    rc = ib_context_module_config(conn->ctx, ib_core_module(),
                                  (void *)&corecfg);

    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to initialize core module: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Data Provider Instance */
    rc = ib_provider_instance_create_ex(ib, corecfg->pr.data, &conn->dpi,
                                        conn->mp, NULL);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to create conn data provider instance: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Parser Implementation -- */

/**
 * Parser provider registration function.
 *
 * This just does a version and sanity check on a registered provider.
 *
 * @param ib Engine
 * @param pr Logger provider
 *
 * @returns Status code
 */
static ib_status_t parser_register(ib_engine_t *ib,
                                   ib_provider_t *pr)
{
    IB_FTRACE_INIT();

    assert(pr != NULL);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = (IB_PROVIDER_IFACE_TYPE(parser) *)pr->iface;
    assert(iface != NULL);

    /* Check that versions match. */
    if (iface->version != IB_PROVIDER_VERSION_PARSER) {
        IB_FTRACE_RET_STATUS(IB_EINCOMPAT);
    }

    /* Verify that required interface functions are implemented. */
    if ((iface->conn_data_in == NULL) || (iface->conn_data_out == NULL)) {
        ib_log_alert(ib, "The data in/out and generate interface functions "
                            "MUST be implemented by a parser provider");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Data Implementation -- */

/**
 * Calls a registered provider interface to add a data field to a
 * provider instance.
 *
 * @param dpi Data provider instance
 * @param f Field to add
 * @param name Name of field
 * @param nlen Length of @a name
 *
 * @returns Status code
 */
static ib_status_t data_api_add(ib_provider_inst_t *dpi,
                                ib_field_t *f,
                                const char *name,
                                size_t nlen)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);

    IB_PROVIDER_IFACE_TYPE(data) *iface = (IB_PROVIDER_IFACE_TYPE(data) *)dpi->pr->iface;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_alert(dpi->pr->ib,  "Failed to fetch data interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->add(dpi, f, name, nlen);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Calls a registered provider interface to set a data field in a
 * provider instance.
 *
 * @param dpi Data provider instance
 * @param f Field to add
 * @param name Name of field
 * @param nlen Length of @a name
 *
 * @returns Status code
 */
static ib_status_t data_api_set(ib_provider_inst_t *dpi,
                                ib_field_t *f,
                                const char *name,
                                size_t nlen)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);

    IB_PROVIDER_IFACE_TYPE(data) *iface = (IB_PROVIDER_IFACE_TYPE(data) *)dpi->pr->iface;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_alert(dpi->pr->ib,  "Failed to fetch data interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->set(dpi, f, name, nlen);
    IB_FTRACE_RET_STATUS(rc);
}

/**
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
    IB_FTRACE_INIT();

    assert(dpi != NULL);

    IB_PROVIDER_IFACE_TYPE(data) *iface = (IB_PROVIDER_IFACE_TYPE(data) *)dpi->pr->iface;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_alert(dpi->pr->ib,  "Failed to fetch data interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->set_relative(dpi, name, nlen, adjval);
    IB_FTRACE_RET_STATUS(rc);
}

/**
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
static ib_status_t data_api_get(const ib_provider_inst_t *dpi,
                                const char *name,
                                size_t nlen,
                                ib_field_t **pf)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);

    IB_PROVIDER_IFACE_TYPE(data) *iface = (IB_PROVIDER_IFACE_TYPE(data) *)dpi->pr->iface;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_alert(dpi->pr->ib,  "Failed to fetch data interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->get(dpi, name, nlen, pf);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Calls a registered provider interface to get all data fields within a
 * provider instance.
 *
 * @param dpi Data provider instance
 * @param list List in which fields are pushed
 *
 * @returns Status code
 */
static ib_status_t data_api_get_all(const ib_provider_inst_t *dpi,
                                    ib_list_t *list)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);

    IB_PROVIDER_IFACE_TYPE(data) *iface = (IB_PROVIDER_IFACE_TYPE(data) *)dpi->pr->iface;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_alert(dpi->pr->ib,  "Failed to fetch data interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->get_all(dpi, list);
    IB_FTRACE_RET_STATUS(rc);
}

/**
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
    IB_FTRACE_INIT();

    assert(dpi != NULL);

    IB_PROVIDER_IFACE_TYPE(data) *iface = (IB_PROVIDER_IFACE_TYPE(data) *)dpi->pr->iface;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_alert(dpi->pr->ib,  "Failed to fetch data interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* This function is required, so no NULL check. */

    rc = iface->remove(dpi, name, nlen, pf);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Calls a registered provider interface to clear all fields from a
 * provider instance.
 *
 * @param dpi Data provider instance
 *
 * @returns Status code
 */
static ib_status_t data_api_clear(ib_provider_inst_t *dpi)
{
    IB_FTRACE_INIT();

    assert(dpi != NULL);
    assert(dpi->pr != NULL);

    IB_PROVIDER_IFACE_TYPE(data) *iface = (IB_PROVIDER_IFACE_TYPE(data) *)dpi->pr->iface;
    ib_status_t rc;

    /* This function is required, so no NULL check. */

    rc = iface->clear(dpi);
    IB_FTRACE_RET_STATUS(rc);
}

/**
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
 * Data access provider registration function.
 *
 * This just does a version and sanity check on a registered provider.
 *
 * @param ib Engine
 * @param pr Logger provider
 *
 * @returns Status code
 */
static ib_status_t data_register(ib_engine_t *ib,
                                 ib_provider_t *pr)
{
    IB_FTRACE_INIT();
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
        ib_log_alert(ib, "All required interface functions "
                            "MUST be implemented by a data provider");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
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
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_hash_t *ht;

    rc = ib_hash_create_nocase(&ht, dpi->mp);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    dpi->data = (void *)ht;

    ib_log_debug3(dpi->pr->ib, "Initialized core data provider instance: %p", dpi);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* -- Matcher Implementation -- */

/**
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
    IB_FTRACE_INIT();
    IB_PROVIDER_IFACE_TYPE(matcher) *iface = mpr?(IB_PROVIDER_IFACE_TYPE(matcher) *)mpr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_util_log_error("Failed to fetch matcher interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    if (iface->compile == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
    }

    rc = iface->compile(mpr, pool, pcpatt, patt, errptr, erroffset);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Match a compiled pattern against a buffer.
 *
 * @param mpr Matcher provider
 * @param cpatt Compiled pattern
 * @param flags Flags
 * @param data Data buffer to perform match on
 * @param dlen Data buffer length
 * @param ctx Context
 *
 * @returns Status code
 */
static ib_status_t matcher_api_match_compiled(ib_provider_t *mpr,
                                              void *cpatt,
                                              ib_flags_t flags,
                                              const uint8_t *data,
                                              size_t dlen,
                                              void *ctx)
{
    IB_FTRACE_INIT();
    IB_PROVIDER_IFACE_TYPE(matcher) *iface = mpr?(IB_PROVIDER_IFACE_TYPE(matcher) *)mpr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_util_log_error("Failed to fetch matcher interface");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    if (iface->match_compiled == NULL) {
        IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
    }

    rc = iface->match_compiled(mpr, cpatt, flags, data, dlen, ctx);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Add a pattern to a matcher provider instance.
 *
 * Multiple patterns can be added to a provider instance and all used
 * to perform a match later on.
 *
 * @todo Document parameters
 *
 * @returns Status code
 */
static ib_status_t matcher_api_add_pattern_ex(ib_provider_inst_t *mpi,
                                              void *patterns,
                                              const char *patt,
                                              ib_void_fn_t callback,
                                              void *arg,
                                              const char **errptr,
                                              int *erroffset)
{
    IB_FTRACE_INIT();

    assert(mpi != NULL);
    assert(mpi->pr != NULL);

    IB_PROVIDER_IFACE_TYPE(matcher) *iface = NULL;

    ib_status_t rc;
    iface = (IB_PROVIDER_IFACE_TYPE(matcher) *)mpi->pr->iface;

    rc = iface->add_ex(mpi, patterns, patt, callback, arg,
                               errptr, erroffset);
    if (rc != IB_OK) {
        ib_log_error(mpi->pr->ib,
                     "Failed to add pattern %s patt: (%s) %s at"
                     " offset %d", patt, ib_status_to_string(rc), *errptr,
                     *erroffset);
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
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
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

/**
 * Match all the provider instance patterns on a data field.
 *
 * @warning Not yet implemented
 *
 * @param mpi Matcher provider instance
 * @param flags Flags
 * @param data Data buffer
 * @param dlen Data buffer length
 * @param ctx Context
 *
 * @returns Status code
 */
static ib_status_t matcher_api_match(ib_provider_inst_t *mpi,
                                     ib_flags_t flags,
                                     const uint8_t *data,
                                     size_t dlen,
                                     void *ctx)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_ENOTIMPL);
}

/**
 * Matcher provider API mapping for core module.
 */
static IB_PROVIDER_API_TYPE(matcher) matcher_api = {
    matcher_api_compile_pattern,
    matcher_api_match_compiled,
    matcher_api_add_pattern,
    matcher_api_add_pattern_ex,
    matcher_api_match,
};

/**
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
    IB_FTRACE_INIT();
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
    IB_FTRACE_INIT();
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
 * Configure the filter controller.
 *
 * @param ib Engine.
 * @param tx Transaction.
 * @param event Event type.
 * @param cbdata Callback data.
 *
 * @returns Status code.
 */
static ib_status_t filter_ctl_config(ib_engine_t *ib,
                                     ib_tx_t *tx,
                                     ib_state_event_type_t event,
                                     void *cbdata)
{
    IB_FTRACE_INIT();
    assert(event == handle_context_tx_event);

    ib_status_t rc = IB_OK;

    /// @todo Need an API for this.
    tx->fctl->filters = tx->ctx->filters;
    tx->fctl->fbuffer = (ib_filter_t *)cbdata;
    ib_fctl_meta_add(tx->fctl, IB_STREAM_FLUSH);

    IB_FTRACE_RET_STATUS(rc);
}


/* -- Core Data Processors -- */

/**
 * Initialize the DPI in the given transaction.
 *
 * @param[in] ib IronBee object.
 * @param[in,out] tx The transaction whose tx->dpi will be populated wit
 *                default values.
 *
 * @returns IB_OK on success or the failure of ib_data_add_list(...).
 */
static ib_status_t dpi_default_init(ib_engine_t *ib, ib_tx_t *tx)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    assert(ib!=NULL);
    assert(tx!=NULL);
    assert(tx->dpi!=NULL);

    rc = ib_data_add_list_ex(tx->dpi, IB_TX_CAPTURE, 2, NULL);

    if (rc!=IB_OK) {
        ib_log_debug2_tx(tx, "Unable to add list \""IB_TX_CAPTURE"\".");
        IB_FTRACE_RET_STATUS(rc);
    }


    IB_FTRACE_RET_STATUS(rc);
}

/* -- Core Hook Handlers -- */

/**
 * Handle the transaction starting.
 *
 * Create the transaction provider instances.  And setup placeholders
 * for all of the core fields. This allows other modules to refer to
 * the field prior to it it being initialized.
 *
 * @param ib Engine.
 * @param tx Transaction.
 * @param event Event type.
 * @param cbdata Callback data.
 *
 * @returns Status code.
 */
static ib_status_t core_hook_tx_started(ib_engine_t *ib,
                                        ib_tx_t *tx,
                                        ib_state_event_type_t event,
                                        void *cbdata)
{
    IB_FTRACE_INIT();

    assert(event == tx_started_event);

    ib_core_cfg_t *corecfg;
    ib_status_t rc;

    rc = ib_context_module_config(tx->ctx, ib_core_module(),
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx,
                        "Failure accessing core module: %s",
                        ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Data Provider Instance */
    rc = ib_provider_instance_create_ex(ib, corecfg->pr.data, &tx->dpi,
                                        tx->mp, NULL);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx,
                        "Failed to create tx data provider instance: %s",
                        ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Data Provider Default Initialization */
    rc = dpi_default_init(ib, tx);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx, "Failed to initialize data provider instance.");
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Logevent Provider Instance */
    rc = ib_provider_instance_create_ex(ib, corecfg->pr.logevent, &tx->epi,
                                        tx->mp, NULL);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx, "Failed to create logevent provider instance: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t core_hook_request_body_data(ib_engine_t *ib,
                                               ib_tx_t *tx,
                                               ib_state_event_type_t event,
                                               ib_txdata_t *txdata,
                                               void *cbdata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);

    ib_core_cfg_t *corecfg;
    void *data_copy;
    ib_status_t rc;

    if (txdata == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Get the current context config. */
    rc = ib_context_module_config(tx->ctx, ib_core_module(), (void *)&corecfg);
    if (rc != IB_OK) {
        ib_log_alert(ib,
                     "Failed to fetch core module context config: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    if (! (corecfg->auditlog_parts & IB_ALPART_HTTP_REQUEST_BODY)) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    data_copy = ib_mpool_memdup(tx->mp, txdata->data, txdata->dlen);

    // TODO: Add a limit to this: size and type
    rc = ib_stream_push(tx->request_body,
                        IB_STREAM_DATA,
                        data_copy,
                        txdata->dlen);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t core_hook_response_body_data(ib_engine_t *ib,
                                                ib_tx_t *tx,
                                                ib_state_event_type_t event,
                                                ib_txdata_t *txdata,
                                                void *cbdata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);

    ib_core_cfg_t *corecfg;
    void *data_copy;
    ib_status_t rc;

    if (txdata == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Get the current context config. */
    rc = ib_context_module_config(tx->ctx, ib_core_module(), (void *)&corecfg);
    if (rc != IB_OK) {
        ib_log_alert(ib,
                     "Failed to fetch core module context config: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    if (! (corecfg->auditlog_parts & IB_ALPART_HTTP_RESPONSE_BODY)) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    data_copy = ib_mpool_memdup(tx->mp, txdata->data, txdata->dlen);

    // TODO: Add a limit to this: size and type
    rc = ib_stream_push(tx->response_body,
                        IB_STREAM_DATA,
                        data_copy,
                        txdata->dlen);

    IB_FTRACE_RET_STATUS(rc);
}


/* -- Directive Handlers -- */

/**
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
    IB_FTRACE_INIT();
    ib_mpool_t *pool = ib_engine_pool_config_get(ib);

    *pabsfile = (char *)ib_mpool_alloc(pool, strlen(basedir) + 1 + strlen(file) + 1);
    if (*pabsfile == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    strcpy(*pabsfile, basedir);
    strcat(*pabsfile, "/");
    strcat(*pabsfile, file);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
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
    IB_FTRACE_INIT();

    assert( cp != NULL );

    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;
    ib_site_t *site;
    ib_loc_t *loc;
    ib_status_t rc;
    char *p1_unescaped;

    assert( ib != NULL );
    assert( ib->mp != NULL );
    assert( name != NULL );
    assert( p1 != NULL );

    rc = core_unescape(ib, &p1_unescaped, p1);

    if ( rc != IB_OK ) {
        ib_log_debug2(ib, "Could not unescape configuration %s=%s", name, p1);
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug2(ib, "Creating site \"%s\"", p1_unescaped);
    rc = ib_site_create(&site, ib, p1_unescaped);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create site \"%s\": %s", p1_unescaped,
                     ib_status_to_string(rc));
    }

    ib_log_debug2(ib,
                 "Creating default location for site \"%s\"", p1_unescaped);
    rc = ib_site_loc_create_default(site, &loc);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Failed to create default location for site \"%s\": %s",
                     p1_unescaped,
                     ib_status_to_string(rc));
    }

    ib_log_debug2(ib,
                 "Creating context for \"%s:%s\"", p1_unescaped, loc->path);
    rc = ib_context_create(&ctx, ib, cp->cur_ctx,
                           "site", p1_unescaped,
                           ib_context_siteloc_chooser,
                           ib_context_site_lookup,
                           loc);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Failed to create context for \"%s:%s\": %s",
                     p1_unescaped,
                     loc->path,
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    ib_cfgparser_context_push(cp, ctx);

    ib_log_debug2(ib, "Opening context %p for \"%s\"", ctx, name);
    rc = ib_context_open(ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error opening context for \"%s\": %s",
                     name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Handle the end of a Site block.
 *
 * This function closes out the site and pops it from the parser stack.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_site_end(ib_cfgparser_t *cp,
                                     const char *name,
                                     void *cbdata)
{
    IB_FTRACE_INIT();

    assert( cp != NULL );
    assert( cp->ib != NULL );
    assert( name != NULL );

    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;
    ib_status_t rc;

    ib_log_debug2(ib, "Processing site block \"%s\"", name);

    /* Pop the current items off the stack */
    rc = ib_cfgparser_context_pop(cp, &ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to pop context for \"%s\": %s", name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug2(ib, "Closing context %p for \"%s\"", ctx, name);
    rc = ib_context_close(ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error closing context for \"%s\": %s",
                     name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
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
    IB_FTRACE_INIT();

    assert( cp != NULL );
    assert( cp->ib != NULL );

    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;
    ib_site_t *site = cp->cur_site;
    ib_loc_t *loc;
    ib_status_t rc;
    char *p1_unescaped;

    assert( site != NULL );
    assert( name != NULL );
    assert( p1 != NULL );

    rc = core_unescape(ib, &p1_unescaped, p1);
    if ( rc != IB_OK ) {
        ib_log_debug2(ib,
                     "Failed to unescape parameter %s=%s.", name, p1);
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug2(ib,
                 "Creating location \"%s\" for site \"%s\"",
                 p1_unescaped,
                 site->name);
    rc = ib_site_loc_create(site, &loc, p1_unescaped);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Failed to create location \"%s:%s\": %s",
                     site->name,
                     p1_unescaped,
                     ib_status_to_string(rc));
    }

    ib_log_debug2(ib,
                 "Creating context for \"%s:%s\"",
                 site->name,
                 loc->path);
    rc = ib_context_create(&ctx, ib, cp->cur_ctx,
                           "location", p1_unescaped,
                           ib_context_siteloc_chooser,
                           ib_context_site_lookup,
                           loc);
    if (rc != IB_OK) {
        ib_log_debug2(ib,
                     "Failed to create context for \"%s:%s\": %s",
                     site->name,
                     loc->path,
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    ib_cfgparser_context_push(cp, ctx);

    ib_log_debug2(ib, "Opening context %p for \"%s\"", ctx, name);
    rc = ib_context_open(ctx);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     "Error opening context for \"%s\": %s",
                     name,
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Handle the end of a Location block.
 *
 * This function closes out the location and pops it from the parser stack.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_loc_end(ib_cfgparser_t *cp,
                                    const char *name,
                                    void *cbdata)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;
    ib_status_t rc;

    ib_log_debug2(ib, "Processing location block \"%s\"", name);

    /* Pop the current items off the stack */
    rc = ib_cfgparser_context_pop(cp, &ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to pop context for \"%s\": %s", name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug2(ib, "Closing context %p for \"%s\"", ctx, name);
    rc = ib_context_close(ctx);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error closing context for \"%s\": %s",
                     name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
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
                                     const ib_list_t *args,
                                     void *cbdata)
{
    IB_FTRACE_INIT();

    assert( cp != NULL );
    assert( cp->ib != NULL );

    ib_engine_t *ib = cp->ib;
    const ib_list_node_t *node;
    ib_status_t rc = IB_EINVAL;

    assert( name != NULL );
    assert( args != NULL );

    IB_LIST_LOOP_CONST(args, node) {
        const char *p = (const char *)ib_list_node_data_const(node);
        char *p_unescaped;

        rc = core_unescape(ib, &p_unescaped, p);

        if ( rc != IB_OK ) {
            ib_log_debug(ib, "Failed to unescape %s=%s", name, p);
            IB_FTRACE_RET_STATUS(rc);
        }

        if (strncasecmp("ip=", p_unescaped, 3) == 0) {
            p_unescaped += 3; /* Skip over ip= */
            ib_log_debug2(ib, "Adding IP \"%s\" to site \"%s\"",
                         p_unescaped, cp->cur_site->name);
            rc = ib_site_address_add(cp->cur_site, p_unescaped);
        }
        else if (strncasecmp("path=", p_unescaped, 5) == 0) {
            //p_unescaped += 5; /* Skip over path= */
            ib_log_debug(ib, "TODO: Handle: %s %s", name, p_unescaped);
        }
        else if (strncasecmp("port=", p_unescaped, 5) == 0) {
            //p_unescaped += 5; /* Skip over port= */
            ib_log_debug(ib, "TODO: Handle: %s %s", name, p_unescaped);
        }
        else {
            /// @todo Handle full wildcards
            if (*p_unescaped == '*') {
                /* Currently we do a match on the end of the host, so
                 * just skipping over the wildcard (assuming only one)
                 * for now.
                 */
                p_unescaped++;
            }
            ib_log_debug2(ib, "Adding host \"%s\" to site \"%s\"",
                         p_unescaped, cp->cur_site->name);
            rc = ib_site_hostname_add(cp->cur_site, p_unescaped);
        }
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
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
    IB_FTRACE_INIT();

    assert( cp != NULL );
    assert( cp->ib != NULL );

    ib_engine_t *ib = cp->ib;
    ib_status_t rc;
    ib_core_cfg_t *corecfg;
    const char *p1_unescaped;
    ib_context_t *ctx;

    assert( name != NULL );
    assert( p1 != NULL );

    ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);

    /* We remove constness to populate this buffer. */
    rc = core_unescape(ib, (char**)&p1_unescaped, p1);
    if ( rc != IB_OK ) {
        ib_log_debug2(ib, "Failed to unescape %s=%s", name, p1);
        IB_FTRACE_RET_STATUS(rc);
    }

    if (strcasecmp("InspectionEngine", name) == 0) {
        ib_log_debug(ib,
                    "TODO: Handle Directive: %s \"%s\"", name, p1_unescaped);
    }
    else if (strcasecmp("AuditEngine", name) == 0) {
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);
        if (strcasecmp("RelevantOnly", p1_unescaped) == 0) {
            rc = ib_context_set_num(ctx, "audit_engine", 2);
            IB_FTRACE_RET_STATUS(rc);
        }
        else if (strcasecmp("On", p1_unescaped) == 0) {
            rc = ib_context_set_num(ctx, "audit_engine", 1);
            IB_FTRACE_RET_STATUS(rc);
        }
        else if (strcasecmp("Off", p1_unescaped) == 0) {
            rc = ib_context_set_num(ctx, "audit_engine", 0);
            IB_FTRACE_RET_STATUS(rc);
        }

        ib_log_error(ib,
                     "Failed to parse directive: %s \"%s\"",
                     name,
                     p1_unescaped);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    else if (strcasecmp("AuditLogIndex", name) == 0) {
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);

        /* "None" means do not use the index file at all. */
        if (strcasecmp("None", p1_unescaped) == 0) {
            rc = ib_context_set_auditlog_index(ctx, NULL);
            IB_FTRACE_RET_STATUS(rc);
        }

        rc = ib_context_set_auditlog_index(ctx, p1_unescaped);

        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("AuditLogIndexFormat", name) == 0) {
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);
        rc = ib_context_set_string(ctx, "auditlog_index_fmt", p1_unescaped);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("AuditLogDirMode", name) == 0) {
        long lmode = strtol(p1_unescaped, NULL, 0);

        if ((lmode > 0777) || (lmode <= 0)) {
            ib_log_error(ib, "Invalid mode: %s \"%s\"", name, p1_unescaped);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);
        rc = ib_context_set_num(ctx, "auditlog_dmode", lmode);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("AuditLogFileMode", name) == 0) {
        long lmode = strtol(p1_unescaped, NULL, 0);

        if ((lmode > 0777) || (lmode <= 0)) {
            ib_log_error(ib, "Invalid mode: %s \"%s\"", name, p1_unescaped);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);
        rc = ib_context_set_num(ctx, "auditlog_fmode", lmode);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("AuditLogBaseDir", name) == 0) {
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);
        rc = ib_context_set_string(ctx, "auditlog_dir", p1_unescaped);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("AuditLogSubDirFormat", name) == 0) {
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);
        rc = ib_context_set_string(ctx, "auditlog_sdir_fmt", p1_unescaped);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if ( (strcasecmp("DebugLogLevel", name) == 0) ||
              (strcasecmp("LogLevel", name) == 0) )
    {
        int num_read = 0;
        long level = 0;
        num_read = sscanf(p1_unescaped, "%ld", &level);
        if (num_read == 0) {
            level = ib_log_string_to_level(p1_unescaped);
            if (level > IB_LOG_TRACE) {
                IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
            }
        }
        ib_log_debug2(ib, "%s: %ld", name, level);
        rc = ib_context_set_num(ctx, "logger.log_level", level);
        IB_FTRACE_RET_STATUS(rc);
    }
    /* Set the default block status for responding to blocked transactions. */
    else if (strcasecmp("DefaultBlockStatus", name) == 0) {
        int status;

        rc = ib_context_module_config(ctx, ib_core_module(), (void *)&corecfg);

        if (rc != IB_OK) {
            ib_log_error(ib,
                         "Could not set DefaultBlockStatus %s",
                         p1_unescaped);
            IB_FTRACE_RET_STATUS(rc);
        }

        status  = atoi(p1);

        if (!(status <= 200 && status < 600))
        {
            ib_log_debug2(ib,
                          "DefaultBlockStatus must be 200 <= status < 600.");
            ib_log_debug2(ib, "DefaultBlockStatus may not be %d", status);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        corecfg->block_status = status;
        ib_log_debug2(ib, "DefaultBlockStatus: %d", status);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if ( (strcasecmp("DebugLog", name) == 0) ||
              (strcasecmp("Log", name) == 0) )
    {
        ib_mpool_t   *mp  = ib_engine_pool_main_get(ib);
        const char   *uri = NULL;

        ib_log_debug2(ib, "%s: \"%s\"", name, p1_unescaped);

        /* Create a file URI from the file path, using memory
         * from the context's mem pool. */
        if ( strstr(p1_unescaped, "://") == NULL )  {
            char *buf = (char *)ib_mpool_alloc( mp, 8+strlen(p1_unescaped) );
            strcpy( buf, "file://" );
            strcat( buf, p1_unescaped );
            uri = buf;
        }
        else if ( strncmp(p1_unescaped, "file://", 7) != 0 ) {
            ib_log_error(ib,
                         "Unsupported URI in %s: \"%s\"",
                         name, p1_unescaped);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        else {
            uri = p1_unescaped;
        }
        ib_log_debug2(ib, "%s: URI=\"%s\"", name, uri);
        rc = ib_context_set_string(ctx, "logger.log_uri", uri);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if ( (strcasecmp("DebugLogHandler", name) == 0) ||
              (strcasecmp("LogHandler", name) == 0) )
    {
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);
        rc = ib_context_set_string(ctx, "logger.log_handler", p1_unescaped);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("RuleEngineLogLevel", name) == 0) {
        ib_rule_log_level_t  level = IB_RULE_LOG_LEVEL_ERROR;
        char                *p1_copy = ib_mpool_strdup(cp->mp, p1_unescaped);
        char                *cur;

        if (p1_copy == NULL) {
            ib_cfg_log_error(cp, "Error copying \"%s\" for \"%s\"",
                             p1_unescaped, name);
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        cur = strtok(p1_copy, ",");
        do {
            if (strcasecmp("Off", cur) == 0) {
                level = IB_RULE_LOG_LEVEL_ERROR;
                break;
            }
            else if (strcasecmp("Error", cur) == 0) {
                level = IB_RULE_LOG_LEVEL_ERROR;
            }
            else if (strcasecmp("Warning", cur) == 0) {
                level = IB_RULE_LOG_LEVEL_WARNING;
            }
            else if (strcasecmp("Debug", cur) == 0) {
                level = IB_RULE_LOG_LEVEL_DEBUG;
            }
            else if (strcasecmp("Trace", cur) == 0) {
                level = IB_RULE_LOG_LEVEL_TRACE;
            }
            else {
                ib_log_error(ib,
                             "Invalid value for %s: \"%s\"",
                             name, cur);
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }
            cur = strtok(NULL, ",");
        } while (cur != NULL);
        ib_log_debug2(ib, "%s: %d", name, level);
        rc = ib_context_set_num(ctx, "rule_log_level", level);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("LoadModule", name) == 0) {
        char *absfile;
        ib_module_t *m;

        if (*p1_unescaped == '/') {
            absfile = (char *)p1_unescaped;
        }
        else {
            rc = ib_context_module_config(ctx,
                                          ib_core_module(),
                                          (void *)&corecfg);

            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }

            rc = core_abs_module_path(ib,
                                      corecfg->module_base_path,
                                      p1_unescaped, &absfile);

            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }
        }

        rc = ib_module_load(&m, ib, absfile);
        /* ib_module_load will report errors. */
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("RequestBuffering", name) == 0) {
        ib_log_debug2(ib, "%s: %s", name, p1_unescaped);
        if (strcasecmp("On", p1_unescaped) == 0) {
            rc = ib_context_set_num(ctx, "buffer_req", 1);
            IB_FTRACE_RET_STATUS(rc);
        }

        rc = ib_context_set_num(ctx, "buffer_req", 0);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("ResponseBuffering", name) == 0) {
        ib_log_debug2(ib, "%s: %s", name, p1_unescaped);
        if (strcasecmp("On", p1_unescaped) == 0) {
            rc = ib_context_set_num(ctx, "buffer_res", 1);
            IB_FTRACE_RET_STATUS(rc);
        }

        rc = ib_context_set_num(ctx, "buffer_res", 0);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("SensorId", name) == 0) {
        union {
            uint64_t uint64;
            uint32_t uint32[2];
        } reduce;

        /* Store the ASCII version for logging */
        ib->sensor_id_str = ib_mpool_strdup(ib_engine_pool_config_get(ib),
                                            p1_unescaped);

        /* Calculate the binary version. */
        rc = ib_uuid_ascii_to_bin(&ib->sensor_id, (const char *)p1_unescaped);
        if (rc != IB_OK) {
            ib_log_error(ib, "Invalid UUID at %s: %s should have "
                            "UUID format "
                            "(xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx where x are"
                            " hex values)",
                            name, p1_unescaped);

            /* Use the default id. */
            ib->sensor_id_str = (const char *)ib_uuid_default_str;
            rc = ib_uuid_ascii_to_bin(&ib->sensor_id, ib_uuid_default_str);

            IB_FTRACE_RET_STATUS(rc);
        }

        ib_log_debug2(ib, "%s: %s", name, ib->sensor_id_str);

        /* Generate a 4byte hash id to use it for transaction id generations */
        reduce.uint64 = ib->sensor_id.uint64[0] ^
                        ib->sensor_id.uint64[1];

        ib->sensor_id_hash = reduce.uint32[0] ^
                             reduce.uint32[1];

        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (strcasecmp("SensorName", name) == 0) {
        ib->sensor_name = ib_mpool_strdup(ib_engine_pool_config_get(ib),
                                          p1_unescaped);
        ib_log_debug2(ib, "%s: %s", name, ib->sensor_name);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (strcasecmp("SensorHostname", name) == 0) {
        ib->sensor_hostname =
            ib_mpool_strdup(ib_engine_pool_config_get(ib), p1_unescaped);
        ib_log_debug2(ib, "%s: %s", name, ib->sensor_hostname);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (strcasecmp("SiteId", name) == 0) {
        ib_site_t *site = cp->cur_site;

        /* Store the ASCII version for logging */
        site->id_str = ib_mpool_strdup(ib_engine_pool_config_get(ib),
                                       p1_unescaped);

        /* Calculate the binary version. */
        rc = ib_uuid_ascii_to_bin(&site->id, (const char *)p1_unescaped);
        if (rc != IB_OK) {
            ib_log_error(ib, "Invalid UUID at %s: %s should have "
                            "UUID format "
                            "(xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx where x are"
                            " hex values)",
                            name, p1_unescaped);

            /* Use the default id. */
            site->id_str = (const char *)ib_uuid_default_str;
            rc = ib_uuid_ascii_to_bin(&site->id, ib_uuid_default_str);

            IB_FTRACE_RET_STATUS(rc);
        }

        ib_log_debug2(ib, "%s: %s", name, site->id_str);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (strcasecmp("ModuleBasePath", name) == 0) {
        rc = ib_context_module_config(ctx, ib_core_module(), (void *)&corecfg);

        if (rc != IB_OK) {
            ib_log_error(ib,
                         "Could not set ModuleBasePath %s", p1_unescaped);
            IB_FTRACE_RET_STATUS(rc);
        }

        corecfg->module_base_path = p1_unescaped;
        ib_log_debug2(ib, "ModuleBasePath: %s", p1_unescaped);
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (strcasecmp("RuleBasePath", name) == 0) {
        rc = ib_context_module_config(ctx, ib_core_module(), (void *)&corecfg);

        if (rc != IB_OK) {
            ib_log_error(ib, "Could not set RuleBasePath %s", p1_unescaped);
            IB_FTRACE_RET_STATUS(rc);
        }

        corecfg->rule_base_path = p1_unescaped;
        ib_log_debug2(ib, "RuleBasePath: %s", p1_unescaped);
        IB_FTRACE_RET_STATUS(IB_OK);

    }

    ib_log_error(ib, "Unhandled directive: %s %s", name, p1_unescaped);
    IB_FTRACE_RET_STATUS(IB_EINVAL);
}

/**
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
    IB_FTRACE_INIT();
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
    ib_num_t parts;
    ib_status_t rc;

    rc = ib_context_get(ctx, "auditlog_parts", ib_ftype_num_out(&parts), NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Merge the set flags with the previous value. */
    parts = (flags & fmask) | (parts & ~fmask);

    ib_log_debug2(ib, "AUDITLOG PARTS: 0x%08lu", (unsigned long)parts);

    rc = ib_context_set_num(ctx, "auditlog_parts", parts);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Handle rule log data directive.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param vars Arguments to directive.
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_rulelogdata(ib_cfgparser_t *cp,
                                        const char *name,
                                        const ib_list_t *vars,
                                        void *cbdata)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
    const ib_list_node_t *node;
    ib_rule_log_mode_t log_mode;
    ib_flags_t log_flags;
    const char *modestr;
    ib_status_t rc = IB_OK;
    ib_num_t tmp;
    bool first = true;

    if (cbdata != NULL) {
        IB_FTRACE_MSG("Callback data is not null.");
    }

    /* Get current rule logging type */
    rc = ib_context_get(ctx, "rule_log_mode", ib_ftype_num_out(&tmp), NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    log_mode = (ib_rule_log_mode_t)tmp;

    /* Get current rule logging flags */
    rc = ib_context_get(ctx, "rule_log_flags", ib_ftype_num_out(&tmp), NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }
    log_flags = (ib_flags_t)tmp;

    /* Loop through all of the parameters in the list */
    IB_LIST_LOOP_CONST(vars, node) {
        const char *param = (const char *)node->data;
        char modifier = '\0';
        const char *pname = param;

        if ( (*pname == '+') || (*pname == '-') ) {
            modifier = *pname;
            ++pname;
        }

        if ((first == true) && (strcasecmp(param, "None") == 0)) {
            log_mode = IB_RULE_LOG_MODE_OFF;
        }
        else if ((first == true) && (strcasecmp(param, "Fast") == 0)) {
            log_mode = IB_RULE_LOG_MODE_FAST;
        }
        else if ((first == true) && (strcasecmp(param, "RuleExec") == 0)) {
            log_mode = IB_RULE_LOG_MODE_EXEC;
            ib_flags_set(log_flags, IB_RULE_LOG_FLAG_FULL);
        }
        else if (strcasecmp(pname, "Full") == 0) {
            if (modifier == '-') {
                ib_flags_clear(log_flags, IB_RULE_LOG_FLAG_FULL);
            }
            else {
                ib_flags_set(log_flags, IB_RULE_LOG_FLAG_FULL);
            }
        }
        else if (strcasecmp(pname, "Debug") == 0) {
            if (modifier == '-') {
                ib_flags_clear(log_flags, IB_RULE_LOG_FLAG_DEBUG);
            }
            else {
                ib_flags_set(log_flags, IB_RULE_LOG_FLAG_DEBUG);
            }
        }
        else if (strcasecmp(pname, "Trace") == 0) {
            if (modifier == '-') {
                ib_flags_clear(log_flags, IB_RULE_LOG_FLAG_TRACE);
            }
            else {
                ib_flags_set(log_flags, IB_RULE_LOG_FLAG_TRACE);
            }
        }
        else {
            ib_cfg_log_error(cp, "Invalid %s parameter \"%s\"", name, param);
            rc = IB_EINVAL;
            continue;
        }
        first = false;
    }

    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Get mode as a string */
    modestr = ib_rule_log_mode_str(log_mode);
    ib_log_debug2(ib, "Rule Log Mode: %s", modestr);
    ib_log_debug2(ib, "Rule Log flags: %02x", log_flags);

    rc = ib_context_set_num(ctx, "rule_log_mode", log_mode);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error setting log mode to %s: %s",
                         modestr, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    rc = ib_context_set_num(ctx, "rule_log_flags", log_flags);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error setting log flags to %02x: %s",
                         log_flags, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Perform any extra duties when certain config parameters are "Set".
 *
 * @param ctx Context
 * @param type Config parameter type
 * @param name Config parameter name
 * @param val Config parameter value
 *
 * @returns Status code
 */
static ib_status_t core_set_value(ib_context_t *ctx,
                                  ib_ftype_t type,
                                  const char *name,
                                  const char *val)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib = ctx->ib;
    ib_core_cfg_t *corecfg;
    ib_status_t rc;

    /* Get the core module config. */
    rc = ib_context_module_config(ib->ctx, ib_core_module(),
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        corecfg = &core_global_cfg;
    }

    if (strcasecmp("parser", name) == 0) {
        ib_provider_inst_t *pi;

        if (strcmp(MODULE_NAME_STR, corecfg->parser) == 0) {
            IB_FTRACE_RET_STATUS(IB_OK);
        }
        /* Lookup/set parser provider instance. */
        rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_PARSER,
                                         val, &pi,
                                         ib->mp, NULL);
        if (rc != IB_OK) {
            ib_log_alert(ib, "Failed to create %s provider instance: %s",
                         IB_PROVIDER_TYPE_PARSER, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }

        rc = ib_parser_provider_set_instance(ctx, pi);
        if (rc != IB_OK) {
            ib_log_alert(ib, "Failed to set %s provider instance: %s",
                         IB_PROVIDER_TYPE_PARSER, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else if (strcasecmp("audit", name) == 0) {
        /* Lookup the audit log provider. */
        rc = ib_provider_lookup(ib,
                                IB_PROVIDER_TYPE_AUDIT,
                                val,
                                &corecfg->pr.audit);
        if (rc != IB_OK) {
            ib_log_alert(ib, "Failed to lookup %s audit log provider: %s",
                         val, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else if (strcasecmp("data", name) == 0) {
        /* Lookup the data provider. */
        rc = ib_provider_lookup(ib,
                                IB_PROVIDER_TYPE_DATA,
                                val,
                                &corecfg->pr.data);
        if (rc != IB_OK) {
            ib_log_alert(ib, "Failed to lookup %s data provider: %s",
                         val, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else if (strcasecmp("logevent", name) == 0) {
        /* Lookup the logevent provider. */
        rc = ib_provider_lookup(ib,
                                IB_PROVIDER_TYPE_LOGEVENT,
                                val,
                                &corecfg->pr.logevent);
        if (rc != IB_OK) {
            ib_log_alert(ib, "Failed to lookup %s logevent provider: %s",
                         val, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
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
    IB_FTRACE_INIT();
    ib_engine_t *ib = cp->ib;
    ib_status_t rc;

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
                ib_log_error(ib,
                             "Can only set string(%d) or numeric(%d) "
                             "types, but %s was type=%d",
                             IB_FTYPE_NULSTR, IB_FTYPE_NUM,
                             p1, type);
                IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        rc = core_set_value(ctx, type, p1, p2);
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_error(ib, "Unhandled directive: %s %s %s", name, p1, p2);
    IB_FTRACE_RET_STATUS(IB_EINVAL);
}


/**
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
    IB_STRVAL_PAIR("requestheader", IB_ALPART_HTTP_REQUEST_HEADER),
    IB_STRVAL_PAIR("requestbody", IB_ALPART_HTTP_REQUEST_BODY),
    IB_STRVAL_PAIR("requesttrailer", IB_ALPART_HTTP_REQUEST_TRAILER),
    IB_STRVAL_PAIR("responsemetadata", IB_ALPART_HTTP_RESPONSE_METADATA),
    IB_STRVAL_PAIR("responseheader", IB_ALPART_HTTP_RESPONSE_HEADER),
    IB_STRVAL_PAIR("responsebody", IB_ALPART_HTTP_RESPONSE_BODY),
    IB_STRVAL_PAIR("responsetrailer", IB_ALPART_HTTP_RESPONSE_TRAILER),
    IB_STRVAL_PAIR("debugfields", IB_ALPART_DEBUG_FIELDS),

    /* End */
    IB_STRVAL_PAIR_LAST
};

/**
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

    /* Blocking */
    IB_DIRMAP_INIT_PARAM1(
        "DefaultBlockStatus",
        core_dir_param1,
        NULL
    ),

    /* Logging */
    IB_DIRMAP_INIT_PARAM1(
        "DebugLogLevel",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "DebugLog",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "DebugLogHandler",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "LogLevel",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "Log",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "LogHandler",
        core_dir_param1,
        NULL
    ),

    /* Config */
    IB_DIRMAP_INIT_SBLK1(
        "Site",
        core_dir_site_start,
        core_dir_site_end,
        NULL,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "SiteId",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_SBLK1(
        "Location",
        core_dir_loc_start,
        core_dir_loc_end,
        NULL,
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
        "AuditLogIndexFormat",
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

    /* Search Paths - Modules */
    IB_DIRMAP_INIT_PARAM1(
        "ModuleBasePath",
        core_dir_param1,
        NULL
    ),

    /* Search Paths - Rules */
    IB_DIRMAP_INIT_PARAM1(
        "RuleBasePath",
        core_dir_param1,
        NULL
    ),

    /* Rule logging level */
    IB_DIRMAP_INIT_LIST(
        "RuleEngineLogData",
        core_dir_rulelogdata,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "RuleEngineLogLevel",
        core_dir_param1,
        NULL
    ),

    /* End */
    IB_DIRMAP_INIT_LAST
};


/* -- Module Routines -- */

/**
 * Logger for util logger.
 **/
static void core_util_logger(
    void *ib, int level,
    const char *file, int line,
    const char *fmt, va_list ap
)
{
    IB_FTRACE_INIT();

    ib_vlog_ex((ib_engine_t *)ib, level, file, line, fmt, ap);

    IB_FTRACE_RET_VOID();
}

/**
 * Initialize the core module on load.
 *
 * @param[in] ib Engine
 * @param[in] m Module
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t core_init(ib_engine_t *ib,
                             ib_module_t *m,
                             void        *cbdata)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg;
    ib_provider_t *core_log_provider;
    ib_provider_t *core_audit_provider;
    ib_provider_t *core_data_provider;
    ib_provider_inst_t *logger;
    ib_provider_inst_t *parser;
    ib_filter_t *fbuffer;
    ib_status_t rc;

    /* Get the core module config. */
    rc = ib_context_module_config(ib->ctx, m,
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch core module config: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Set defaults */
    corecfg->log_level          = 4;
    corecfg->log_uri            = "";
    corecfg->log_handler        = MODULE_NAME_STR;
    corecfg->logevent           = MODULE_NAME_STR;
    corecfg->parser             = MODULE_NAME_STR;
    corecfg->buffer_req         = 0;
    corecfg->buffer_res         = 0;
    corecfg->audit_engine       = 0;
    corecfg->auditlog_dmode     = 0700;
    corecfg->auditlog_fmode     = 0600;
    corecfg->auditlog_parts     = IB_ALPARTS_DEFAULT;
    corecfg->auditlog_dir       = "/var/log/ironbee";
    corecfg->auditlog_sdir_fmt  = "";
    corecfg->auditlog_index_fmt = IB_LOGFORMAT_DEFAULT;
    corecfg->audit              = MODULE_NAME_STR;
    corecfg->data               = MODULE_NAME_STR;
    corecfg->module_base_path   = X_MODULE_BASE_PATH;
    corecfg->rule_base_path     = X_RULE_BASE_PATH;
    corecfg->rule_log_mode      = IB_RULE_LOG_MODE_OFF;
    corecfg->rule_log_flags     = IB_RULE_LOG_FLAG_NONE;
    corecfg->rule_log_level     = IB_RULE_LOG_LEVEL_ERROR;
    corecfg->block_status       = 403;

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

    /* Force any IBUtil calls to use the default logger */
    rc = ib_util_log_logger(core_util_logger, ib);
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
        ib_log_alert(ib, "Failed to define parser provider: %s", ib_status_to_string(rc));
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
        ib_log_alert(ib, "Failed to register buffer filter: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_hook_tx_register(ib, handle_context_tx_event,
                        filter_ctl_config, fbuffer);

    /* Register hooks. */
    ib_hook_conn_register(ib, conn_started_event,
                          core_hook_conn_started, NULL);
    ib_hook_tx_register(ib, tx_started_event,
                        core_hook_tx_started, NULL);
    /*
     * @todo Need the parser to parse the header before context, but others after
     * context so that the personality can change based on the header (Host, uri
     * path, etc)
     */
    /*
     * ib_hook_register(ib, handle_context_tx_event, (void *)parser_hook_req_header,NULL);
     */

    /* Register auditlog body buffering hooks. */
    ib_hook_txdata_register(ib, request_body_data_event,
                            core_hook_request_body_data, NULL);

    ib_hook_txdata_register(ib, response_body_data_event,
                            core_hook_response_body_data, NULL);

    /* Register logevent hooks. */
    ib_hook_tx_register(ib, handle_postprocess_event,
                        logevent_hook_postprocess, NULL);

    /* Define the data field provider API */
    rc = ib_provider_define(ib, IB_PROVIDER_TYPE_DATA,
                            data_register, &data_api);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to define data provider: %s", ib_status_to_string(rc));
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
        ib_log_alert(ib, "Failed to define matcher provider: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Lookup/set default logger provider. */
    rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_LOGGER,
                                     corecfg->log_handler, &logger,
                                     ib->mp, NULL);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to create %s provider instance '%s': %s",
                     IB_PROVIDER_TYPE_LOGGER, corecfg->log_handler, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_log_provider_set_instance(ib->ctx, logger);

    /* Lookup the core data provider. */
    rc = ib_provider_lookup(ib,
                            IB_PROVIDER_TYPE_DATA,
                            IB_DSTR_CORE,
                            &corecfg->pr.data);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to lookup %s data provider: %s",
                     IB_DSTR_CORE, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Lookup the core audit log provider. */
    rc = ib_provider_lookup(ib,
                            IB_PROVIDER_TYPE_AUDIT,
                            IB_DSTR_CORE,
                            &corecfg->pr.audit);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to lookup %s audit log provider: %s",
                     IB_DSTR_CORE, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Lookup the core logevent provider. */
    rc = ib_provider_lookup(ib,
                            IB_PROVIDER_TYPE_LOGEVENT,
                            IB_DSTR_CORE,
                            &corecfg->pr.logevent);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to lookup %s logevent provider: %s",
                     IB_DSTR_CORE, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Lookup/set default parser provider if not the "core" parser. */
    if (strcmp(MODULE_NAME_STR, corecfg->parser) != 0) {
        rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_PARSER,
                                         corecfg->parser, &parser,
                                         ib->mp, NULL);
        if (rc != IB_OK) {
            ib_log_alert(ib, "Failed to create %s provider instance: %s",
                         IB_DSTR_CORE, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
        ib_parser_provider_set_instance(ib->ctx, parser);
    }

    /* Initialize the core fields */
    rc = ib_core_fields_init(ib, m);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to initialize core fields: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Initialize the core rule engine */
    rc = ib_rule_engine_init(ib, m);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to initialize rule engine: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Initialize the core transformations */
    rc = ib_core_transformations_init(ib, m);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to initialize core operators: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Initialize the core operators */
    rc = ib_core_operators_init(ib, m);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to initialize core operators: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Initialize the core actions */
    rc = ib_core_actions_init(ib, m);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to initialize core actions: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Core module configuration parameter initialization structure.
 */
static IB_CFGMAP_INIT_STRUCTURE(core_config_map) = {
    /* Logger */
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_LOGGER,
        IB_FTYPE_NULSTR,
        ib_core_cfg_t,
        log_handler
    ),
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_LOGGER ".log_level",
        IB_FTYPE_NUM,
        ib_core_cfg_t,
        log_level
    ),
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_LOGGER ".log_uri",
        IB_FTYPE_NULSTR,
        ib_core_cfg_t,
        log_uri
    ),
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_LOGGER ".log_handler",
        IB_FTYPE_NULSTR,
        ib_core_cfg_t,
        log_handler
    ),

    /* Logevent */
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_LOGEVENT,
        IB_FTYPE_NULSTR,
        ib_core_cfg_t,
        logevent
    ),

    /* Rule logging */
    IB_CFGMAP_INIT_ENTRY(
        "rule_log_mode",
        IB_FTYPE_NUM,
        ib_core_cfg_t,
        rule_log_mode
    ),
    IB_CFGMAP_INIT_ENTRY(
        "rule_log_flags",
        IB_FTYPE_NUM,
        ib_core_cfg_t,
        rule_log_flags
    ),
    IB_CFGMAP_INIT_ENTRY(
        "rule_log_level",
        IB_FTYPE_NUM,
        ib_core_cfg_t,
        rule_log_level
    ),

    /* Parser */
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_PARSER,
        IB_FTYPE_NULSTR,
        ib_core_cfg_t,
        parser
    ),

    /* Buffering */
    IB_CFGMAP_INIT_ENTRY(
        "buffer_req",
        IB_FTYPE_NUM,
        ib_core_cfg_t,
        buffer_req
    ),
    IB_CFGMAP_INIT_ENTRY(
        "buffer_res",
        IB_FTYPE_NUM,
        ib_core_cfg_t,
        buffer_res
    ),

    /* Audit Log */
    IB_CFGMAP_INIT_ENTRY(
        "audit_engine",
        IB_FTYPE_NUM,
        ib_core_cfg_t,
        audit_engine
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog_dmode",
        IB_FTYPE_NUM,
        ib_core_cfg_t,
        auditlog_dmode
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog_fmode",
        IB_FTYPE_NUM,
        ib_core_cfg_t,
        auditlog_fmode
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog_parts",
        IB_FTYPE_NUM,
        ib_core_cfg_t,
        auditlog_parts
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog_dir",
        IB_FTYPE_NULSTR,
        ib_core_cfg_t,
        auditlog_dir
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog_sdir_fmt",
        IB_FTYPE_NULSTR,
        ib_core_cfg_t,
        auditlog_sdir_fmt
    ),
    IB_CFGMAP_INIT_ENTRY(
        "auditlog_index_fmt",
        IB_FTYPE_NULSTR,
        ib_core_cfg_t,
        auditlog_index_fmt
    ),
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_AUDIT,
        IB_FTYPE_NULSTR,
        ib_core_cfg_t,
        audit
    ),

    /* Data Acquisition */
    IB_CFGMAP_INIT_ENTRY(
        IB_PROVIDER_TYPE_DATA,
        IB_FTYPE_NULSTR,
        ib_core_cfg_t,
        data
    ),

    /* End */
    IB_CFGMAP_INIT_LAST
};

ib_module_t *ib_core_module(void)
{
    return IB_MODULE_STRUCT_PTR;
}

/**
 * Initialize the core module context
 *
 * @param ib Engine
 * @param mod Module
 * @param ctx Context
 * @param cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t core_ctx_open(ib_engine_t  *ib,
                                 ib_module_t  *mod,
                                 ib_context_t *ctx,
                                 void         *cbdata)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Initialize the core fields context. */
    rc = ib_core_fields_ctx_init(ib, mod, ctx, cbdata);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to initialize core fields: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Initialize the rule engine for the context */
    rc = ib_rule_engine_ctx_init(ib, mod, ctx);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to initialize rule engine context: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Initialize the core module context
 *
 * @param ib Engine
 * @param mod Module
 * @param ctx Context
 * @param cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t core_ctx_close(ib_engine_t  *ib,
                                  ib_module_t  *mod,
                                  ib_context_t *ctx,
                                  void         *cbdata)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg;
    ib_provider_t *lp;
    ib_provider_inst_t *lpi;
    const char *handler;
    ib_status_t rc;
    ib_context_t *main_ctx;
    ib_core_cfg_t *main_core_config;
    ib_provider_t *main_lp;
    FILE *orig_fp;

    /* Initialize the rule engine for the context */
    rc = ib_rule_engine_ctx_close(ib, mod, ctx);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to close rule engine context: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Get the main context config, it's config, and it's logger. */
    main_ctx = ib_context_main(ib);
    rc = ib_context_module_config(main_ctx, ib_core_module(),
                                  (void *)&main_core_config);
    if (rc != IB_OK) {
        ib_log_alert(ib,
                     "Failed to fetch main core module context config: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    main_lp = main_core_config->pi.logger->pr;


    /* Get the current context config. */
    rc = ib_context_module_config(ctx, mod, (void *)&corecfg);
    if (rc != IB_OK) {
        ib_log_alert(ib,
                     "Failed to fetch core module context config: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Lookup/set logger provider. */
    handler = corecfg->log_handler;
    rc = ib_provider_instance_create(ib,
                                     IB_PROVIDER_TYPE_LOGGER,
                                     handler,
                                     &lpi,
                                     ib->mp,
                                     NULL);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to create %s provider instance '%s': %s",
                     IB_PROVIDER_TYPE_LOGGER, handler, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    ib_log_provider_set_instance(ctx, lpi);

    /* Get the log provider. */
    lp  = lpi->pr;

    /* If it's not the core log provider, we're done: we know nothing
     * about it's data, so don't try to treat it as a file handle! */
    if ( main_lp != lp ) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Now, copy the parent's file handle (which was copied in for us
       when the context was created) */
    orig_fp = (FILE *) lpi->data;
    if ( orig_fp != NULL ) {
        FILE *new_fp = fdup( orig_fp );
        if ( new_fp != NULL ) {
            lpi->data = new_fp;
        }
        else {
            fprintf(stderr,
                    "core_ctx_close:failed to duplicate file handle: %s\n",
                    strerror(errno));
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Close the core module context
 *
 * @param ib Engine
 * @param mod Module
 * @param ctx Context
 * @param cbdata Callback data (unused)
 *
 * @returns Status code
 */
static ib_status_t core_ctx_destroy(ib_engine_t *ib,
                                    ib_module_t *mod,
                                    ib_context_t *ctx,
                                    void *cbdata)
{
    IB_FTRACE_INIT();
    ib_core_cfg_t *corecfg;
    ib_provider_t *lp;
    ib_provider_inst_t *lpi;
    ib_status_t rc;
    ib_context_t *main_ctx;
    ib_core_cfg_t *main_core_config;
    ib_provider_t *main_lp;
    FILE *fp;

    /* Get the main context config, it's config, and it's logger. */
    main_ctx = ib_context_main(ib);

    /* If the main context has already been destroyed nothing must be done. */
    if (main_ctx == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    rc = ib_context_module_config(main_ctx, ib_core_module(),
                                  (void *)&main_core_config);
    if (rc != IB_OK) {
        ib_log_alert(ib,
                     "Failed to fetch main core module context config: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    main_lp = main_core_config->pi.logger->pr;


    /* Get the current context config. */
    rc = ib_context_module_config(ctx, mod, (void *)&corecfg);
    if (rc != IB_OK) {
        ib_log_alert(ib,
                     "Failed to fetch core module context config: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Get the current logger. */
    lpi = corecfg->pi.logger;
    if (lpi == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    lp  = lpi->pr;

    /* If it's not the core log provider, we're done: we know nothing
     * about it's data, so don't try to treat it as a file handle! */
    if (main_lp != lp) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }
    else if (  (main_ctx == ctx) && (ib_context_engine(ib) == ctx)  ) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Close our file handle */
    fp = (FILE *) lpi->data;
    if (fp != NULL) {
        if (fclose(fp) < 0) {
            fprintf( stderr,
                     "core_ctx_destroy:Failed closing our fp %p: %s\n",
                     (void *)fp, strerror(errno) );
        }
        lpi->data = NULL;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Static core module structure.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /**< Default metadata */
    MODULE_NAME_STR,                     /**< Module name */
    IB_MODULE_CONFIG(&core_global_cfg),  /**< Global config data */
    core_config_map,                     /**< Configuration field map */
    core_directive_map,                  /**< Config directive map */
    core_init,                           /**< Initialize function */
    NULL,                                /**< Callback data */
    NULL,                                /**< Finish function */
    NULL,                                /**< Callback data */
    core_ctx_open,                       /**< Context open function */
    NULL,                                /**< Callback data */
    core_ctx_close,                      /**< Context close function */
    NULL,                                /**< Callback data */
    core_ctx_destroy,                    /**< Context destroy function */
    NULL                                 /**< Callback data */
);
