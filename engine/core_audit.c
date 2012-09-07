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

#include "core_audit_private.h"
#include "core_private.h"
#include "engine_private.h"
#include "rule_engine_private.h"

#include <ironbee/core.h>
#include <ironbee/debug.h>
#include <ironbee/engine_types.h>
#include <ironbee/path.h>
#include <ironbee/provider.h>
#include <ironbee/rule_engine.h>
#include <ironbee/util.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    core_audit_cfg_t *cfg;
    ib_auditlog_t *log;
    ib_tx_t *tx;
    ib_conn_t *conn;
    ib_site_t *site;
} auditlog_callback_data_t;

/* The default shell to use for piped commands. */
static const char * const ib_pipe_shell = "/bin/sh";
const size_t LOGFORMAT_MAX_LINE_LENGTH = 8192;

ib_status_t core_audit_open_auditfile(ib_provider_inst_t *lpi,
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

ib_status_t core_audit_open_auditindexfile(ib_provider_inst_t *lpi,
                                           ib_auditlog_t *log,
                                           core_audit_cfg_t *cfg,
                                           ib_core_cfg_t *corecfg)
{
    IB_FTRACE_INIT();

    char* index_file;
    int index_file_sz;
    ib_status_t ib_rc;
    int sys_rc;

    if (log->ctx->auditlog->index == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

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

    ib_log_info(log->ib, "AUDITLOG INDEX%s: %s",
                (log->ctx->auditlog->index[0] == '|'?" (piped)":""),
                index_file);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t core_audit_open(ib_provider_inst_t *lpi,
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
            rc = ib_logformat_parse(auditlog_index_hp,
                                    corecfg->auditlog_index_fmt);
        }
        else {
            rc = ib_logformat_parse(auditlog_index_hp, IB_LOGFORMAT_DEFAULT);
        }
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        /* Commit built struct. */
        corecfg->auditlog_index_hp = auditlog_index_hp;
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t core_audit_write_header(ib_provider_inst_t *lpi,
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

ib_status_t core_audit_write_part(ib_provider_inst_t *lpi,
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

ib_status_t core_audit_write_footer(ib_provider_inst_t *lpi,
                                    ib_auditlog_t *log)
{
    IB_FTRACE_INIT();
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;

    if (cfg->parts_written > 0) {
        fprintf(cfg->fp, "\r\n--%s--\r\n", cfg->boundary);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t audit_add_line_item(const ib_logformat_t *lf,
                                       const ib_logformat_field_t *field,
                                       const void *cbdata,
                                       const char **str)
{
    IB_FTRACE_INIT();

    const auditlog_callback_data_t *logdata =
        (const auditlog_callback_data_t *)cbdata;

    switch (field->fchar) {

    case IB_LOG_FIELD_REMOTE_ADDR:
        *str = logdata->tx->er_ipstr;
        break;
    case IB_LOG_FIELD_LOCAL_ADDR:
        *str = logdata->conn->local_ipstr;
        break;
    case IB_LOG_FIELD_HOSTNAME:
        *str = logdata->tx->hostname;
        break;
    case IB_LOG_FIELD_SITE_ID:
        if (logdata->site == NULL) {
            *str = (char *)"-";
        }
        else {
            *str = logdata->site->id_str;
        }
        break;
    case IB_LOG_FIELD_SENSOR_ID:
        *str = logdata->log->ib->sensor_id_str;
        break;
    case IB_LOG_FIELD_TRANSACTION_ID:
        *str = logdata->tx->id;
        break;
    case IB_LOG_FIELD_TIMESTAMP:
    {
        char *tstamp = NULL;
        /* Prepare timestamp (only if needed) */
        tstamp = (char *)ib_mpool_alloc(logdata->log->mp, 30);
        if (tstamp == NULL) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        ib_clock_timestamp(tstamp, &logdata->tx->tv_created);
        *str = tstamp;
        break;
    }
    case IB_LOG_FIELD_LOG_FILE:
        *str = logdata->cfg->fn;
        break;
    default:
        *str = "\n";
        /* Not understood */
        IB_FTRACE_RET_STATUS(IB_EINVAL);
        break;
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t core_audit_get_index_line(ib_provider_inst_t *lpi,
                                             ib_auditlog_t *log,
                                             char *line,
                                             size_t line_size,
                                             size_t *line_len)
{
    IB_FTRACE_INIT();
    assert(lpi != NULL);
    assert(log != NULL);
    assert(line != NULL);
    assert(line_size > 0);
    assert(line_len != NULL);
    assert(log->tx != NULL);

    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;
    ib_core_cfg_t *corecfg;
    ib_tx_t *tx = log->tx;
    ib_conn_t *conn = tx->conn;
    ib_site_t *site = ib_context_site_get(log->ctx);
    const ib_logformat_t *lf;
    ib_status_t rc;
    auditlog_callback_data_t cbdata;

    /* Retrieve corecfg to get the AuditLogIndexFormat */
    rc = ib_context_module_config(log->ctx, ib_core_module(),
                                  (void *)&corecfg);

    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    lf = corecfg->auditlog_index_hp;

    cbdata.cfg = cfg;
    cbdata.log = log;
    cbdata.tx = tx;
    cbdata.conn = conn;
    cbdata.site = site;
    rc = ib_logformat_format(lf, line, line_size, line_len,
                             audit_add_line_item, &cbdata);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t core_audit_close(ib_provider_inst_t *lpi, ib_auditlog_t *log)
{
    IB_FTRACE_INIT();
    core_audit_cfg_t *cfg = (core_audit_cfg_t *)log->cfg_data;
    ib_core_cfg_t *corecfg;
    ib_status_t ib_rc = IB_OK;
    int sys_rc;
    char *line = NULL;
    size_t len = 0;

    line = malloc(LOGFORMAT_MAX_LINE_LENGTH + 2);
    if (line == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Retrieve corecfg to get the AuditLogIndexFormat */
    ib_rc = ib_context_module_config(log->ctx, ib_core_module(),
                                     &corecfg);
    if (ib_rc != IB_OK) {
        ib_log_alert(log->ib,
                     "Failure accessing core module: %s",
                     ib_status_to_string(ib_rc));
        goto cleanup;
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
            ib_rc = IB_EOTHER;
            goto cleanup;
        }
        ib_log_info(log->ib, "AUDITLOG: %s", cfg->full_path);
        cfg->fp = NULL;
    }

    /* Write to the index file if using one. */
    if ((cfg->index_fp != NULL) && (cfg->parts_written > 0)) {

        ib_lock_lock(&log->ctx->auditlog->index_fp_lock);

        ib_rc = core_audit_get_index_line(lpi, log, line,
                                          LOGFORMAT_MAX_LINE_LENGTH,
                                          &len);
        line[len + 0] = '\n';
        line[len + 1] = '\0';

        if ( (ib_rc != IB_ETRUNC) && (ib_rc != IB_OK) ) {
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
            goto cleanup;
        }

        sys_rc = fwrite(line, len, 1, cfg->index_fp);

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
            goto cleanup;
        }

        fflush(cfg->index_fp);
        ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
    }

cleanup:
    if (line != NULL) {
        free(line);
    }
    IB_FTRACE_RET_STATUS(ib_rc);
}
