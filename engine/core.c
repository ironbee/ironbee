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
 * @brief IronBee --- Core Module
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/core.h>

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif

#include "core_private.h"
#include "core_audit_private.h"
#include "engine_private.h"
#include "state_notify_private.h"

#include <ironbee/bytestr.h>
#include <ironbee/cfgmap.h>
#include <ironbee/clock.h>
#include <ironbee/context.h>
#include <ironbee/context_selection.h>
#include <ironbee/engine_types.h>
#include <ironbee/escape.h>
#include <ironbee/field.h>
#include <ironbee/flags.h>
#include <ironbee/json.h>
#include <ironbee/logevent.h>
#include <ironbee/mm.h>
#include <ironbee/rule_defs.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>
#include <ironbee/transformation.h>
#include <ironbee/util.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#define MODULE_NAME        core
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

IB_MODULE_DECLARE();

/* The default UUID value */
static const char * const ib_uuid_default_str = "00000000-0000-0000-0000-000000000000";

#ifndef MODULE_BASE_PATH
/* Always define a module base path. */
#define MODULE_BASE_PATH /usr/local/ironbee/libexec
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


/* Rule log parts amalgamation */
#define IB_RULE_LOG_FLAGS_REQUEST                               \
    ( IB_RULE_LOG_FLAG_REQ_LINE |                               \
      IB_RULE_LOG_FLAG_REQ_HEADER |                             \
      IB_RULE_LOG_FLAG_REQ_BODY )
#define IB_RULE_LOG_FLAGS_RESPONSE                   \
    ( IB_RULE_LOG_FLAG_RSP_LINE |                    \
      IB_RULE_LOG_FLAG_RSP_HEADER |                  \
      IB_RULE_LOG_FLAG_RSP_BODY )
#define IB_RULE_LOG_FLAGS_EXEC                         \
    ( IB_RULE_LOG_FLAG_PHASE |                         \
      IB_RULE_LOG_FLAG_RULE |                          \
      IB_RULE_LOG_FLAG_TARGET |                        \
      IB_RULE_LOG_FLAG_TFN |                           \
      IB_RULE_LOG_FLAG_OPERATOR |                      \
      IB_RULE_LOG_FLAG_ACTION |                        \
      IB_RULE_LOG_FILT_ACTIONABLE )
#define IB_RULE_LOG_FLAGS_ALL                                \
    ( IB_RULE_LOG_FLAG_TX |                                  \
      IB_RULE_LOG_FLAG_REQ_LINE |                            \
      IB_RULE_LOG_FLAG_REQ_HEADER |                          \
      IB_RULE_LOG_FLAG_REQ_BODY |                            \
      IB_RULE_LOG_FLAG_RSP_LINE |                            \
      IB_RULE_LOG_FLAG_RSP_HEADER |                          \
      IB_RULE_LOG_FLAG_RSP_BODY |                            \
      IB_RULE_LOG_FLAG_PHASE |                               \
      IB_RULE_LOG_FLAG_RULE |                                \
      IB_RULE_LOG_FLAG_TARGET |                              \
      IB_RULE_LOG_FLAG_TFN |                                 \
      IB_RULE_LOG_FLAG_OPERATOR |                            \
      IB_RULE_LOG_FLAG_ACTION |                              \
      IB_RULE_LOG_FLAG_EVENT |                               \
      IB_RULE_LOG_FLAG_AUDIT )


/* Inspection Engine Options */
#define IB_IEOPT_REQUEST_URI              IB_TX_FINSPECT_REQURI
#define IB_IEOPT_REQUEST_PARAMS           IB_TX_FINSPECT_REQPARAMS
#define IB_IEOPT_REQUEST_HEADER \
    ( IB_TX_FINSPECT_REQHDR | \
      IB_TX_FINSPECT_REQURI )
#define IB_IEOPT_REQUEST_BODY             IB_TX_FINSPECT_REQBODY
#define IB_IEOPT_RESPONSE_HEADER          IB_TX_FINSPECT_RESHDR
#define IB_IEOPT_RESPONSE_BODY            IB_TX_FINSPECT_RESBODY
#define IB_IEOPT_RESPONSE_BODY            IB_TX_FINSPECT_RESBODY

/* NOTE: Make sure to add new options from above to any groups below. */
#define IB_IEOPT_DEFAULT \
    ( IB_IEOPT_REQUEST_HEADER | \
      IB_IEOPT_REQUEST_URI | \
      IB_IEOPT_REQUEST_PARAMS )
#define IB_IEOPT_ALL \
    ( IB_IEOPT_REQUEST_HEADER | \
      IB_IEOPT_REQUEST_BODY | \
      IB_IEOPT_RESPONSE_HEADER | \
      IB_IEOPT_RESPONSE_BODY | \
      IB_IEOPT_REQUEST_URI | \
      IB_IEOPT_REQUEST_PARAMS )
#define IB_IEOPT_REQUEST \
    ( IB_IEOPT_REQUEST_HEADER | \
      IB_IEOPT_REQUEST_BODY | \
      IB_IEOPT_REQUEST_URI | \
      IB_IEOPT_REQUEST_PARAMS )
#define IB_IEOPT_RESPONSE \
    ( IB_IEOPT_RESPONSE_HEADER | \
      IB_IEOPT_RESPONSE_BODY )

/* Protection Engine Options */
#define IB_PEOPT_BLOCKING_MODE            IB_TX_FBLOCKING_MODE

/* NOTE: Make sure to add new options from above to any groups below. */
#define IB_PEOPT_DEFAULT                  0
#define IB_PEOPT_ALL \
    ( IB_PEOPT_BLOCKING_MODE )

/* -- Utilities -- */

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
 * @return
 * - IB_OK On success.
 * - IB_EALLOC on malloc failures.
 * - Other on escaping failures.
 */
static ib_status_t core_unescape(ib_engine_t *ib, char **dst, const char *src)
{
    size_t src_len = strlen(src);
    char *dst_tmp = ib_mm_alloc(ib_engine_mm_main_get(ib), src_len+1);
    size_t dst_len;
    ib_status_t rc;

    if ( dst_tmp == NULL ) {
        return IB_EALLOC;
    }

    rc = ib_util_unescape_string(dst_tmp, &dst_len, src, src_len);
    if (rc != IB_OK) {
        ib_log_debug(ib, "Failed to unescape string \"%s\"", src);
        return rc;
    }

    /* There may be no null characters in the escaped string. */
    if (memchr(dst_tmp, '\0', dst_len) != NULL) {
        ib_log_debug(ib,
                     "Failed to unescape string \"%s\" because resultant unescaped "
                     "string contains a NULL character.",
                     src);
        return IB_EBADVAL;
    }

    assert(dst_len <= src_len);

    /* Null-terminate the string. */
    dst_tmp[dst_len] = '\0';

    /* Success! */
    *dst = dst_tmp;

    return IB_OK;
}

/// @todo Make this public
static ib_status_t ib_auditlog_part_add(ib_auditlog_t *log,
                                        const char *name,
                                        const char *type,
                                        void *data,
                                        ib_auditlog_part_gen_fn_t generator,
                                        void *gen_data)
{
    ib_status_t rc;

    ib_auditlog_part_t *part =
        (ib_auditlog_part_t *)ib_mm_alloc(log->mm, sizeof(*part));

    if (part == NULL) {
        return IB_EALLOC;
    }

    part->log = log;
    part->name = name;
    part->content_type = type;
    part->part_data = data;
    part->fn_gen = generator;
    part->gen_data = gen_data;

    rc = ib_list_push(log->parts, part);

    return rc;
}

/* -- Logger API Implementations -- */

static void core_logger_element(void *element, void *cbdata)
{
    assert(element != NULL);
    assert(cbdata  != NULL);

    ib_logger_standard_msg_t *msg = (ib_logger_standard_msg_t *)element;
    ib_core_cfg_t            *cfg = (ib_core_cfg_t *)cbdata;

    fprintf(
        cfg->log_fp,
        "%s %.*s\n",
        msg->prefix,
        (int)msg->msg_sz,
        (char *)msg->msg);
    fflush(cfg->log_fp);
}

/**
 * Logger callback that writes log records to core's file descriptor.
 *
 * @param[in] logger The logger.
 * @param[in] writer The writer with the queued log messages.
 * @param[in] cbdata Callback data. The @ref ib_core_cfg_t.
 */
static ib_status_t core_logger_record(
    ib_logger_t        *logger,
    ib_logger_writer_t *writer,
    void               *cbdata
)
{
    return ib_logger_dequeue(logger, writer, core_logger_element, cbdata);
}

/**
 * Take the @a cfg FILE pointer for the log file and give it to the logger.
 *
 * @param[in] ib Engine to be modified.
 * @param[in] corecfg The core configuration use to configure the logger in
 *            @a ib.
 *
 * @returns
 * - IB_OK On success.
 * - Other on ib_logger_* failures.
 */
static ib_status_t core_add_core_logger(
    ib_engine_t   *ib,
    ib_core_cfg_t *corecfg
)
{
    assert(ib != NULL);
    assert(corecfg != NULL);
    assert(corecfg->log_fp != NULL);

    ib_status_t         rc;
    ib_logger_format_t *fmt;

    rc = ib_logger_fetch_format(
        ib_engine_logger_get(ib),
        IB_LOGGER_DEFAULT_FORMATTER_NAME,
        &fmt);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_logger_writer_add(
        ib_engine_logger_get(ib),
        NULL, /* Open. */
        NULL,
        NULL, /* Close. */
        NULL,
        NULL, /* Reopen. */
        NULL,
        fmt,
        core_logger_record,
        corecfg
    );
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Open the configuration's log file
 *
 * @param[in] ib IronBee engine
 * @param[in,out] config Core configuration
 */
static void core_log_file_close(ib_engine_t *ib,
                                ib_core_cfg_t *config)
{
    assert(ib != NULL);
    assert(config != NULL);

    if (config->log_fp == NULL) {
        config->log_fp = stderr;
    }
    else if (config->log_fp != stderr) {
        fclose(config->log_fp);
        config->log_fp = stderr;
    }
}

/* -- Audit Implementations -- */

/**
 * Signal that an event part has not stated to be generated yet.
 *
 * If ib_auditlog_part_t::gen_data is set to this, then generation of the
 * event part has not occurred yet.
 *
 * @sa AUDITLOG_GEN_FINISHED
 */
static void *AUDITLOG_GEN_NOTSTARTED = NULL;

/**
 * Signal that an event part has finished being generated.
 *
 * If ib_auditlog_part_t::gen_data is set to this, then generation of the
 * event part has completed.
 *
 * @sa AUDITLOG_GEN_NOTSTARTED
 */
static void *AUDITLOG_GEN_FINISHED = (void *)-1;

/**
 * Write an audit log.
 *
 * @param[in] ib IronBee engine.
 * @param[in] log Log to write.
 *
 * @returns Status code
 */
static ib_status_t audit_write_log(ib_engine_t *ib, ib_auditlog_t *log)
{
    ib_list_node_t *node;
    ib_status_t rc;

    if (ib_list_elements(log->parts) == 0) {
        ib_log_error(ib, "No parts to write to audit log.");
        return IB_EINVAL;
    }

    /* Open the log if required. This is thread safe. */
    rc = core_audit_open(ib, log);
    if (rc != IB_OK) {
        if (log->ctx->auditlog->index != NULL) {
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
        }
        return rc;
    }

    /* Lock to write. */
    if (log->ctx->auditlog->index != NULL) {
        rc = ib_lock_lock(&log->ctx->auditlog->index_fp_lock);
        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to lock \"%s\" for write.",
                         log->ctx->auditlog->index);
            return rc;
        }
    }

    /* Write the header if required. */
    rc = core_audit_write_header(ib, log);
    if (rc != IB_OK) {
        ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
        return rc;
    }

    /* Write the parts. */
    IB_LIST_LOOP(log->parts, node) {
        ib_auditlog_part_t *part =
            (ib_auditlog_part_t *)ib_list_node_data(node);
        rc = core_audit_write_part(ib, part);
        if (rc != IB_OK) {
            ib_log_error(log->ib, "Error writing audit log part: %s",
                         part->name);
        }
    }

    /* Write the footer if required. */
    rc = core_audit_write_footer(ib, log);
    if (rc != IB_OK) {
        if (log->ctx->auditlog->index != NULL) {
            ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
        }
        return rc;
    }

    /* Writing is done. Unlock. Close is thread-safe. */
    if (log->ctx->auditlog->index != NULL) {
        ib_lock_unlock(&log->ctx->auditlog->index_fp_lock);
    }

    /* Close the audit log and write to the index file. */
    rc = core_audit_close(ib, log);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

static size_t ib_auditlog_gen_raw_stream(ib_auditlog_part_t *part,
                                         const uint8_t **chunk)
{
    ib_sdata_t *sdata;
    size_t dlen;

    if (part->gen_data == AUDITLOG_GEN_NOTSTARTED) {
        ib_stream_t *stream = (ib_stream_t *)part->part_data;

        /* No data. */
        if (stream->slen == 0) {
            *chunk = NULL;
            part->gen_data = AUDITLOG_GEN_FINISHED;
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
            part->gen_data = AUDITLOG_GEN_FINISHED;
        }

        return dlen;
    }
    else if (part->gen_data == AUDITLOG_GEN_FINISHED) {
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
        part->gen_data = AUDITLOG_GEN_FINISHED;
    }

    return dlen;
}

static size_t ib_auditlog_gen_json_flist(ib_auditlog_part_t *part,
                                         const uint8_t **chunk)
{
    ib_engine_t *ib = part->log->ib;
    ib_status_t rc;

    /* When gen_data = -1, end the generation by returning 0. */
    if (part->gen_data == AUDITLOG_GEN_FINISHED) {
        part->gen_data = AUDITLOG_GEN_NOTSTARTED;
        return 0;
    }

    /* We only call this function twice. Once to do the work. Once to signal
     * the work is done. */
    part->gen_data = AUDITLOG_GEN_FINISHED;

    if (part->part_data != NULL) {
        ib_mm_t mm = part->log->mm;
        size_t chunk_len;

        rc = ib_json_encode(
            mm,
            (ib_list_t *)part->part_data,
            true,
            (char **)chunk,
            &chunk_len
        );
        if (rc != IB_OK) {
            ib_log_notice(ib, "Unable to generate JSON.");
            *chunk = (uint8_t *)("{}");
            return 2;
        }

        return chunk_len;
    }
    else {
        ib_log_notice(ib, "No data in audit log part: %s", part->name);
        *chunk = (const uint8_t *)"{}";
        part->gen_data = AUDITLOG_GEN_FINISHED;
        return strlen(*(const char **)chunk);
    }
}

static size_t ib_auditlog_gen_header_flist(ib_auditlog_part_t *part,
                                           const uint8_t **chunk)
{
    ib_engine_t *ib = part->log->ib;
    ib_field_t *f;
    uint8_t *rec;
    int rlen;
    ib_status_t rc;

#define CORE_HEADER_MAX_FIELD_LEN 8192

    /* The gen_data field is used to store the current state. NULL
     * means the part has not started yet and a -1 value
     * means it is done. Anything else is a node in the event list.
     */
    if (part->gen_data == AUDITLOG_GEN_NOTSTARTED) {
        ib_list_t *list = (ib_list_t *)part->part_data;

        /* No data. */
        if (ib_list_elements(list) == 0) {
            ib_log_notice(ib, "No data in audit log part: %s", part->name);
            part->gen_data = AUDITLOG_GEN_NOTSTARTED;
            return 0;
        }

        /* First should be a request/response line. */
        part->gen_data = ib_list_first(list);
        f = (ib_field_t *)ib_list_node_data((ib_list_node_t *)part->gen_data);
        if ((f != NULL) && (f->type == IB_FTYPE_BYTESTR)) {
            const ib_bytestr_t *bs;
            rec = (uint8_t *)ib_mm_alloc(part->log->mm,
                                         CORE_HEADER_MAX_FIELD_LEN);
            if (rec == NULL) {
                return 0;
            }

            rc = ib_field_value(f, ib_ftype_bytestr_out(&bs));
            if (rc != IB_OK) {
                return 0;
            }

            rlen = snprintf((char *)rec, CORE_HEADER_MAX_FIELD_LEN,
                            "%" IB_BYTESTR_FMT "\r\n",
                            IB_BYTESTR_FMT_PARAM(bs));

            /* Verify size. */
            if (rlen >= CORE_HEADER_MAX_FIELD_LEN) {
                ib_log_notice(ib, "Item too large to log in part %s: %d",
                              part->name, rlen);
                *chunk = (const uint8_t *)"\r\n";
                part->gen_data = AUDITLOG_GEN_FINISHED;
                return strlen(*(const char **)chunk);
            }

            *chunk = rec;

            part->gen_data =
                ib_list_node_next((ib_list_node_t *)part->gen_data);
            if (part->gen_data == AUDITLOG_GEN_NOTSTARTED) {
                part->gen_data = AUDITLOG_GEN_FINISHED;
            }

            return strlen(*(const char **)chunk);
        }
    }
    else if (part->gen_data == AUDITLOG_GEN_FINISHED) {
        part->gen_data = AUDITLOG_GEN_NOTSTARTED;
        *chunk = (const uint8_t *)"";
        return 0;
    }

    /* Header Lines */
    f = (ib_field_t *)ib_list_node_data((ib_list_node_t *)part->gen_data);
    if (f == NULL) {
        ib_log_notice(ib, "NULL field in part: %s", part->name);
        *chunk = (const uint8_t *)"\r\n";
        part->gen_data = AUDITLOG_GEN_FINISHED;
        return strlen(*(const char **)chunk);
    }

    rec = (uint8_t *)ib_mm_alloc(part->log->mm, CORE_HEADER_MAX_FIELD_LEN);
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
        ib_log_notice(ib, "Item too large to log in part %s: %d",
                      part->name, rlen);
        *chunk = (const uint8_t *)"\r\n";
        part->gen_data = AUDITLOG_GEN_FINISHED;
        return strlen(*(const char **)chunk);
    }

    *chunk = rec;

    /* Stage the next chunk of data (header). */
    part->gen_data = ib_list_node_next((ib_list_node_t *)part->gen_data);

    /* Close the structure if there is no more data. */
    if (part->gen_data == AUDITLOG_GEN_NOTSTARTED) {
        part->gen_data = AUDITLOG_GEN_FINISHED;
    }

    return strlen(*(const char **)chunk);
}


static size_t ib_auditlog_gen_json_events(ib_auditlog_part_t *part,
                                          const uint8_t **chunk)
{
    assert(part != NULL);
    assert(part->log != NULL);
    assert(part->log->ib != NULL);
    assert(part->log->tx != NULL);

    ib_status_t       rc;
    yajl_gen_status   yajl_status;

    ib_tx_t              *tx   = part->log->tx;
    const ib_list_t      *list = (const ib_list_t *)part->part_data;
    const ib_list_node_t *node;
    yajl_gen              yajl_handle;
    size_t                chunk_len = 0; /* This value is returned. */

    /* When gen_data == -1, return 0. */
    if (part->gen_data == AUDITLOG_GEN_FINISHED) {
        part->gen_data = AUDITLOG_GEN_NOTSTARTED;
        return 0;
    }

    /* Gen_data may only be NULL or -1. */
    assert(part->gen_data == AUDITLOG_GEN_NOTSTARTED);

    /* No events. */
    if (ib_list_elements(list) == 0) {
        *chunk = (const uint8_t *)"{}";
        return 2;
    }

    rc = ib_json_yajl_gen_create(&yajl_handle, part->log->mm);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Failed to create JSON generation resource.");
        goto failure;
    }

    /* Set pretty option. */
    int opt = yajl_gen_config(yajl_handle, yajl_gen_beautify, 1);
    if (opt == 0) {
        ib_log_error_tx(tx, "Failed to set yajl beautify option.");
        goto failure;
    }

    yajl_status = yajl_gen_map_open(yajl_handle);
    if (yajl_status != yajl_gen_status_ok) {
        ib_log_error_tx(tx, "Failed to open events JSON map.");
        goto failure;
    }

    yajl_status =
        yajl_gen_string(yajl_handle, (unsigned char *)"events", 6);
    if (yajl_status != yajl_gen_status_ok) {
        ib_log_error_tx(tx, "Failed to add events JSON array.");
        goto failure;
    }

    yajl_status = yajl_gen_array_open(yajl_handle);
    if (yajl_status != yajl_gen_status_ok) {
        ib_log_error_tx(tx, "Failed to open events JSON array.");
        goto failure;
    }

    IB_LIST_LOOP_CONST(list, node) {
        const ib_logevent_t *e =
            (const ib_logevent_t *)ib_list_node_data_const(node);

        /* Open event hash. */
        yajl_status = yajl_gen_map_open(yajl_handle);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to open event JSON map.");
            goto failure;
        }

        /* Event ID. */
        yajl_status =
            yajl_gen_string(yajl_handle, (unsigned char *)"event-id", 8);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add event id.");
            goto failure;
        }

        yajl_status = yajl_gen_integer(yajl_handle, e->event_id);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add event id.");
            goto failure;
        }

        /* Rule ID. */
        yajl_status =
            yajl_gen_string(yajl_handle, (unsigned char *)"rule-id", 7);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add rule id for event.");
            goto failure;
        }

        yajl_status = yajl_gen_string(
            yajl_handle, (unsigned char *)IB_S2SL(e->rule_id)
        );
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add rule id for event.");
            goto failure;
        }

        /* Type. */
        yajl_status = yajl_gen_string(yajl_handle, (unsigned char *)"type", 4);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add event type.");
            goto failure;
        }

        yajl_status = yajl_gen_string(
            yajl_handle,
            (unsigned char *)IB_S2SL(ib_logevent_type_name(e->type)));
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add event type.");
            goto failure;
        }

        /* Suppress. */
        yajl_status =
            yajl_gen_string(yajl_handle, (unsigned char *)"suppress", 8);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add suppression.");
            goto failure;
        }

        yajl_status = yajl_gen_string(
            yajl_handle,
            (unsigned char *)IB_S2SL(ib_logevent_suppress_name(e->suppress)));
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add suppression flags.");
            goto failure;
        }

        /* Recommended Action. */
        yajl_status = yajl_gen_string(
            yajl_handle, (unsigned char *)"rec-action", 10);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add recommended action.");
            goto failure;
        }

        yajl_status = yajl_gen_string(
            yajl_handle,
            (unsigned char *)IB_S2SL(ib_logevent_action_name(e->rec_action)));
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add recommended action.");
            goto failure;
        }

        /* Confidence. */
        yajl_status =
            yajl_gen_string(yajl_handle, (unsigned char *)"confidence", 10);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add confidence.");
            goto failure;
        }

        yajl_status = yajl_gen_double(yajl_handle, e->confidence);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add confidence.");
            goto failure;
        }

        /* Severity. */
        yajl_status =
            yajl_gen_string(yajl_handle, (unsigned char *)"severity", 8);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add severity.");
            goto failure;
        }

        yajl_status = yajl_gen_double(yajl_handle, e->severity);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add severity.");
            goto failure;
        }

        /* Tag List. */
        yajl_status =
            yajl_gen_string(yajl_handle, (unsigned char *)"tags", 4);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add tags.");
            goto failure;
        }

        yajl_status = yajl_gen_array_open(yajl_handle);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to open tag JSON array.");
            goto failure;
        }

        if (e->tags != NULL) {
            const ib_list_node_t *tag_node;
            IB_LIST_LOOP_CONST(e->tags, tag_node) {
                const char *tag_name =
                    (const char *)ib_list_node_data_const(tag_node);

                yajl_status = yajl_gen_string(
                    yajl_handle,
                    (unsigned char *)IB_S2SL(tag_name));
                if (yajl_status != yajl_gen_status_ok) {
                    ib_log_error_tx(
                        tx, "Failed to add tag: %s", tag_name
                    );
                    return IB_EOTHER;
                }
            }
        }

        yajl_status = yajl_gen_array_close(yajl_handle);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to close tag JSON array.");
            goto failure;
        }

        /* Field List. */
        yajl_status = yajl_gen_string(yajl_handle, (unsigned char *)"fields", 6);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add fields.");
            goto failure;
        }

        yajl_status = yajl_gen_array_open(yajl_handle);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to open fields JSON array.");
            goto failure;
        }

        if (e->fields != NULL) {
            const ib_list_node_t *field_node;

            IB_LIST_LOOP_CONST(e->fields, field_node) {
                const char *field_name =
                    (const char *) ib_list_node_data_const(field_node);

                yajl_status = yajl_gen_string(
                    yajl_handle,
                    (unsigned char *)IB_S2SL(field_name));
                if (yajl_status != yajl_gen_status_ok) {
                    ib_log_error_tx(
                        tx, "Failed to add field name: %s", field_name
                    );
                    return IB_EOTHER;
                }
            }
        }

        yajl_status = yajl_gen_array_close(yajl_handle);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to close fields JSON array.");
            goto failure;
        }

        /* Message. */
        yajl_status = yajl_gen_string(yajl_handle, (unsigned char *)"msg", 3);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add event message.");
            goto failure;
        }

        yajl_status = yajl_gen_string(
            yajl_handle,
            (unsigned char *)IB_S2SL(e->msg != NULL ? e->msg : "-"));
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to encode event message.");
            goto failure;
        }

        /* Event Data. */
        yajl_status = yajl_gen_string(yajl_handle, (unsigned char *)"data", 4);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to add event data.");
            goto failure;
        }

        yajl_status = yajl_gen_string(yajl_handle, e->data, e->data_len);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to encode event data.");
            goto failure;
        }

        /* Close event hash. */
        yajl_status = yajl_gen_map_close(yajl_handle);
        if (yajl_status != yajl_gen_status_ok) {
            ib_log_error_tx(tx, "Failed to close event JSON map.");
            goto failure;
        }
    }

    yajl_status = yajl_gen_array_close(yajl_handle);
    if (yajl_status != yajl_gen_status_ok) {
        ib_log_error_tx(tx, "Failed to close events JSON array.");
        goto failure;
    }

    yajl_status = yajl_gen_map_close(yajl_handle);
    if (yajl_status != yajl_gen_status_ok) {
        ib_log_error_tx(tx, "Failed to close events JSON map.");
        goto failure;
    }

    yajl_status = yajl_gen_get_buf(
        yajl_handle,
        (const unsigned char **)chunk,
        &chunk_len
    );
    if (yajl_status != yajl_gen_status_ok) {
        ib_log_error_tx(tx, "Failed to generate event JSON.");
        goto failure;
    }

    /* Signal that we should stop. */
    part->gen_data = AUDITLOG_GEN_FINISHED;

    return chunk_len;

failure:
    /* Signal that we should stop. */
    part->gen_data = AUDITLOG_GEN_FINISHED;

    return 0;
}

#define CORE_AUDITLOG_FORMAT "http-message/1"

static ib_status_t ib_auditlog_add_part_header(ib_auditlog_t *log)
{
    assert(log != NULL);
    assert(log->tx != NULL);

    ib_core_audit_cfg_t *cfg = (ib_core_audit_cfg_t *)log->cfg_data;
    ib_engine_t *ib = log->ib;
    ib_tx_t *tx = log->tx;
    ib_num_t tx_num = tx->conn->tx_count;
    const ib_site_t *site;
    ib_mm_t mm = log->mm;
    const ib_field_t *threat_level_f;
    ib_field_t *f;
    ib_list_t *list;
    ib_num_t tx_time = 0;
    char *tstamp;
    char *log_format;
    ib_status_t rc;

    /* Timestamp */
    tstamp = (char *)ib_mm_alloc(mm, 30);
    if (tstamp == NULL) {
        return IB_EALLOC;
    }
    ib_clock_relative_timestamp(tstamp, &log->tx->tv_created,
                                (log->tx->t.logtime - log->tx->t.started));

    /*
     * Transaction time depends on where processing stopped.
     */
    if (tx->t.response_finished > tx->t.request_started) {
        tx_time = IB_CLOCK_USEC_TO_MSEC(tx->t.response_finished - tx->t.request_started);
    }
    else if (tx->t.response_body > tx->t.request_started) {
        tx_time = IB_CLOCK_USEC_TO_MSEC(tx->t.response_body - tx->t.request_started);
    }
    else if (tx->t.response_header > tx->t.request_started) {
        tx_time = IB_CLOCK_USEC_TO_MSEC(tx->t.response_header - tx->t.request_started);
    }
    else if (tx->t.response_started > tx->t.request_started) {
        tx_time = IB_CLOCK_USEC_TO_MSEC(tx->t.response_started - tx->t.request_started);
    }
    else if (tx->t.request_finished > tx->t.request_started) {
        tx_time = IB_CLOCK_USEC_TO_MSEC(tx->t.request_finished - tx->t.request_started);
    }
    else if (tx->t.request_body > tx->t.request_started) {
        tx_time = IB_CLOCK_USEC_TO_MSEC(tx->t.request_body - tx->t.request_started);
    }
    else if (tx->t.request_header > tx->t.request_started) {
        tx_time = IB_CLOCK_USEC_TO_MSEC(tx->t.request_header - tx->t.request_started);
    }
    else if (tx->t.request_started > tx->t.started) {
        tx_time = IB_CLOCK_USEC_TO_MSEC(tx->t.request_started - tx->t.started);
    }


    /* Log Format */
    log_format = ib_mm_strdup(mm, CORE_AUDITLOG_FORMAT);
    if (log_format == NULL) {
        return IB_EALLOC;
    }

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, mm);
    if (rc != IB_OK) {
        return rc;
    }

    ib_field_create_bytestr_alias(&f, mm,
                                  IB_S2SL("conn-id"),
                                  (uint8_t *)tx->conn->id,
                                  strlen(tx->conn->id));
    ib_list_push(list, f);

    ib_field_create(&f, mm,
                    IB_S2SL("tx-num"),
                    IB_FTYPE_NUM,
                    ib_ftype_num_in(&tx_num));
    ib_list_push(list, f);

    ib_field_create(&f, mm,
                    IB_S2SL("tx-time"),
                    IB_FTYPE_NUM,
                    ib_ftype_num_in(&tx_time));
    ib_list_push(list, f);

    ib_list_t *events;

    ib_field_create_bytestr_alias(&f, mm,
                                  IB_S2SL("tx-id"),
                                  (uint8_t *)tx->id,
                                  strlen(tx->id));
    ib_list_push(list, f);

    /* Add all unsuppressed alert event tags as well
     * as the last alert message and action. */
    rc = ib_logevent_get_all(tx, &events);
    if (rc == IB_OK) {
        ib_list_node_t *enode;
        ib_field_t *tx_action;
        ib_field_t *tx_msg;
        ib_field_t *tx_tags;
        ib_field_t *tx_threat_level;
        ib_num_t threat_level = 0;
        bool do_threat_calc = true;
        int num_events = 0;

        ib_field_create(&tx_action, mm,
                        IB_S2SL("tx-action"),
                        IB_FTYPE_NULSTR,
                        NULL);

        ib_field_create(&tx_msg, mm,
                        IB_S2SL("tx-msg"),
                        IB_FTYPE_NULSTR,
                        NULL);

        ib_field_create(&tx_threat_level, mm,
                        IB_S2SL("tx-threatlevel"),
                        IB_FTYPE_NUM,
                        NULL);

        ib_field_create(&tx_tags, mm,
                        IB_S2SL("tx-tags"),
                        IB_FTYPE_LIST,
                        NULL);

        /* Determine transaction action (block/log) via flags. */
        if (ib_flags_any(tx->flags, IB_TX_FBLOCK_PHASE|IB_TX_FBLOCK_IMMEDIATE)) {
            ib_field_setv(tx_action, ib_ftype_nulstr_in(
                ib_logevent_action_name(IB_LEVENT_ACTION_BLOCK))
            );
        }
        else {
            ib_field_setv(tx_action, ib_ftype_nulstr_in(
                ib_logevent_action_name(IB_LEVENT_ACTION_LOG))
            );
        }

        /* Check if THREAT_LEVEL is available, or if we need to calculate
         * it here.
         */
        rc = ib_var_source_get_const(
            cfg->core_cfg->vars->threat_level,
            &threat_level_f,
            tx->var_store
        );
        if ((rc == IB_OK) && (threat_level_f->type == IB_FTYPE_NUM)) {
            rc = ib_field_value(
                    threat_level_f,
                    ib_ftype_num_out(&threat_level)
            );
            if (rc == IB_OK) {
                ib_log_debug_tx(tx, "Using THREAT_LEVEL field as threat level value.");
                do_threat_calc = false;
            }
            else {
                ib_log_debug_tx(tx, "No numeric THREAT_LEVEL field to use as threat level value.");
            }
        }
        else {
            ib_log_debug_tx(tx, "No THREAT_LEVEL field to use as threat level value.");
        }

        /* It is more important to write out what is possible
         * than to fail here. So, some error codes are ignored.
         *
         * TODO: Simplify by not using collections
         */
        IB_LIST_LOOP(events, enode) {
            ib_logevent_t *e = (ib_logevent_t *)ib_list_node_data(enode);
            ib_list_node_t *tnode;

            /* Only unsuppressed. */
            if (   (e == NULL)
                || (e->suppress != IB_LEVENT_SUPPRESS_NONE))
            {
                continue;
            }

            if (do_threat_calc) {
                /* The threat_level is average severity. */
                if (e->severity > 0) {
                    threat_level += e->severity;
                    ++num_events;
                }
            }

            /* Only alerts. */
            if (e->type != IB_LEVENT_TYPE_ALERT) {
                continue;
            }

            /* Use the last event message, if there is one. */
            if ((e->msg != NULL) && (strlen(e->msg) > 0)) {
                ib_field_setv(tx_msg, ib_ftype_nulstr_in(e->msg));
            }

            IB_LIST_LOOP(e->tags, tnode) {
                char *tag = (char *)ib_list_node_data(tnode);

                if (tag != NULL) {
                    ib_field_create(&f, mm,
                                    IB_S2SL("tag"),
                                    IB_FTYPE_NULSTR,
                                    ib_ftype_nulstr_in(tag));
                    ib_field_list_add(tx_tags, f);
                }
            }
        }

        /* Use the average threat level. */
        if ((do_threat_calc) && (num_events > 0)) {
            threat_level /= num_events;
        }
        ib_field_setv(tx_threat_level, ib_ftype_num_in(&threat_level));

        ib_list_push(list, tx_action);
        ib_list_push(list, tx_msg);
        ib_list_push(list, tx_tags);
        ib_list_push(list, tx_threat_level);
    }

    ib_field_create_bytestr_alias(&f, mm,
                                  IB_S2SL("log-timestamp"),
                                  (uint8_t *)tstamp,
                                  strlen(tstamp));
    ib_list_push(list, f);

    /* TODO: This probably will be removed in the near future. */
    ib_field_create_bytestr_alias(&f, mm,
                                  IB_S2SL("log-format"),
                                  (uint8_t *)log_format,
                                  strlen(log_format));
    ib_list_push(list, f);

    ib_field_create_bytestr_alias(&f, mm,
                                  IB_S2SL("log-id"),
                                  (uint8_t *)cfg->boundary,
                                  strlen(cfg->boundary));
    ib_list_push(list, f);

    ib_field_create_bytestr_alias(&f, mm,
                                  IB_S2SL("sensor-id"),
                                  (uint8_t *)ib->sensor_id,
                                  strlen(ib->sensor_id));
    ib_list_push(list, f);

    ib_field_create_bytestr_alias(&f, mm,
                                  IB_S2SL("sensor-name"),
                                  (uint8_t *)ib->sensor_name,
                                  strlen(ib->sensor_name));
    ib_list_push(list, f);

    ib_field_create_bytestr_alias(&f, mm,
                                  IB_S2SL("sensor-version"),
                                  (uint8_t *)ib->sensor_version,
                                  strlen(ib->sensor_version));
    ib_list_push(list, f);

    ib_field_create_bytestr_alias(&f, mm,
                                  IB_S2SL("sensor-hostname"),
                                  (uint8_t *)ib->sensor_hostname,
                                  strlen(ib->sensor_hostname));
    ib_list_push(list, f);

    rc = ib_context_site_get(log->ctx, &site);
    if (rc != IB_OK) {
        return rc;
    }
    if (site != NULL) {
        ib_field_create_bytestr_alias(&f, mm,
                                      IB_S2SL("site-id"),
                                      (uint8_t *)site->id,
                                      strlen(site->id));
        ib_list_push(list, f);

        ib_field_create_bytestr_alias(&f, mm,
                                      IB_S2SL("site-name"),
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

    return rc;
}

static ib_status_t ib_auditlog_add_part_events(ib_auditlog_t *log)
{
    ib_list_t *list;
    ib_status_t rc;

    /* Get the list of events. */
    rc = ib_logevent_get_all(log->tx, &list);
    if (rc != IB_OK) {
        return rc;
    }

    /* Add the part to the auditlog. */
    rc = ib_auditlog_part_add(log,
                              "events",
                              "application/json",
                              list,
                              ib_auditlog_gen_json_events,
                              NULL);

    return rc;
}

static ib_status_t ib_auditlog_add_part_http_request_meta(ib_auditlog_t *log)
{
    ib_tx_t *tx = log->tx;
    ib_mm_t mm = log->mm;
    ib_field_t *f;
    ib_list_t *list;
    char *tstamp;
    ib_status_t rc;
    const ib_core_audit_cfg_t *core_audit_cfg =
        (const ib_core_audit_cfg_t *)log->cfg_data;
    const ib_core_cfg_t *core_cfg = core_audit_cfg->core_cfg;

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, mm);
    if (rc != IB_OK) {
        return rc;
    }

    if (tx != NULL) {
        ib_var_store_t *var_store = tx->var_store;
        ib_num_t num;

        /* Timestamp */
        tstamp = (char *)ib_mm_alloc(mm, 30);
        if (tstamp == NULL) {
            return IB_EALLOC;
        }
        ib_clock_relative_timestamp(tstamp, &tx->tv_created,
                                    (tx->t.request_started - tx->t.started));

        ib_field_create_bytestr_alias(&f, mm,
                                      IB_S2SL("request-timestamp"),
                                      (uint8_t *)tstamp,
                                      strlen(tstamp));
        ib_list_push(list, f);

        ib_field_create_bytestr_alias(&f, mm,
                                      IB_S2SL("remote-addr"),
                                      (uint8_t *)tx->remote_ipstr,
                                      strlen(tx->remote_ipstr));
        ib_list_push(list, f);

        num = tx->conn->remote_port;
        ib_field_create(&f, mm,
                        IB_S2SL("remote-port"),
                        IB_FTYPE_NUM,
                        ib_ftype_num_in(&num));
        ib_list_push(list, f);

        ib_field_create_bytestr_alias(&f, mm,
                                      IB_S2SL("local-addr"),
                                      (uint8_t *)tx->conn->local_ipstr,
                                      strlen(tx->conn->local_ipstr));
        ib_list_push(list, f);

        num = tx->conn->local_port;
        ib_field_create(&f, mm,
                        IB_S2SL("local-port"),
                        IB_FTYPE_NUM,
                        ib_ftype_num_in(&num));
        ib_list_push(list, f);

        /// @todo If this is NULL, parser failed - what to do???
        if (tx->path != NULL) {
            ib_field_create_bytestr_alias(&f, mm,
                                          IB_S2SL("request-uri-path"),
                                          (uint8_t *)tx->path,
                                          strlen(tx->path));
            ib_list_push(list, f);
        }

        rc = ib_var_source_get(
            core_cfg->vars->request_protocol,
            &f,
            var_store
        );
        if (rc == IB_OK) {
            ib_list_push(list, f);
        }
        else {
            ib_log_notice_tx(tx, "Failed to get request_protocol: %s",
                             ib_status_to_string(rc));
        }

        rc = ib_var_source_get(
            core_cfg->vars->request_method,
            &f,
            var_store
        );
        if (rc == IB_OK) {
            ib_list_push(list, f);
        }
        else {
            ib_log_notice_tx(tx, "Failed to get request_method: %s",
                             ib_status_to_string(rc));
        }

        /// @todo If this is NULL, parser failed - what to do???
        if (tx->hostname != NULL) {
            ib_field_create_bytestr_alias(&f, mm,
                                          IB_S2SL("request-hostname"),
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

    return rc;
}

static ib_status_t ib_auditlog_add_part_http_response_meta(ib_auditlog_t *log)
{
    ib_tx_t *tx = log->tx;
    ib_mm_t mm = log->mm;
    ib_field_t *f;
    ib_list_t *list;
    char *tstamp;
    const ib_core_audit_cfg_t *core_audit_cfg =
        (const ib_core_audit_cfg_t *)log->cfg_data;
    const ib_core_cfg_t *core_cfg = core_audit_cfg->core_cfg;
    ib_var_store_t *var_store = tx->var_store;
    ib_status_t rc;

    /* Timestamp */
    tstamp = (char *)ib_mm_alloc(mm, 30);
    if (tstamp == NULL) {
        return IB_EALLOC;
    }
    ib_clock_relative_timestamp(tstamp, &tx->tv_created,
                                (tx->t.response_started - tx->t.started));

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, mm);
    if (rc != IB_OK) {
        return rc;
    }

    ib_field_create_bytestr_alias(&f, mm,
                                  IB_S2SL("response-timestamp"),
                                  (uint8_t *)tstamp,
                                  strlen(tstamp));
    ib_list_push(list, f);

    rc = ib_var_source_get(core_cfg->vars->response_status, &f, var_store);
    if (rc == IB_OK) {
        ib_list_push(list, f);
    }
    else {
        ib_log_notice_tx(tx, "Failed to get response_status: %s",
                         ib_status_to_string(rc));
    }

    rc = ib_var_source_get(
            core_cfg->vars->response_protocol,
            &f,
            var_store
    );
    if (rc == IB_OK) {
        ib_list_push(list, f);
    }
    else {
        ib_log_notice_tx(tx, "Failed to get response_protocol: %s",
                         ib_status_to_string(rc));
    }

    /* Add the part to the auditlog. */
    rc = ib_auditlog_part_add(log,
                              "http-response-metadata",
                              "application/json",
                              list,
                              ib_auditlog_gen_json_flist,
                              NULL);

    return rc;
}

/**
 * Add request/response header fields to the audit log
 *
 * @param[in] tx Transaction
 * @param[in] mm Memory manager to user for allocations
 * @param[in,out] list List to add the fields to
 * @param[in] label Label string ("request"/"response")
 * @param[in] header  Parsed header fields data
 *
 * @return Status code
 */
static ib_status_t ib_auditlog_add_part_http_head_fields(
    ib_tx_t *tx,
    ib_mm_t mm,
    ib_list_t *list,
    const char *label,
    ib_parsed_headers_t *header )
{
    ib_parsed_header_t *nvpair;
    ib_status_t rc;
    ib_field_t *f;

    /* Loop through all of the header name/value pairs */
    for (nvpair = header ->head;
         nvpair != NULL;
         nvpair = nvpair->next)
    {
        /* Create a field to hold the name/value pair. */
        rc = ib_field_create(&f, mm,
                             (char *)ib_bytestr_const_ptr(nvpair->name),
                             ib_bytestr_length(nvpair->name),
                             IB_FTYPE_BYTESTR,
                             ib_ftype_bytestr_mutable_in(nvpair->value));
        if (rc != IB_OK) {
            return rc;
        }

        /* Add the new field to the list */
        rc = ib_list_push(list, f);
        if (rc != IB_OK) {
            return rc;
        }
    }
    return IB_OK;
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
    ib_mm_t mm = log->mm;
    ib_tx_t *tx = log->tx;
    ib_list_t *list;
    ib_field_t *f;
    ib_status_t rc;

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Add the raw request line */
    // FIXME: Why would this be NULL?  Should this ever happen?
    if (tx->request_line != NULL) {
        rc = ib_field_create(&f, mm,
                             IB_S2SL("request_line"),
                             IB_FTYPE_BYTESTR,
                             tx->request_line->raw);
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_list_push(list, f);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Add the request header fields */
    if (tx->request_header != NULL) {
        rc = ib_auditlog_add_part_http_head_fields(tx, mm,
                                                   list, "request",
                                                   tx->request_header);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Add the part to the auditlog. */
    rc = ib_auditlog_part_add(log,
                              "http-request-header",
                              "application/octet-stream",
                              list,
                              ib_auditlog_gen_header_flist,
                              NULL);

    return rc;
}

static ib_status_t ib_auditlog_add_part_http_request_body(ib_auditlog_t *log)
{
    ib_tx_t *tx = log->tx;
    ib_status_t rc;

    rc = ib_auditlog_part_add(log,
                              "http-request-body",
                              "application/octet-stream",
                              tx->request_body,
                              ib_auditlog_gen_raw_stream,
                              NULL);

    return rc;
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
    ib_mm_t mm = log->mm;
    ib_tx_t *tx = log->tx;
    ib_list_t *list;
    ib_field_t *f;
    ib_status_t rc;

    /* Generate a list of fields in this part. */
    rc = ib_list_create(&list, mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Add the raw response line
     *
     * The response_line may be NULL for HTTP/0.9 requests.
     */
    if (tx->response_line != NULL) {
        rc = ib_field_create(&f, mm,
                             IB_S2SL("response_line"),
                             IB_FTYPE_BYTESTR,
                             tx->response_line->raw);
        if (rc != IB_OK) {
            return rc;
        }
        rc = ib_list_push(list, f);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Add the response header fields */
    if (tx->response_header != NULL) {
        rc = ib_auditlog_add_part_http_head_fields(tx, mm,
                                                   list, "response",
                                                   tx->response_header);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Add the part to the auditlog. */
    rc = ib_auditlog_part_add(log,
                              "http-response-header",
                              "application/octet-stream",
                              list,
                              ib_auditlog_gen_header_flist,
                              NULL);

    return rc;
}

static ib_status_t ib_auditlog_add_part_http_response_body(ib_auditlog_t *log)
{
    ib_tx_t *tx = log->tx;
    ib_status_t rc;

    rc = ib_auditlog_part_add(log,
                              "http-response-body",
                              "application/octet-stream",
                              tx->response_body,
                              ib_auditlog_gen_raw_stream,
                              NULL);

    return rc;
}

/**
 * Handle writing the logevents that audit IronBee.
 *
 * This should occur in the post processing phase. If this is moved
 * earlier then, for example, the rule engine's logging of
 * events will not be audited.
 *
 * @param ib Engine.
 * @param tx Transaction.
 * @param event Event type.
 * @param cbdata Callback data.
 *
 * @returns Status code.
 */
static ib_status_t auditing_hook(ib_engine_t *ib,
                                 ib_tx_t *tx,
                                 ib_state_event_type_t event,
                                 void *cbdata)
{
    assert(event == handle_postprocess_event);

    ib_auditlog_t *log;
    ib_core_cfg_t *corecfg;
    ib_core_module_tx_data_t *core_txdata;
    ib_core_audit_cfg_t *cfg;
    ib_list_t *events;
    ib_status_t rc;

    /* If there's not events, do nothing */
    if (tx->logevents == NULL) {
        return IB_OK;
    }

    /* If the transaction never started, do nothing */
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_STARTED) ) {
        return IB_OK;
    }

    /* Get core tx module data. */
    rc = ib_tx_get_module_data(tx, ib_core_module(ib), &core_txdata);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_core_context_config(tx->ctx, &corecfg);
    if (rc != IB_OK) {
        return rc;
    }

    switch (corecfg->audit_engine) {
        /* Always On */
        case 1:
            break;
        /* Only if events are present */
        case 2:
            rc = ib_logevent_get_all(tx, &events);
            if (rc != IB_OK) {
                return rc;
            }
            if (ib_list_elements(events) == 0) {
                return IB_OK;
            }
            break;
        /* Anything else is Off */
        default:
            return IB_OK;
    }

    /* Mark time. */
    tx->t.logtime = ib_clock_get_time();

    /* Auditing */
    /// @todo Only create if needed
    log = (ib_auditlog_t *)ib_mm_calloc(tx->mm, 1, sizeof(*log));
    if (log == NULL) {
        return IB_EALLOC;
    }

    log->ib = ib;
    log->mm = tx->mm;
    log->ctx = tx->ctx;
    log->tx = tx;

    rc = ib_list_create(&log->parts, log->mm);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create a unique MIME tx->audit_log_id. */
    rc = ib_uuid_create_v4(tx->audit_log_id);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create the core config. */
    cfg = (ib_core_audit_cfg_t *)ib_mm_calloc(log->mm, 1, sizeof(*cfg));
    if (cfg == NULL) {
        return IB_EALLOC;
    }

    cfg->tx = tx;
    cfg->boundary = tx->audit_log_id;
    cfg->core_cfg = corecfg;
    log->cfg_data = cfg;

    /* Add all the parts to the log. */
    if (tx->auditlog_parts & IB_ALPART_HEADER) {
        ib_auditlog_add_part_header(log);
    }
    if (tx->auditlog_parts & IB_ALPART_EVENTS) {
        ib_auditlog_add_part_events(log);
    }
    if (tx->auditlog_parts & IB_ALPART_HTTP_REQUEST_METADATA) {
        ib_auditlog_add_part_http_request_meta(log);
    }
    if (tx->auditlog_parts & IB_ALPART_HTTP_RESPONSE_METADATA) {
        ib_auditlog_add_part_http_response_meta(log);
    }
    if (tx->auditlog_parts & IB_ALPART_HTTP_REQUEST_HEADER) {
        /* Only add if this was inspected. */
        if (ib_flags_all(tx->flags, IB_TX_FINSPECT_REQHDR)) {
            ib_auditlog_add_part_http_request_head(log);
        }
    }
    if (tx->auditlog_parts & IB_ALPART_HTTP_REQUEST_BODY) {
        /* Only add if this was inspected. */
        if (ib_flags_all(tx->flags, IB_TX_FINSPECT_REQBODY)) {
            ib_auditlog_add_part_http_request_body(log);
        }
    }
    if (tx->auditlog_parts & IB_ALPART_HTTP_RESPONSE_HEADER) {
        /* Only add if this was inspected. */
        if (ib_flags_all(tx->flags, IB_TX_FINSPECT_RESHDR)) {
            ib_auditlog_add_part_http_response_head(log);
        }
    }
    if (tx->auditlog_parts & IB_ALPART_HTTP_RESPONSE_BODY) {
        /* Only add if this was inspected. */
        if (ib_flags_all(tx->flags, IB_TX_FINSPECT_RESBODY)) {
            ib_auditlog_add_part_http_response_body(log);
        }
    }

    audit_write_log(ib, log);

    /* Events */
    ib_logevent_write_all(tx);

    return IB_OK;
}

/**
 * Handle the connection starting.
 *
 * Forward to ib_core_context_config().
 *
 * @param ib Engine.
 * @param conn Connection.
 * @param event Event type.
 * @param cbdata Callback data.
 *
 * @returns Status code.
 */
static ib_status_t core_hook_conn_started(ib_engine_t *ib,
                                          ib_conn_t *conn,
                                          ib_state_event_type_t event,
                                          void *cbdata)
{
    assert(event == conn_started_event);

    ib_core_cfg_t *corecfg;
    ib_status_t rc;

    rc = ib_core_context_config(conn->ctx, &corecfg);

    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to initialize core module: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    return IB_OK;
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
 * @param mm Memory manager
 * @param pflags Address which flags are written
 *
 * @returns Status code
 */
static ib_status_t filter_buffer(ib_filter_t *f,
                                 ib_fdata_t *fdata,
                                 ib_context_t *ctx,
                                 ib_mm_t mm,
                                 ib_flags_t *pflags)
{
    ib_stream_t *buf = (ib_stream_t *)fdata->state;
    ib_sdata_t *sdata;
    ib_status_t rc;

    if (buf == NULL) {
        fdata->state = ib_mm_calloc(mm, 1, sizeof(*buf));
        if (fdata->state == NULL) {
            return IB_EALLOC;
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
                    return rc;
                }
                break;
            }
            rc = ib_stream_pull(fdata->stream, &sdata);
        }
    }
    if (rc != IB_ENOENT) {
        return rc;
    }

    return IB_OK;
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
    assert(event == handle_context_tx_event);

    ib_status_t rc = IB_OK;

    /// @todo Need an API for this.
    tx->fctl->filters = tx->ctx->filters;
    tx->fctl->fbuffer = (ib_filter_t *)cbdata;
    ib_fctl_meta_add(tx->fctl, IB_STREAM_FLUSH);

    return rc;
}

/* -- Core Hook Handlers -- */

/**
 * Execute the InitVar directive to initialize field in a transaction's DPI
 *
 * @param[in] ib Engine.
 * @param[in] tx Transaction.
 * @param[in] initvar_list List of the initvar fields
 *
 * @returns Status code.
 */
static ib_status_t core_initvar(ib_engine_t *ib,
                                ib_tx_t *tx,
                                const ib_list_t *initvar_list)
{
    const ib_list_node_t *node;
    ib_status_t rc = IB_OK;

    if (initvar_list == NULL) {
        return IB_OK;
    }

    IB_LIST_LOOP_CONST(initvar_list, node) {
        ib_status_t trc; /* Temp RC */
        const ib_core_initvar_t *initvar =
            (const ib_core_initvar_t *)ib_list_node_data_const(node);
        ib_field_t *newf;

        trc = ib_field_copy(
            &newf,
            tx->mm,
            initvar->initial_value->name, initvar->initial_value->nlen,
            initvar->initial_value
        );
        if (trc != IB_OK) {
            ib_log_notice_tx(tx, "Failed to copy field: %s",
                             ib_status_to_string(trc));
            if (rc == IB_OK) {
                rc = trc;
            }
            continue;
        }

        trc = ib_var_source_set(
            initvar->source,
            tx->var_store,
            newf
        );
        if (trc != IB_OK) {
            if (rc == IB_OK) {
                rc = trc;
            }
        }
    }

    return rc;
}

/**
 * Handle the transaction context selected
 *
 * @param ib Engine.
 * @param tx Transaction.
 * @param event Event type.
 * @param cbdata Callback data.
 *
 * @returns Status code.
 */
static ib_status_t core_hook_context_tx(ib_engine_t *ib,
                                        ib_tx_t *tx,
                                        ib_state_event_type_t event,
                                        void *cbdata)
{
    assert(event == handle_context_tx_event);

    ib_core_cfg_t *corecfg;
    ib_core_module_tx_data_t *core_txdata;
    ib_status_t rc;

    rc = ib_core_context_config(tx->ctx, &corecfg);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx,
                        "Error accessing core module: %s",
                        ib_status_to_string(rc));
        return rc;
    }

    /* Set default options. */
    tx->flags |= corecfg->inspection_engine_options;
    tx->flags |= corecfg->protection_engine_options;

    /* Copy the configuration limits into the tx. */
    memcpy(&(tx->limits), &(corecfg->limits), sizeof(corecfg->limits));

    /* Copy config to transaction for potential runtime changes. */
    core_txdata =
        (ib_core_module_tx_data_t *)ib_mm_alloc(tx->mm,
                                                   sizeof(*core_txdata));
    if (core_txdata == NULL) {
        return IB_EALLOC;
    }
    core_txdata->auditlog_parts = corecfg->auditlog_parts;
    rc = ib_tx_set_module_data(tx, ib_core_module(ib), core_txdata);
    if (rc != IB_OK) {
        return rc;
    }

    /* Var Initialization */
    rc = ib_var_source_initialize(
        corecfg->vars->tx_capture,
        NULL,
        tx->var_store,
        IB_FTYPE_LIST
    );
    if (rc != IB_OK) {
        ib_log_alert_tx(tx, "Failed to initialize TX capture var.");
        return rc;
    }

    /* Handle InitVar list */
    rc = core_initvar(ib, tx, corecfg->initvar_list);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx, "Error executing InitVar(s): %s",
                        ib_status_to_string(rc));
        return rc;
    }

    return IB_OK;
}

static ib_status_t core_hook_request_body_data(ib_engine_t *ib,
                                               ib_tx_t *tx,
                                               ib_state_event_type_t event,
                                               const char *data,
                                               size_t data_length,
                                               void *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);

    ib_core_cfg_t *corecfg;
    void *data_copy;
    size_t data_copy_length;
    size_t limit;
    size_t remaining;
    ib_status_t rc;

    if ((data == NULL) || (data_length == 0)) {
        return IB_OK;
    }

    rc = ib_core_context_config(tx->ctx, &corecfg);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx,
                        "Error accessing core module: %s",
                        ib_status_to_string(rc));
        return rc;
    }

    /* Already at the limit? */
    limit = corecfg->limits.request_body_log_limit;
    if (tx->request_body->slen >= limit) {
        /* Already at the limit. */
        ib_log_debug_tx(tx,
                        "Request body log limit (%zd) reached: Ignoring %zd bytes.",
                        limit,
                        data_length);
        return IB_OK;
    }

    /* Check remaining space, adding only what will fit. */
    remaining = limit - tx->request_body->slen;
    if (remaining >= data_length) {
        data_copy = ib_mm_memdup(tx->mm, data, data_length);
        data_copy_length = data_length;
    }
    else {
        data_copy = ib_mm_memdup(tx->mm, data, remaining);
        data_copy_length = remaining;
    }

    rc = ib_stream_push(tx->request_body,
                        IB_STREAM_DATA,
                        data_copy,
                        data_copy_length);

    return rc;
}

static ib_status_t core_hook_response_body_data(ib_engine_t *ib,
                                                ib_tx_t *tx,
                                                ib_state_event_type_t event,
                                                const char *data,
                                                size_t data_length,
                                                void *cbdata)
{
    ib_core_cfg_t *corecfg;
    void *data_copy;
    size_t data_copy_length;
    size_t limit;
    size_t remaining;
    ib_status_t rc;

    if ((data == NULL) || (data_length == 0)) {
        return IB_OK;
    }

    rc = ib_core_context_config(tx->ctx, &corecfg);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx,
                        "Error accessing core module: %s",
                        ib_status_to_string(rc));
        return rc;
    }

    /* Already at the limit? */
    limit = corecfg->limits.response_body_log_limit;
    if (tx->response_body->slen >= limit) {
        /* Already at the limit. */
        ib_log_debug_tx(tx,
                        "Response body log limit (%zd) reached: Ignoring %zd bytes.",
                        limit,
                        data_length);
        return IB_OK;
    }

    /* Check remaining space, adding only what will fit. */
    remaining = limit - tx->response_body->slen;
    if (remaining >= data_length) {
        data_copy = ib_mm_memdup(tx->mm, data, data_length);
        data_copy_length = data_length;
    }
    else {
        data_copy = ib_mm_memdup(tx->mm, data, remaining);
        data_copy_length = remaining;
    }

    rc = ib_stream_push(tx->response_body,
                        IB_STREAM_DATA,
                        data_copy,
                        data_copy_length);

    return rc;
}

ib_status_t ib_core_module_data(
    ib_engine_t            *ib,
    ib_module_t           **core_module,
    ib_core_module_data_t **core_data)
{
    ib_module_t *module;

    /* Get the core module data */
    module = ib_core_module(ib);
    if (core_module != NULL) {
        *core_module = module;
    }

    if (core_data == NULL) {
        return IB_OK;
    }

    if (core_data != NULL) {
        *core_data = (ib_core_module_data_t *)module->data;
        if (*core_data == NULL) {
            return IB_EUNKNOWN;
        }
    }
    return IB_OK;
}

ib_status_t ib_core_context_config(const ib_context_t *ctx,
                                   ib_core_cfg_t **pcfg)
{
    assert(ctx != NULL);
    assert(pcfg != NULL);

    return ib_context_module_config(ctx,
                                    ib_core_module(ib_context_get_engine(ctx)),
                                    pcfg);
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
    ib_mm_t mm = ib_engine_mm_config_get(ib);

    *pabsfile = (char *)
        ib_mm_alloc(mm, strlen(basedir) + 1 + strlen(file) + 1);
    if (*pabsfile == NULL) {
        return IB_EALLOC;
    }

    strcpy(*pabsfile, basedir);
    strcat(*pabsfile, "/");
    strcat(*pabsfile, file);

    return IB_OK;
}

/**
 * Core: Create site
 *
 * @param[in] cp Configuration parser
 * @param[in,out] core_data Core module data
 * @param[in] site_name Site name string
 * @param[out] pctx Pointer to new context / NULL
 * @param[out] psite Pointer to new site object / NULL
 *
 * @returns Status code:
 * - IB_OK
 * - Errors from ib_context_create()
 * - Errors from ib_context_data_set()
 * - Errors from ib_cfgparser_context_push()
 */
static ib_status_t core_site_create(
    ib_cfgparser_t *cp,
    ib_core_module_data_t *core_data,
    const char *site_name,
    ib_context_t **pctx,
    ib_site_t **psite)
{
    assert(cp != NULL);
    assert(core_data != NULL);
    assert(site_name != NULL);

    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;
    ib_status_t rc;


    /* Create the site list if this is the first site */
    if (core_data->site_list == NULL) {
        rc = ib_list_create(&(core_data->site_list), cp->cur_ctx->mm);
        if (rc != IB_OK) {
            ib_log_error(ib, "Error creating core site list: %s",
                         ib_status_to_string(rc));
            return rc;
        }
    }

    /* Create the context */
    rc = ib_context_create(ib, cp->cur_ctx, IB_CTYPE_SITE,
                           "site", site_name, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error creating context for \"%s\": %s",
                         site_name, ib_status_to_string(rc));
        return IB_EINVAL;
    }
    core_data->cur_ctx = ctx;

    rc = ib_context_open(ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error opening context for \"%s\": %s",
                         site_name, ib_status_to_string(rc));
        return IB_EINVAL;
    }

    /* Create the site */
    rc = ib_ctxsel_site_create(ctx, site_name, psite);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error creating site \"%s\": %s",
                         site_name, ib_status_to_string(rc));
        return rc;
    }

    /* Store the site in the context */
    rc = ib_context_site_set(ctx, *psite);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to set site for context \"%s\": %s",
                         site_name, ib_status_to_string(rc));
        return rc;
    }

    if (pctx != NULL) {
        *pctx = ctx;
    }
    return IB_OK;
}

/**
 * Core: Site open
 *
 * @param[in] cp Configuration parser
 * @param[in] ctx Configuration context
 * @param[in,out] core_data Core module data
 * @param[in,out] site Site to open
 *
 * @returns Status code:
 * - IB_OK
 * - Errors from ib_context_config_set_parser()
 * - Errors from ib_cfgparser_context_push()
 * - Errors from ib_ctxsel_open()
 */
static ib_status_t core_site_open(ib_cfgparser_t *cp,
                                  ib_context_t *ctx,
                                  ib_core_module_data_t *core_data,
                                  ib_site_t *site)
{
    assert(cp != NULL);
    assert(site != NULL);
    ib_status_t rc;

    if (core_data->cur_site != NULL) {
        return IB_EUNKNOWN;
    }

    rc = ib_ctxsel_site_open(cp->ib, site);
    if (rc != IB_OK) {
        return rc;
    }

    core_data->cur_site = site;
    return rc;
}

/**
 * Core: Site close
 *
 * @param[in] cp Configuration parser
 * @param[in,out] core_data Core module data
 * @param[in,out] site Site to close
 *
 * @returns Status code:
 * - IB_OK
 * - Errors from ib_cfgparser_context_pop()
 * - Errors from ib_ctxsel_close()
 */
static ib_status_t core_site_close(
    ib_cfgparser_t *cp,
    ib_core_module_data_t *core_data,
    ib_site_t *site)
{
    assert(cp != NULL);
    assert(site != NULL);
    ib_status_t rc;
    ib_context_t *ctx;

    if (core_data->cur_site == NULL) {
        return IB_EUNKNOWN;
    }

    /* Verify that the current context matches the site context */
    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        goto done;
    }
    if (core_data->cur_ctx != ctx) {
        rc = IB_EUNKNOWN;
        goto done;
    }

    /* Close the site */
    rc = ib_ctxsel_site_close(cp->ib, site);
    if (rc != IB_OK) {
        goto done;
    }

    /* Close the context */
    rc = ib_context_close(ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error closing context for site \"%s\": %s",
                         site->name, ib_status_to_string(rc));
        goto done;
    }

    /* NULL the site pointer *after* closing the site */
done :
    core_data->cur_site = NULL;
    return rc;
}

/**
 * Core: Create a location for a site
 *
 * @param[in] cp Configuration parser
 * @param[in,out] core_data Core module data
 * @param[in] path Location path
 * @param[in,out] plocation Pointer to new location object
 *
 * @returns Status code:
 * - IB_OK
 * - Errors from ib_context_create()
 * - Errors from ib_context_data_set()
 * - Errors from ib_context_config_set_parser()
 * - Errors from ib_cfgparser_context_push()
 */
static ib_status_t core_location_create(
    ib_cfgparser_t *cp,
    ib_core_module_data_t *core_data,
    const char *path,
    ib_site_location_t **plocation)
{
    assert(cp != NULL);
    assert(core_data != NULL);
    assert(core_data->cur_site != NULL);
    assert(path != NULL);

    ib_status_t rc;
    ib_context_t *ctx;
    ib_site_t *site = core_data->cur_site;

    rc = ib_context_create(cp->ib, site->context, IB_CTYPE_LOCATION,
                           "location", path, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error creating location context for \"%s:%s\": %s",
                         site->name, path, ib_status_to_string(rc));
        return rc;
    }

    /* Store the site in the context */
    rc = ib_context_site_set(ctx, site);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error setting site for context \"%s\": %s",
                         ib_context_full_get(ctx), ib_status_to_string(rc));
        return rc;
    }

    /* Open the new context */
    rc = ib_context_open(ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error opening context for \"%s:%s\": %s",
                         site->name, path, ib_status_to_string(rc));
        return rc;
    }
    core_data->cur_ctx = ctx;

    /* Create the location object */
    rc = ib_ctxsel_location_create(site, ctx, path, plocation);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error creating location \"%s:%s\": %s",
                         site->name, path, ib_status_to_string(rc));
        return rc;
    }

    /* Store the site in the context */
    rc = ib_context_location_set(ctx, *plocation);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Error setting location for context \"%s\": %s",
                         ib_context_full_get(ctx), ib_status_to_string(rc));
        return rc;
    }

    return IB_OK;
}

/**
 * Core: Location open
 *
 * @param[in] cp Configuration parser
 * @param[in,out] core_data Core module data
 * @param[in,out] location Location to open
 *
 * @returns Status code:
 * - IB_OK
 * - IB_EUNKNOWN if current location is not NULL
 * - Errors from ib_ctxsel_location_open()
 */
static ib_status_t core_location_open(ib_cfgparser_t *cp,
                                      ib_core_module_data_t *core_data,
                                      ib_site_location_t *location)
{
    assert(cp != NULL);
    assert(location != NULL);

    ib_status_t rc;
    ib_core_cfg_t *site_cfg;
    ib_core_cfg_t *location_cfg;

    if (core_data->cur_location != NULL) {
        return IB_EUNKNOWN;
    }

    rc = ib_ctxsel_location_open(cp->ib, location);
    core_data->cur_location = location;
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_core_context_config(location->site->context, &site_cfg);
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_core_context_config(location->context, &location_cfg);
    if (rc != IB_OK) {
        return rc;
    }

    /* Copy the InitVar list from the site context */
    if (site_cfg->initvar_list != NULL) {
        const ib_list_node_t *node;

        rc = ib_list_create(&(location_cfg->initvar_list),
                            location->context->mm);
        if (rc != IB_OK) {
            return rc;
        }
        IB_LIST_LOOP_CONST(site_cfg->initvar_list, node) {
            assert(node->data != NULL);
            rc = ib_list_push(location_cfg->initvar_list, node->data);
            if (rc != IB_OK) {
                return rc;
            }
        }
    }

    return rc;
}

/**
 * Core: Location close
 *
 * @param[in] cp Configuration parser
 * @param[in,out] core_data Core module data
 * @param[in,out] location Location to close
 *
 * @returns Status code:
 * - IB_OK
 * - IB_EUNKNOWN if current location is not NULL
 * - Errors from ib_ctxsel_location_open()
 */
static ib_status_t core_location_close(ib_cfgparser_t *cp,
                                       ib_core_module_data_t *core_data,
                                       ib_site_location_t *location)
{
    assert(cp != NULL);
    assert(core_data != NULL);
    assert(location != NULL);

    ib_status_t rc;
    ib_context_t *ctx;

    if (core_data->cur_location == NULL) {
        return IB_EUNKNOWN;
    }

    /* Verify that the context matches */
    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        goto done;
    }
    if (core_data->cur_ctx != ctx) {
        rc = IB_EUNKNOWN;
        goto done;
    }

    /* Close the context */
    rc = ib_context_close(ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error closing context \"%s\": %s",
                         ib_context_full_get(ctx), ib_status_to_string(rc));
        goto done;
    }

    /* After closing the context, store the current one */
    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        goto done;
    }
    core_data->cur_ctx = ctx;

    /* Close the location */
    rc = ib_ctxsel_location_close(cp->ib, location);

done:
    core_data->cur_location = NULL;
    return rc;
}

/**
 * Handle the start of a Site block.
 *
 * This function sets up the new site and pushes it onto the parser stack.
 *
 * @param cp Config parser
 * @param dir_name Directive name
 * @param p1 First parameter
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_site_start(ib_cfgparser_t *cp,
                                       const char *dir_name,
                                       const char *p1,
                                       void *cbdata)
{
    assert(cp != NULL);

    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;
    ib_status_t rc;
    char *site_name;
    ib_site_t *site;
    ib_core_module_data_t *core_data;

    assert(ib != NULL);
    assert(ib->mp != NULL);
    assert(p1 != NULL);

    /* Get core module data */
    rc = ib_core_module_data(cp->ib, NULL, &core_data);
    if (rc != IB_OK) {
        return rc;
    }

    /* Checks */
    if (core_data->cur_site != NULL) {
        ib_cfg_log_error(cp, "Nested site block in site \"%s\"",
                         core_data->cur_site->name);
        return IB_EINVAL;
    }

    /* Unescape the parameter */
    rc = core_unescape(ib, &site_name, p1);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create and open the site object */
    rc = core_site_create(cp, core_data, site_name, &ctx, &site);
    if (rc != IB_OK) {
        return rc;
    }

    rc = core_site_open(cp, ctx, core_data, site);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error opening site \"%s\": %s",
                         site_name, ib_status_to_string(rc));
        return rc;
    }

    return rc;
}

/**
 * Handle the end of a Site block.
 *
 * This function closes out the site and pops it from the parser stack.
 *
 * @param cp Config parser
 * @param dir_name Directive name
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_site_end(ib_cfgparser_t *cp,
                                     const char *dir_name,
                                     void *cbdata)
{
    assert( cp != NULL );
    assert( cp->ib != NULL );
    assert( dir_name != NULL );

    ib_status_t rc;
    ib_core_module_data_t *core_data;
    const char *site_name;

    /* Get core module data */
    rc = ib_core_module_data(cp->ib, NULL, &core_data);
    if (rc != IB_OK) {
        return rc;
    }

    if (core_data->cur_site == NULL) {
        ib_cfg_log_error(cp, "Site end with no open site block.");
        return IB_EINVAL;
    }
    site_name = core_data->cur_site->name;

    rc = core_site_close(cp, core_data, core_data->cur_site);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error closing site \"%s\": %s",
                         site_name, ib_status_to_string(rc));
        return IB_EINVAL;
    }

    return IB_OK;
}

/**
 * Handle the start of a Location block.
 *
 * This function sets up the new location and pushes it onto the parser stack.
 *
 * @param cp Config parser
 * @param dir_name Directive name
 * @param p1 First parameter
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_loc_start(ib_cfgparser_t *cp,
                                      const char *dir_name,
                                      const char *p1,
                                      void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);

    ib_engine_t *ib = cp->ib;
    ib_site_t *site;
    ib_site_location_t *location;
    ib_status_t rc;
    char *path;
    ib_core_module_data_t *core_data;

    assert(dir_name != NULL);
    assert(p1 != NULL);

    /* Get core module data */
    rc = ib_core_module_data(cp->ib, NULL, &core_data);
    if (rc != IB_OK) {
        return rc;
    }

    /* Check that we're in a site, and not in a location */
    site = core_data->cur_site;
    if (site == NULL) {
        ib_cfg_log_error(cp, "%s directive must be within a site block.", dir_name);
        return IB_EINVAL;
    }
    if (core_data->cur_location != NULL) {
        ib_cfg_log_error(cp, "Nested location block in location \"%s:%s\"",
                         site->name,
                         core_data->cur_location->path);
        return IB_EINVAL;
    }

    rc = core_unescape(ib, &path, p1);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create and open the location object */
    rc = core_location_create(cp, core_data, path, &location);
    if (rc != IB_OK) {
        return rc;
    }
    rc = core_location_open(cp, core_data, location);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
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
    ib_status_t rc;
    ib_core_module_data_t *core_data;

    /* Get core module data */
    rc = ib_core_module_data(cp->ib, NULL, &core_data);
    if (rc != IB_OK) {
        return rc;
    }

    if (core_data->cur_location == NULL) {
        ib_cfg_log_error(cp, "Location end with no open location block.");
        return IB_EINVAL;
    }

    rc = core_location_close(cp, core_data, core_data->cur_location);
    if (rc != IB_OK) {
        return IB_EINVAL;
    }
    core_data->cur_location = NULL;

    return IB_OK;
}

/**
 * Handle the site-specific directives
 *
 * @param cp Config parser
 * @param directive Directive name
 * @param vars The list of variables passed to @a directive.
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_site_list(ib_cfgparser_t *cp,
                                      const char *directive,
                                      const ib_list_t *vars,
                                      void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(directive != NULL);
    assert(vars != NULL);

    ib_engine_t *ib = cp->ib;
    ib_status_t rc;
    const ib_list_node_t *node;
    const char *param1;
    const char *param1u;
    ib_core_module_data_t *core_data;
    ib_site_t *site;

    /* Get core module data */
    rc = ib_core_module_data(cp->ib, NULL, &core_data);
    if (rc != IB_OK) {
        return rc;
    }

    /* Get the first parameter */
    node = ib_list_first_const(vars);
    if (node == NULL) {
        ib_cfg_log_error(cp, "No %s specified for \"%s\" directive.",
                         directive, directive);
        return IB_EINVAL;
    }
    param1 = (const char *)node->data;

    /* Verify that we are in a site */
    if (core_data->cur_site == NULL) {
        ib_cfg_log_error(cp, "No site for %s directive.", directive);
        return IB_EINVAL;
    }
    site = core_data->cur_site;

    /* We remove constness to populate this buffer. */
    rc = core_unescape(ib, (char**)&param1u, param1);
    if (rc != IB_OK) {
        return rc;
    }

    /* Now, look at the parameter name */
    if (strcasecmp("SiteId", directive) == 0) {

        if (strlen(param1u) != IB_UUID_LENGTH - 1) {
            ib_cfg_log_error(cp,
                             "Invalid UUID at %s: %s should have UUID format "
                             "(xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx "
                             "where x are hex values)",
                             directive, param1u);

            /* Use the default id. */
            memcpy(site->id, ib_uuid_default_str, IB_UUID_LENGTH);

            return rc;
        }

        memcpy(site->id, param1u, IB_UUID_LENGTH);

        return IB_OK;
    }
    else if (strcasecmp("Hostname", directive) == 0) {
        const char *ip = "*";
        const char *port = NULL;
        bool service_specified = false;

        rc = ib_ctxsel_host_create(site, param1u, NULL);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "%s: Invalid hostname \"%s\" for site \"%s\".",
                             directive, param1u, site->id);
            return rc;
        }

        /* Handle ip= and port= for backward compatibility */
        while( (node = ib_list_node_next_const(node)) != NULL) {
            const char *param = (const char *)node->data;
            const char *unescaped;

            rc = core_unescape(ib, (char**)&unescaped, param);
            if ( rc != IB_OK ) {
                return rc;
            }

            if (strncasecmp(unescaped, "ip=", 3) == 0) {
                ip = unescaped+3;
                if (*ip == '\0') {
                    ip = "*";
                }
            }
            else if (strncasecmp(unescaped, "port=", 5) == 0) {
                port = unescaped+5;
                if (*port == '\0') {
                    port = NULL;
                }
            }
            else {
                ib_cfg_log_error(cp, "Unhandled %s parameter: \"%s\"",
                                 directive, unescaped);
                return IB_EINVAL;
            }
            service_specified = true;
        }

        if (! service_specified) {
            return IB_OK;
        }

        if (port == NULL) {
            rc = ib_ctxsel_service_create(site, ip, NULL);
            if (rc != IB_OK) {
                ib_cfg_log_error(cp, "%s: Invalid port=\"%s\" for site \"%s\"",
                                 directive, param1u, site->id);
                return rc;
            }
        }
        else {
            size_t len = strlen(ip) + 1 + strlen(port) + 1;
            char *service = (char *)ib_mm_alloc(cp->mm, len);
            if (service == NULL) {
                return IB_EALLOC;
            }

            strcpy(service, ip);
            strcat(service, ":");
            strcat(service, port);
            rc = ib_ctxsel_service_create(site, service, NULL);
            if (rc != IB_OK) {
                ib_cfg_log_error(cp,
                                 "%s: Invalid service \"%s\" for site \"%s\"",
                                 directive, service, site->id);
                return rc;
            }
        }

        return IB_OK;
    }
    else if (strcasecmp("Service", directive) == 0) {
        rc = ib_ctxsel_service_create(site, param1u, NULL);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "%s: Invalid service \"%s\" for site \"%s\"",
                             directive, param1u, site->id);
            return rc;
        }

        return IB_OK;
    }

    ib_cfg_log_error(cp, "Unhandled directive: %s \"%s\"", directive, param1u);

    return IB_EINVAL;
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
    assert(cp != NULL);
    assert(cp->ib != NULL);

    ib_engine_t *ib = cp->ib;
    ib_status_t rc;
    ib_core_cfg_t *corecfg;
    const char *p1_unescaped;
    ib_context_t *ctx;

    assert(name != NULL);
    assert(p1 != NULL);

    ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);

    /* We remove constness to populate this buffer. */
    rc = core_unescape(ib, (char**)&p1_unescaped, p1);
    if ( rc != IB_OK ) {
        return rc;
    }

    if (strcasecmp("InspectionEngine", name) == 0) {
        ib_log_notice(ib,
                      "TODO: Handle Directive: %s \"%s\"",
                      name, p1_unescaped);
    }
    else if (strcasecmp("AuditEngine", name) == 0) {
        if (strcasecmp("RelevantOnly", p1_unescaped) == 0) {
            rc = ib_context_set_num(
                ctx,
                "audit_engine",
                IB_AUDIT_MODE_RELEVANT);
            return rc;
        }
        else if (strcasecmp("On", p1_unescaped) == 0) {
            rc = ib_context_set_num(
                ctx,
                "audit_engine",
                IB_AUDIT_MODE_ON);
            return rc;
        }
        else if (strcasecmp("Off", p1_unescaped) == 0) {
            rc = ib_context_set_num(
                ctx,
                "audit_engine",
                IB_AUDIT_MODE_OFF);
            return rc;
        }

        ib_log_error(ib,
                     "Failed to parse directive: %s \"%s\"",
                     name,
                     p1_unescaped);
        return IB_EINVAL;
    }
    else if (strcasecmp("AuditLogIndex", name) == 0) {
        /* "None" means do not use the index file at all. */
        if (strcasecmp("None", p1_unescaped) == 0) {
            rc = ib_context_set_auditlog_index(ctx, false, NULL);
            return rc;
        }

        rc = ib_context_set_auditlog_index(ctx, true, p1_unescaped);
        return rc;
    }
    else if (strcasecmp("AuditLogIndexFormat", name) == 0) {
        rc = ib_context_set_string(ctx, "auditlog_index_fmt", p1_unescaped);
        return rc;
    }
    else if (strcasecmp("AuditLogDirMode", name) == 0) {
        long lmode = strtol(p1_unescaped, NULL, 0);

        if ((lmode > 0777) || (lmode <= 0)) {
            ib_log_error(ib, "Invalid mode: %s \"%s\"", name, p1_unescaped);
            return IB_EINVAL;
        }
        rc = ib_context_set_num(ctx, "auditlog_dmode", lmode);
        return rc;
    }
    else if (strcasecmp("AuditLogFileMode", name) == 0) {
        ib_num_t mode;
        rc = ib_string_to_num(p1_unescaped, 0, &mode);
        if ( (rc != IB_OK) || (mode > 0777) || (mode <= 0) ) {
            ib_log_error(ib, "Invalid mode: %s \"%s\"", name, p1_unescaped);
            return IB_EINVAL;
        }
        rc = ib_context_set_num(ctx, "auditlog_fmode", mode);
        return rc;
    }
    else if (strcasecmp("AuditLogBaseDir", name) == 0) {
        rc = ib_context_set_string(ctx, "auditlog_dir", p1_unescaped);
        return rc;
    }
    else if (strcasecmp("AuditLogSubDirFormat", name) == 0) {
        rc = ib_context_set_string(ctx, "auditlog_sdir_fmt", p1_unescaped);
        return rc;
    }
    /* Set the default block status for responding to blocked transactions. */
    else if (strcasecmp("DefaultBlockStatus", name) == 0) {
        int status;

        rc = ib_core_context_config(ctx, &corecfg);

        if (rc != IB_OK) {
            ib_log_error(ib,
                         "Could not set DefaultBlockStatus %s",
                         p1_unescaped);
            return rc;
        }

        status  = atoi(p1);

        if (status < 100 || status >= 600)
        {
            ib_log_error(
                ib,
                "DefaultBlockStatus status must be 100 <= status < 600: %d",
                status);
            return IB_EINVAL;
        }

        corecfg->block_status = status;
        return IB_OK;
    }
    else if (strcasecmp("BlockingMethod", name) == 0) {

        rc = ib_core_context_config(ctx, &corecfg);

        if (rc != IB_OK) {
            ib_log_error(ib,
                         "Could not set BlockingMethod: %s",
                         p1_unescaped);
            return rc;
        }

        if (!strcasecmp(p1, "close")) {
            corecfg->block_method = IB_BLOCK_METHOD_CLOSE;
        }
        /* The only argument is status=<int>.
         * Check for it. If OK, set status_str. */
        else if (strncasecmp(p1, "status=", sizeof("status=")-1) == 0) {
            int status;
            const char *status_str;

            status_str = p1 + sizeof("status=")-1;
            status  = atoi(status_str);

            if (status < 100 || status >= 600)
            {
                ib_log_error(
                    ib,
                    "BlockingMethod status must be 100 <= status < 600: %d",
                    status);
                return IB_EINVAL;
            }

            corecfg->block_status = status;
            corecfg->block_method = IB_BLOCK_METHOD_STATUS;
        }
        else {
            ib_log_error(
                ib,
                "Unrecognized parameter to directive \"%s\": \"%s\"",
                name,
                p1);
            return IB_EINVAL;
        }
        return IB_OK;
    }
    else if (strcasecmp("Log", name) == 0)
    {
        ib_mm_t       mm  = ib_engine_mm_main_get(ib);
        const char   *uri = NULL;

        /* Create a file URI from the file path, using memory
         * from the context's mem pool. */
        if ( strstr(p1_unescaped, "://") == NULL )  {
            char *buf = (char *)ib_mm_alloc( mm, 8+strlen(p1_unescaped) );
            if (buf == NULL) {
                return IB_EALLOC;
            }
            strcpy( buf, "file://" );
            strcat( buf, p1_unescaped );
            uri = buf;
        }
        else if ( strncmp(p1_unescaped, "file://", 7) != 0 ) {
            ib_log_error(ib,
                         "Unsupported URI in %s: \"%s\"",
                         name, p1_unescaped);
            return IB_EINVAL;
        }
        else {
            uri = p1_unescaped;
        }
        rc = ib_context_set_string(ctx, "logger.log_uri", uri);
        return rc;
    }
    else if (strcasecmp("LoadModule", name) == 0) {
        char *absfile;

        if (*p1_unescaped == '/') {
            absfile = (char *)p1_unescaped;
        }
        else {
            char *module_name;

            if ( strncmp(p1_unescaped, "ibmod_", 6) != 0 ) {
                /* Add "ibmod_" prefix and ".so" suffix (9 bytes, total) if not given. */
                ib_mm_t mm = ib_engine_mm_main_get(ib);
                size_t module_name_size = strlen(p1_unescaped) + 9 + 1;

                module_name = (char *)ib_mm_alloc(mm, module_name_size);
                if (module_name == NULL) {
                    return IB_EALLOC;
                }
                ib_log_debug(ib, "Expanding module name=%s to filename: %s",
                             p1_unescaped, module_name);
                snprintf(module_name, module_name_size, "ibmod_%s.so", p1_unescaped);
            }
            else {
                module_name = (char *)p1_unescaped;
            }

            rc = ib_core_context_config(ctx, &corecfg);
            if (rc != IB_OK) {
                return rc;
            }

            rc = core_abs_module_path(ib,
                                      corecfg->module_base_path,
                                      module_name, &absfile);
            if (rc != IB_OK) {
                return rc;
            }
        }

        rc = ib_module_load(ib, absfile);
        /* ib_module_load will report errors. */
        return rc;
    }
    else if (strcasecmp("RequestBuffering", name) == 0) {
        if (strcasecmp("On", p1_unescaped) == 0) {
            rc = ib_context_set_num(ctx, "buffer_req", 1);
            return rc;
        }

        rc = ib_context_set_num(ctx, "buffer_req", 0);
        return rc;
    }
    else if (strcasecmp("ResponseBuffering", name) == 0) {
        if (strcasecmp("On", p1_unescaped) == 0) {
            rc = ib_context_set_num(ctx, "buffer_res", 1);
            return rc;
        }

        rc = ib_context_set_num(ctx, "buffer_res", 0);
        return rc;
    }
    else if (strcasecmp("SensorId", name) == 0) {
        if (strlen(p1_unescaped) != IB_UUID_LENGTH - 1) {
            ib_log_error(ib, "Invalid UUID at %s: %s should have "
                         "UUID format "
                         "(xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx where x are"
                         " hex values)",
                         name, p1_unescaped);

            /* Use the default id. */
            memcpy(ib->sensor_id, ib_uuid_default_str, IB_UUID_LENGTH);

            return rc;
        }

        memcpy(ib->sensor_id, p1_unescaped, IB_UUID_LENGTH);

        return IB_OK;
    }
    else if (strcasecmp("SensorName", name) == 0) {
        ib->sensor_name = ib_mm_strdup(ib_engine_mm_config_get(ib),
                                       p1_unescaped);
        return IB_OK;
    }
    else if (strcasecmp("SensorHostname", name) == 0) {
        ib->sensor_hostname =
            ib_mm_strdup(ib_engine_mm_config_get(ib), p1_unescaped);
        return IB_OK;
    }
    else if (strcasecmp("ModuleBasePath", name) == 0) {
        rc = ib_core_context_config(ctx, &corecfg);

        if (rc != IB_OK) {
            ib_log_error(ib,
                         "Could not set ModuleBasePath %s", p1_unescaped);
            return rc;
        }

        corecfg->module_base_path = p1_unescaped;
        return IB_OK;
    }
    else if (strcasecmp("RuleBasePath", name) == 0) {
        rc = ib_core_context_config(ctx, &corecfg);

        if (rc != IB_OK) {
            ib_log_error(ib, "Could not set RuleBasePath %s", p1_unescaped);
            return rc;
        }

        corecfg->rule_base_path = p1_unescaped;
        return IB_OK;

    }
    else if (strcasecmp("RequestBodyBufferLimit", name) == 0) {
        rc = ib_core_context_config(ctx, &corecfg);
        if (rc != IB_OK) {
            ib_log_error(ib, "Could not fetch core module config.");
            return rc;
        }

        corecfg->limits.request_body_buffer_limit = atoll(p1_unescaped);
    }
    else if (strcasecmp("RequestBodyBufferLimitAction", name) == 0) {
        rc = ib_core_context_config(ctx, &corecfg);
        if (rc != IB_OK) {
            ib_log_error(ib, "Could not fetch core module config.");
            return rc;
        }

        if (strcasecmp(p1_unescaped, "FlushPartial") == 0) {
            corecfg->limits.request_body_buffer_limit_action =
                IB_BUFFER_LIMIT_ACTION_FLUSH_PARTIAL;
        }
        else if (strcasecmp(p1_unescaped, "FlushAll") == 0) {
            corecfg->limits.request_body_buffer_limit_action =
                IB_BUFFER_LIMIT_ACTION_FLUSH_ALL;
        }
        else {
            ib_cfg_log_error(cp, "Unknown limit action: %s", p1);
            return IB_EINVAL;
        }
    }
    else if (strcasecmp("ResponseBodyBufferLimit", name) == 0) {
        rc = ib_core_context_config(ctx, &corecfg);
        if (rc != IB_OK) {
            ib_log_error(ib, "Could not fetch core module config.");
            return rc;
        }

        corecfg->limits.response_body_buffer_limit = atoll(p1_unescaped);
    }
    else if (strcasecmp("ResponseBodyBufferLimitAction", name) == 0) {
        rc = ib_core_context_config(ctx, &corecfg);
        if (rc != IB_OK) {
            ib_log_error(ib, "Could not fetch core module config.");
            return rc;
        }

        if (strcasecmp(p1_unescaped, "FlushPartial") == 0) {
            corecfg->limits.response_body_buffer_limit_action =
                IB_BUFFER_LIMIT_ACTION_FLUSH_PARTIAL;
        }
        else if (strcasecmp(p1_unescaped, "FlushAll") == 0) {
            corecfg->limits.response_body_buffer_limit_action =
                IB_BUFFER_LIMIT_ACTION_FLUSH_ALL;
        }
        else {
            ib_cfg_log_error(cp, "Unknown limit action: %s", p1);
            return IB_EINVAL;
        }
    }
    else if (strcasecmp("ResponseBodyLogLimit", name) == 0) {
        rc = ib_core_context_config(ctx, &corecfg);
        if (rc != IB_OK) {
            ib_log_error(ib, "Could not fetch core module config.");
            return rc;
        }

        corecfg->limits.response_body_log_limit = atoll(p1_unescaped);
    }
    else if (strcasecmp("RequestBodyLogLimit", name) == 0) {
        rc = ib_core_context_config(ctx, &corecfg);
        if (rc != IB_OK) {
            ib_log_error(ib, "Could not fetch core module config.");
            return rc;
        }

        corecfg->limits.request_body_log_limit = atoll(p1_unescaped);
    }
    else {
        ib_log_error(ib, "Unhandled directive: %s %s", name, p1_unescaped);
        rc = IB_EINVAL;
    }
    return rc;
}

/**
 * Handle loglevel directives.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param p1 First parameter
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_loglevel(ib_cfgparser_t *cp,
                                     const char *name,
                                     const char *p1,
                                     void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(cbdata != NULL);

    ib_engine_t *ib = cp->ib;
    ib_status_t rc;
    const ib_strval_t *map = (const ib_strval_t *)cbdata;
    ib_context_t *ctx;
    ib_num_t level;
    long tmp = 0;

    assert(name != NULL);
    assert(p1 != NULL);

    ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);

    if (sscanf(p1, "%ld", &tmp) != 0) {
        level = tmp;
    }
    else {
        rc = ib_config_strval_pair_lookup(p1, map, &level);
        if (rc != IB_OK) {
            return IB_EUNKNOWN;
        }
    }

    if (strcasecmp("LogLevel", name) == 0)
    {
        ib_logger_level_set(ib_engine_logger_get(ib), level);
        return IB_OK;
    }
    else if (strcasecmp("RuleEngineLogLevel", name) == 0) {
        rc = ib_context_set_num(ctx, "rule_log_level", level);
        return rc;
    }

    ib_log_error(ib, "Unhandled directive: %s %s", name, p1);
    return IB_EINVAL;
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
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
    ib_num_t parts;
    ib_status_t rc;

    rc = ib_context_get(ctx, "auditlog_parts", ib_ftype_num_out(&parts), NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Merge the set flags with the previous value. */
    parts = ib_flags_merge(parts, flags, fmask);

    rc = ib_context_set_num(ctx, "auditlog_parts", parts);
    return rc;
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
static ib_status_t core_dir_rulelog_data(ib_cfgparser_t *cp,
                                         const char *name,
                                         ib_flags_t flags,
                                         ib_flags_t fmask,
                                         void *cbdata)
{
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
    ib_num_t tmp;
    ib_flags_t log_flags;
    ib_status_t rc;

    rc = ib_context_get(ctx, "rule_log_flags", ib_ftype_num_out(&tmp), NULL);
    if (rc != IB_OK) {
        return rc;
    }
    log_flags = tmp;

    /* Merge the set flags with the previous value. */
    log_flags = ib_flags_merge(log_flags, flags, fmask);

    rc = ib_context_set_num(ctx, "rule_log_flags", log_flags);
    return rc;
}

/**
 * Handle InspectionEngineOptions directive.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param flags Flags
 * @param fmask Flags mask (which bits were actually set)
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_inspection_engine_options(ib_cfgparser_t *cp,
                                                      const char *name,
                                                      ib_flags_t flags,
                                                      ib_flags_t fmask,
                                                      void *cbdata)
{
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
    ib_num_t options = 0;
    ib_status_t rc;

    rc = ib_context_get(ctx, "inspection_engine_options",
                        ib_ftype_num_out(&options), NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Merge the set flags with the previous value. */
    options = ib_flags_merge(options, flags, fmask);

    rc = ib_context_set_num(ctx, "inspection_engine_options", options);
    return rc;
}

/**
 * Handle ProtectionEngineOptions directive.
 *
 * @param cp Config parser
 * @param name Directive name
 * @param flags Flags
 * @param fmask Flags mask (which bits were actually set)
 * @param cbdata Callback data (from directive registration)
 *
 * @returns Status code
 */
static ib_status_t core_dir_protection_engine_options(ib_cfgparser_t *cp,
                                                      const char *name,
                                                      ib_flags_t flags,
                                                      ib_flags_t fmask,
                                                      void *cbdata)
{
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
    ib_num_t options = 0;
    ib_status_t rc;

    rc = ib_context_get(ctx, "protection_engine_options",
                        ib_ftype_num_out(&options), NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* Merge the set flags with the previous value. */
    options = ib_flags_merge(options, flags, fmask);

    rc = ib_context_set_num(ctx, "protection_engine_options", options);
    return rc;
}

/**
 * Perform any extra duties when certain config parameters are "Set".
 *
 * @param cp Config parser
 * @param ctx Context
 * @param type Config parameter type
 * @param name Config parameter name
 * @param val Config parameter value
 *
 * @returns Status code
 */
static ib_status_t core_set_value(ib_cfgparser_t *cp,
                                  ib_context_t *ctx,
                                  ib_ftype_t type,
                                  const char *name,
                                  const char *val)
{
    ib_engine_t *ib = ctx->ib;
    ib_core_cfg_t *corecfg;
    ib_status_t rc;

    /* Get the core module config. */
    rc = ib_core_context_config(ib->ctx, &corecfg);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to fetch ctx config for core module.");
        return rc;
    }

    if (strcasecmp("RuleEngineDebugLogLevel", name) == 0) {
        rc = ib_rule_engine_set(cp, name, val);
        if (rc != IB_OK) {
            return rc;
        }
    }

    else {
        return IB_EINVAL;
    }

    return IB_OK;
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
    ib_engine_t *ib = cp->ib;
    ib_status_t rc;

    if (strcasecmp("Set", name) == 0) {
        assert(cp->cur_ctx != NULL);
        ib_context_t *ctx = cp->cur_ctx;
        void *val;
        ib_ftype_t type;

        rc = ib_context_get(ctx, p1, &val, &type);
        if (rc != IB_OK) {
            return rc;
        }
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
                return IB_EINVAL;
        }

        rc = core_set_value(cp, ctx, type, p1, p2);
        if (rc == IB_EINVAL) {
            /* Core doesn't know about it; another module may. */
            return IB_OK;
        }
        else {
            return rc;
        }
    }

    ib_log_error(ib, "Unhandled directive: %s %s %s", name, p1, p2);
    return IB_EINVAL;
}

/**
 * Parse a InitVar directive.
 *
 * Register a InitVar directive to the engine.
 *
 * @param[in] cp Configuration parser
 * @param[in] directive The directive name.
 * @param[in] name 1st parameter to InitVar
 * @param[in] value 2nd parameter to InitVar
 * @param[in] cbdata User data. If non-NULL, will index key.
 */
static ib_status_t core_dir_initvar(ib_cfgparser_t *cp,
                                    const char *directive,
                                    const char *name,
                                    const char *value,
                                    void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(cp->cur_ctx != NULL);
    assert(directive != NULL);
    assert(name != NULL);
    assert(value != NULL);

    ib_status_t           rc;
    ib_engine_t          *ib = cp->ib;
    ib_mm_t               mm = cp->cur_ctx->mm;
    ib_core_cfg_t        *corecfg;
    const ib_field_t     *field;
    ib_var_source_t      *source;
    ib_core_initvar_t    *initvar;
    const char           *target;
    ib_list_t            *tfn_fields = NULL;
    ib_list_t            *tfn_insts = NULL;
    const ib_list_node_t *node;

    /* Get the core module config. */
    rc = ib_core_context_config(cp->cur_ctx, &corecfg);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to fetch config for core module.");
        return rc;
    }

    /* Initialize the fields list */
    if (corecfg->initvar_list == NULL) {
        rc = ib_list_create(&(corecfg->initvar_list), mm);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Error creating InitVar directive list: %s",
                             ib_status_to_string(rc));
            return rc;
        }
    }

    rc = ib_list_create(&tfn_fields, mm);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to create transformation list.");
        return rc;
    }

    rc = ib_cfg_parse_target_string(
        mm,
        value,
        &target,
        tfn_fields
    );
    if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "Failed to parse target string for InitVar: %s",
            value);
        return rc;
    }

    rc = ib_list_create(&tfn_insts, mm);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to create transformation instance list.");
        return rc;
    }

    rc = ib_rule_tfn_fields_to_inst(ib, mm, tfn_fields, tfn_insts);
    if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "Failed to build list of transformation instances.");
        return rc;
    }

    /* Check if target was allocated out of mp by ib_cfg_parse_target_string()
     * or just forwarded. If forwarded, a copy must be made because
     * we do not know who owns the memory used in "value" and it may go away. */
    if (target == value) {
        target = ib_mm_strdup(mm, value);
        if (target == NULL) {
            return IB_EALLOC;
        }
    }

    /* Create the field. Note: We remove the constness to create the field. */
    rc = ib_field_from_string(mm, IB_S2SL(name), target, (ib_field_t **)&field);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error creating field for InitVar: %s",
                         ib_status_to_string(rc));
        return rc;
    }

    /* Convert NULSTR created by ib_field_from_string to a BYTESTR. */
    if (field->type == IB_FTYPE_NULSTR) {
        ib_field_t *new_field = NULL;
        rc = ib_field_convert(mm, IB_FTYPE_BYTESTR, field, &new_field);
        if (rc != IB_OK) {
            ib_cfg_log_error(
                cp,
                "Error converting nulstr to bytestr field: %s",
                ib_status_to_string(rc));
        }
        field = (const ib_field_t *)new_field;
    }

    /* Apply all transformations. */
    IB_LIST_LOOP_CONST(tfn_insts, node) {
        const ib_field_t *tmp_field;
        const ib_transformation_inst_t *tfn_inst =
            (const ib_transformation_inst_t *)ib_list_node_data_const(node);

        rc = ib_transformation_inst_execute(tfn_inst, mm, field, &tmp_field);
        if (rc != IB_OK) {
            ib_cfg_log_error(
                cp,
                "Failed to run transformation %s for InitVar. "
                "Not initializing %s: %s",
                ib_transformation_name(
                    ib_transformation_inst_transformation(tfn_inst)
                ),
                name,
                ib_status_to_string(rc));
            /* As above, failure should not kill the whole config. */
            return IB_OK;
        }

        /* Promote the temporary field to the new current field. */
        field = tmp_field;
    }

    /* Register. */
    rc = ib_var_source_register(
        &source,
        ib_engine_var_config_get(cp->ib),
        name, strlen(name),
        IB_PHASE_NONE,
        IB_PHASE_NONE
    );
    if (rc == IB_EEXIST) {
        /* Acquire existing source. */
        rc = ib_var_source_acquire(
            &source,
            IB_MM_NULL,
            ib_engine_var_config_get(cp->ib),
            name, strlen(name)
        );
    }
    if (rc != IB_OK) {
        ib_cfg_log_error(
            cp,
            "Error sourcing InitVar %s: %s",
            name,
            ib_status_to_string(rc)
        );
        return rc;
    }

    /* Construct initvar */
    initvar = ib_mm_alloc(mm, sizeof(*initvar));
    if (initvar == NULL) {
        return IB_EALLOC;
    }
    initvar->source = source;
    initvar->initial_value = field;

    /* Add to the list */
    rc = ib_list_push(corecfg->initvar_list, initvar);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error pushing InitVar on list: %s",
                         ib_status_to_string(rc));
        return rc;
    }

    /* Done */
    return IB_OK;
}


/**
 * Mapping of valid debug log levels to numerical value
 */
static IB_STRVAL_MAP(core_loglevels_map) = {
    IB_STRVAL_PAIR("emergency", IB_LOG_EMERGENCY),
    IB_STRVAL_PAIR("alert", IB_LOG_ALERT),
    IB_STRVAL_PAIR("critical", IB_LOG_CRITICAL),
    IB_STRVAL_PAIR("error", IB_LOG_ERROR),
    IB_STRVAL_PAIR("warning", IB_LOG_WARNING),
    IB_STRVAL_PAIR("notice", IB_LOG_NOTICE),
    IB_STRVAL_PAIR("info", IB_LOG_INFO),
    IB_STRVAL_PAIR("debug", IB_LOG_DEBUG),
    IB_STRVAL_PAIR("debug2", IB_LOG_DEBUG2),
    IB_STRVAL_PAIR("debug3", IB_LOG_DEBUG3),
    IB_STRVAL_PAIR("trace", IB_LOG_TRACE),
    IB_STRVAL_PAIR_LAST
};

/**
 * Mapping of valid audit log part names to flag values.
 */
static IB_STRVAL_MAP(core_auditlog_parts_map) = {
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
 * Mapping of valid rule logging names to flag values.
 */
static IB_STRVAL_MAP(core_rulelog_flags_map) = {
    /* Rule log Flag Groups */
    IB_STRVAL_PAIR("none", 0),
    IB_STRVAL_PAIR("default", 0),
    IB_STRVAL_PAIR("all", IB_RULE_LOG_FLAGS_ALL),
    IB_STRVAL_PAIR("request", IB_RULE_LOG_FLAGS_REQUEST),
    IB_STRVAL_PAIR("response", IB_RULE_LOG_FLAGS_RESPONSE),
    IB_STRVAL_PAIR("ruleExec", IB_RULE_LOG_FLAGS_EXEC),

    /* Rule log Individual flags */
    IB_STRVAL_PAIR("tx", IB_RULE_LOG_FLAG_TX),
    IB_STRVAL_PAIR("requestLine", IB_RULE_LOG_FLAG_REQ_LINE),
    IB_STRVAL_PAIR("requestHeader", IB_RULE_LOG_FLAG_REQ_HEADER),
    IB_STRVAL_PAIR("requestBody", IB_RULE_LOG_FLAG_REQ_BODY),
    IB_STRVAL_PAIR("responseLine", IB_RULE_LOG_FLAG_RSP_LINE),
    IB_STRVAL_PAIR("responseHeader", IB_RULE_LOG_FLAG_RSP_HEADER),
    IB_STRVAL_PAIR("responseBody", IB_RULE_LOG_FLAG_RSP_BODY),
    IB_STRVAL_PAIR("phase", IB_RULE_LOG_FLAG_PHASE),
    IB_STRVAL_PAIR("rule", IB_RULE_LOG_FLAG_RULE),
    IB_STRVAL_PAIR("target", IB_RULE_LOG_FLAG_TARGET),
    IB_STRVAL_PAIR("transformation", IB_RULE_LOG_FLAG_TFN),
    IB_STRVAL_PAIR("operator", IB_RULE_LOG_FLAG_OPERATOR),
    IB_STRVAL_PAIR("action", IB_RULE_LOG_FLAG_ACTION),
    IB_STRVAL_PAIR("event", IB_RULE_LOG_FLAG_EVENT),
    IB_STRVAL_PAIR("audit", IB_RULE_LOG_FLAG_AUDIT),
    IB_STRVAL_PAIR("timing", IB_RULE_LOG_FLAG_TIMING),

    IB_STRVAL_PAIR("allRules", IB_RULE_LOG_FILT_ALL),
    IB_STRVAL_PAIR("actionableRulesOnly", IB_RULE_LOG_FILT_ACTIONABLE),
    IB_STRVAL_PAIR("operatorExecOnly", IB_RULE_LOG_FILT_OPEXEC),
    IB_STRVAL_PAIR("operatorErrorOnly", IB_RULE_LOG_FILT_ERROR),
    IB_STRVAL_PAIR("returnedTrueOnly", IB_RULE_LOG_FILT_TRUE),
    IB_STRVAL_PAIR("returnedFalseOnly", IB_RULE_LOG_FILT_FALSE),

    /* End */
    IB_STRVAL_PAIR_LAST
};

/**
 * Mapping of valid inspection engine options
 */
static IB_STRVAL_MAP(core_inspection_engine_options_map) = {
    /* Inspection Engine Options Groups */
    IB_STRVAL_PAIR("none", 0),
    IB_STRVAL_PAIR("all", IB_IEOPT_ALL),
    IB_STRVAL_PAIR("default", IB_IEOPT_DEFAULT),
    IB_STRVAL_PAIR("request", IB_IEOPT_REQUEST),
    IB_STRVAL_PAIR("response", IB_IEOPT_RESPONSE),

    /* Individual Inspection Engine Options */
    IB_STRVAL_PAIR("requestheader", IB_IEOPT_REQUEST_HEADER),
    IB_STRVAL_PAIR("requestbody", IB_IEOPT_REQUEST_BODY),
    IB_STRVAL_PAIR("requesturi", IB_IEOPT_REQUEST_URI),
    IB_STRVAL_PAIR("requestparams", IB_IEOPT_REQUEST_PARAMS),
    IB_STRVAL_PAIR("responseheader", IB_IEOPT_RESPONSE_HEADER),
    IB_STRVAL_PAIR("responsebody", IB_IEOPT_RESPONSE_BODY),

    /* End */
    IB_STRVAL_PAIR_LAST
};

/**
 * Mapping of valid protection engine options
 */
static IB_STRVAL_MAP(core_protection_engine_options_map) = {
    /* Protection Engine Options Groups */
    IB_STRVAL_PAIR("none", 0),
    IB_STRVAL_PAIR("all", IB_PEOPT_ALL),
    IB_STRVAL_PAIR("default", IB_PEOPT_DEFAULT),

    /* Individual Protection Engine Options */
    IB_STRVAL_PAIR("blockingmode", IB_PEOPT_BLOCKING_MODE),

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
    IB_DIRMAP_INIT_PARAM1(
        "RequestBodyBufferLimit",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "RequestBodyBufferLimitAction",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "ResponseBodyBufferLimit",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "ResponseBodyBufferLimitAction",
        core_dir_param1,
        NULL
    ),

    /* Blocking */
    IB_DIRMAP_INIT_PARAM1(
        "DefaultBlockStatus",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "BlockingMethod",
        core_dir_param1,
        NULL
    ),

    /* Logging */
    IB_DIRMAP_INIT_PARAM1(
        "LogLevel",
        core_dir_loglevel,
        core_loglevels_map
    ),
    IB_DIRMAP_INIT_PARAM1(
        "Log",
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
    IB_DIRMAP_INIT_LIST(
        "SiteId",
        core_dir_site_list,
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
        core_dir_site_list,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "Service",
        core_dir_site_list,
        NULL
    ),

    /* Inspection Engine */
    IB_DIRMAP_INIT_PARAM1(
        "InspectionEngine",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_OPFLAGS(
        "InspectionEngineOptions",
        core_dir_inspection_engine_options,
        NULL,
        core_inspection_engine_options_map
    ),

    /* Protection Engine */
    IB_DIRMAP_INIT_OPFLAGS(
        "ProtectionEngineOptions",
        core_dir_protection_engine_options,
        NULL,
        core_protection_engine_options_map
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
    IB_DIRMAP_INIT_PARAM1(
    "RequestBodyLogLimit",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
    "ResponseBodyLogLimit",
        core_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_OPFLAGS(
        "AuditLogParts",
        core_dir_auditlogparts,
        NULL,
        core_auditlog_parts_map
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

    /* Rule logging data */
    IB_DIRMAP_INIT_OPFLAGS(
        "RuleEngineLogData",
        core_dir_rulelog_data,
        NULL,
        core_rulelog_flags_map
    ),
    IB_DIRMAP_INIT_PARAM1(
        "RuleEngineLogLevel",
        core_dir_loglevel,
        core_loglevels_map
    ),

    /* TX DPI Initializers */
    IB_DIRMAP_INIT_PARAM2(
        "InitVar",
        core_dir_initvar,
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
    const char *file, const char *func, int line,
    const char *fmt, va_list ap
)
{
    ib_log_vex_ex((ib_engine_t *)ib, level, file, func, line, fmt, ap);

    return;
}

/**
 * Handle context open events for the core module
 *
 * @param[in] ib Engine
 * @param[in] ctx Context
 * @param[in] event Event triggering the callback
 * @param[in] cbdata Callback data (Module data)
 *
 * @returns Status code
 */
static ib_status_t core_ctx_open(ib_engine_t *ib,
                                 ib_context_t *ctx,
                                 ib_state_event_type_t event,
                                 void *cbdata)
{
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(event == context_open_event);
    assert(cbdata != NULL);

    ib_status_t rc;
    ib_module_t *mod = (ib_module_t *)cbdata;

    /* Initialize the core fields context. */
    rc = ib_core_vars_ctx_init(ib, mod, ctx, cbdata);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error initializing core fields: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    return IB_OK;
}

/**
 * Handle context close events for the core module
 *
 * @param[in] ib Engine
 * @param[in] ctx Context
 * @param[in] event Event triggering the callback
 * @param[in] cbdata Callback data (Module data)
 *
 * @returns Status code
 */
static ib_status_t core_ctx_close(ib_engine_t *ib,
                                  ib_context_t *ctx,
                                  ib_state_event_type_t event,
                                  void *cbdata)
{
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(event == context_close_event);
    assert(cbdata != NULL);

    ib_core_cfg_t *corecfg;
    ib_status_t rc;
    ib_mm_t mm = ib_engine_mm_main_get(ib);
    const ib_var_config_t *var_config = ib_engine_var_config_get(ib);

    /* Get the current context config. */
    rc = ib_core_context_config(ctx, &corecfg);
    if (rc != IB_OK) {
        ib_log_alert(ib,
                     "Error fetching core module context config: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Build the site selection list at the close of the main context */
    if (ib_context_type(ctx) == IB_CTYPE_MAIN) {
        rc = ib_ctxsel_finalize( ib );
        if (rc != IB_OK) {
            return rc;
        }

        /* Set up sources. */
/* Helper Macro */
#define CCC_SOURCE(name, src) \
    { \
        ib_var_source_t *temp; \
        rc = ib_var_source_acquire( \
            &temp, mm, var_config, IB_S2SL((name)) \
        ); \
        if (rc != IB_OK) { \
            ib_log_error(ib, \
                "Error acquiring var source: %s: %s", \
                (name), ib_status_to_string(rc) \
            ); \
            return rc; \
        } \
        corecfg->vars->src = temp; \
    }
/* End Helper Macro */

        CCC_SOURCE("THREAT_LEVEL",      threat_level);
        CCC_SOURCE("REQUEST_PROTOCOL",  request_protocol);
        CCC_SOURCE("REQUEST_METHOD",    request_method);
        CCC_SOURCE("RESPONSE_STATUS",   response_status);
        CCC_SOURCE("RESPONSE_PROTOCOL", response_protocol);
        CCC_SOURCE(IB_TX_CAPTURE,       tx_capture);
        CCC_SOURCE("FIELD_NAME_FULL",   field_name_full);
#undef CCC_SOURCE
    }

    return IB_OK;
}

typedef struct core_auditlog_fn_t {
     ib_core_auditlog_fn_t  handler; /**< Audit log handler. */
     void                  *cbdata;  /**< Associated callback data. */
} core_auditlog_fn_t;

ib_status_t ib_core_add_auditlog_handler(
    ib_context_t          *ctx,
    ib_core_auditlog_fn_t  auditlog_fn,
    void                  *auditlog_cbdata
)
{
    assert(ctx != NULL);
    assert(auditlog_fn != NULL);

    ib_status_t    rc;
    ib_core_cfg_t *config;
    core_auditlog_fn_t *handler;

    rc = ib_core_context_config(ctx, &config);
    if (rc != IB_OK) {
        ib_log_error(ctx->ib, "Failed to fetch core configuration.");
        return rc;
    }

    handler =
        (core_auditlog_fn_t *)ib_mm_calloc(ctx->mm, sizeof(*handler), 1);
    handler->handler = auditlog_fn;
    handler->cbdata  = auditlog_cbdata;

    rc = ib_list_push(config->auditlog_handlers, handler);
    if (rc != IB_OK) {
        ib_log_error(ctx->ib, "Failed to add auditlog handler to context.");
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_core_dispatch_auditlog(
    ib_tx_t                   *tx,
    ib_core_auditlog_event_en  event,
    ib_auditlog_t             *auditlog
)
{
    assert(tx != NULL);
    assert(tx->ib != NULL);
    assert(tx->ctx != NULL);

    ib_status_t         rc;
    ib_engine_t        *ib = tx->ib;
    ib_context_t       *ctx = tx->ctx;
    ib_core_cfg_t      *config;

    const ib_list_node_t *node;

    rc = ib_core_context_config(ctx, &config);
    if (rc != IB_OK) {
        ib_log_error(ctx->ib, "Failed to fetch core configuration.");
        return rc;
    }

    IB_LIST_LOOP_CONST(config->auditlog_handlers, node) {
        const core_auditlog_fn_t *handler =
            (const core_auditlog_fn_t *)ib_list_node_data_const(node);
        rc = handler->handler(ib, tx, event, auditlog, handler->cbdata);
        if (rc != IB_OK) {
            ib_log_notice_tx(
                tx,
                "Audit log handler returned status: %s",
                ib_status_to_string(rc));
        }
    }

    return IB_OK;
}

/**
 * Destroy the core module context
 *
 * @param[in] ib Engine
 * @param[in] ctx Context
 * @param[in] event Event triggering the callback
 * @param[in] cbdata Callback data (Module data)
 *
 * @returns Status code
 */
static ib_status_t core_ctx_destroy(ib_engine_t *ib,
                                    ib_context_t *ctx,
                                    ib_state_event_type_t event,
                                    void *cbdata)
{
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(event == context_destroy_event);
    assert(cbdata != NULL);

    if (ib_context_type_check(ctx, IB_CTYPE_ENGINE)) {

        ib_core_cfg_t *config;
        ib_status_t rc;

        rc = ib_core_context_config(ctx, &config);
        if (rc != IB_OK) {
            ib_log_alert(ib,
                         "Failed to fetch core module main context config.");
            return rc;
        }

        core_log_file_close(ib, config);
    }

    return IB_OK;
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
    ib_core_cfg_t *corecfg;
    ib_core_module_data_t *core_data;
    ib_filter_t *fbuffer;
    ib_status_t rc;
    ib_mm_t mm;

    mm = ib_engine_mm_main_get(ib);

    corecfg = ib_mm_calloc(mm, sizeof(*corecfg), 1);
    if (corecfg == NULL) {
        return IB_EALLOC;
    }

    rc = ib_module_config_initialize(m, corecfg, sizeof(*corecfg));
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to set configuration data for core module.");
        return rc;
    }

    /* Set defaults */
    corecfg->log_fp               = stderr;
    corecfg->log_uri              = "";
    corecfg->buffer_req           = 0;
    corecfg->buffer_res           = 0;
    corecfg->audit_engine         = IB_AUDIT_MODE_RELEVANT;
    corecfg->auditlog_dmode       = 0700;
    corecfg->auditlog_fmode       = 0600;
    corecfg->auditlog_parts       = IB_ALPARTS_DEFAULT;
    corecfg->auditlog_dir         = "/var/log/ironbee";
    corecfg->auditlog_sdir_fmt    = "";
    corecfg->auditlog_index_fmt   = IB_LOGFORMAT_DEFAULT;
    corecfg->audit                = MODULE_NAME_STR;
    corecfg->data                 = MODULE_NAME_STR;
    corecfg->module_base_path     = X_MODULE_BASE_PATH;
    corecfg->rule_base_path       = X_RULE_BASE_PATH;
    corecfg->rule_log_flags       = 0;
    corecfg->rule_log_level       = IB_LOG_INFO;
    corecfg->rule_debug_str       = "error";
    corecfg->rule_debug_level     = IB_RULE_DLOG_ERROR;
    corecfg->block_status         = 403;
    corecfg->block_method         = IB_BLOCK_METHOD_STATUS;
    corecfg->inspection_engine_options = IB_IEOPT_DEFAULT;
    corecfg->protection_engine_options = IB_PEOPT_DEFAULT;

    rc = ib_list_create(&(corecfg->auditlog_handlers), mm);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create auditlog handlers list in corecfg.");
        return rc;
    }

    /* Initialize core module limits to "off." */
    corecfg->limits.request_body_buffer_limit         = -1;
    corecfg->limits.request_body_buffer_limit_action  = IB_BUFFER_LIMIT_ACTION_FLUSH_PARTIAL;
    corecfg->limits.response_body_buffer_limit        = -1;
    corecfg->limits.response_body_buffer_limit_action = IB_BUFFER_LIMIT_ACTION_FLUSH_PARTIAL;
    corecfg->limits.request_body_log_limit            = -1;
    corecfg->limits.response_body_log_limit           = -1;

    /* Initialize vars */
    corecfg->vars = ib_mm_calloc(
        ib_engine_mm_main_get(ib), 1, sizeof(*corecfg->vars)
    );

    /* Register logger functions. */
    ib_logger_level_set(ib_engine_logger_get(ib), IB_LOG_INFO);

    /* Add the core logger to the engine. */
    core_add_core_logger(ib, corecfg);

    /* Force any IBUtil calls to use the default logger */
    rc = ib_util_log_logger(core_util_logger, ib);
    if (rc != IB_OK) {
        return rc;
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
        return rc;
    }
    ib_hook_tx_register(ib, handle_context_tx_event,
                        filter_ctl_config, fbuffer);

    /* Register hooks. */
    ib_hook_tx_register(ib, handle_context_tx_event,
                        core_hook_context_tx, NULL);
    ib_hook_conn_register(ib, conn_started_event, core_hook_conn_started, NULL);

    /* Register auditlog body buffering hooks. */
    ib_hook_txdata_register(ib, request_body_data_event,
                            core_hook_request_body_data, NULL);

    ib_hook_txdata_register(ib, response_body_data_event,
                            core_hook_response_body_data, NULL);

    /* Register postprocessing hooks. */
    ib_hook_tx_register(ib, handle_postprocess_event,
                        auditing_hook, NULL);

    /* Register context hooks. */
    ib_hook_context_register(ib, context_open_event,
                             core_ctx_open, m);
    ib_hook_context_register(ib, context_close_event,
                             core_ctx_close, m);
    ib_hook_context_register(ib, context_destroy_event,
                             core_ctx_destroy, m);

    /* Create core data structure */
    core_data = ib_mm_calloc(
        ib_engine_mm_main_get(ib),
        sizeof(*core_data),
        1
    );
    if (core_data == NULL) {
        return IB_EALLOC;
    }
    m->data = (void *)core_data;

    /* Register context selection hooks, etc. */
    rc = ib_core_ctxsel_init(ib, m);
    if (rc != IB_OK) {
        return rc;
    }

    /* Initialize the core fields */
    rc = ib_core_vars_init(ib, m);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error initializing core vars: %s", ib_status_to_string(rc));
        return rc;
    }

    /* Initialize the core transformations */
    rc = ib_core_transformations_init(ib, m);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Error initializing core operators: %s", ib_status_to_string(rc));
        return rc;
    }

    /* Initialize the core operators */
    rc = ib_core_operators_init(ib, m);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Error initializing core operators: %s", ib_status_to_string(rc));
        return rc;
    }

    /* Initialize the core actions */
    rc = ib_core_actions_init(ib, m);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Error initializing core actions: %s", ib_status_to_string(rc));
        return rc;
    }

    /* Register CAPTURE */
    rc = ib_var_source_register(
        NULL,
        ib_engine_var_config_get(ib),
        IB_TX_CAPTURE, strlen(IB_TX_CAPTURE),
        IB_PHASE_NONE, IB_PHASE_NONE
    );
    if (rc != IB_OK) {
        ib_log_notice(ib,
            "Failed to register %s: %s",
            IB_TX_CAPTURE, ib_status_to_string(rc)
        );
        /* Everything should still work, so do not return error. */
    }

    return IB_OK;
}

/**
 * Shutdown the core module on exit.
 *
 * @param[in] ib Engine
 * @param[in] m Module
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 */
static
ib_status_t core_finish(
    ib_engine_t *ib,
    ib_module_t *m,
    void        *cbdata
)
{
    /* Nop. */
    return IB_OK;
}

/**
 * Core module configuration parameter initialization structure.
 */
static IB_CFGMAP_INIT_STRUCTURE(core_config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        "logger.log_uri",
        IB_FTYPE_NULSTR,
        ib_core_cfg_t,
        log_uri
    ),

    /* Rule logging */
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
    IB_CFGMAP_INIT_ENTRY(
        "RuleEngineDebugLogLevel",
        IB_FTYPE_NULSTR,
        ib_core_cfg_t,
        rule_debug_str
    ),
    IB_CFGMAP_INIT_ENTRY(
        "_RuleEngineDebugLevel",
        IB_FTYPE_NUM,
        ib_core_cfg_t,
        rule_debug_level
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
        "inspection_engine_options",
        IB_FTYPE_NUM,
        ib_core_cfg_t,
        inspection_engine_options
    ),
    IB_CFGMAP_INIT_ENTRY(
        "protection_engine_options",
        IB_FTYPE_NUM,
        ib_core_cfg_t,
        protection_engine_options
    ),

    /* End */
    IB_CFGMAP_INIT_LAST
};

ib_module_t *ib_core_module_sym(void)
{
    return IB_MODULE_STRUCT_PTR;
}

ib_module_t *ib_core_module(
    const ib_engine_t *ib)
{
    assert(ib != NULL);
    ib_module_t *module;
    ib_status_t  rc;

    /* If this fails, we're in bad shape.  Fail hard. */
    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    assert(rc == IB_OK);
    return module;
}

ib_status_t ib_core_auditlog_parts_map(
    const ib_strval_t   **pmap)
{
    assert(pmap != NULL);
    *pmap = core_auditlog_parts_map;
    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_core_limits_get(
    ib_context_t *ctx,
    const ib_tx_limits_t **limits
)
{
    assert(ctx != NULL);
    assert(limits != NULL);

    ib_core_cfg_t *corecfg = NULL;
    ib_status_t    rc;

    rc = ib_core_context_config(ctx, &corecfg);
    if (rc != IB_OK) {
        ib_log_error(ctx->ib, "Failed to retrieve core configuration.");
        return rc;
    }

    *limits = &(corecfg->limits);

    return IB_OK;
}

/**
 * Configuration context copy.
 *
 * @param[in] ib IronBee engine.
 * @param[in] module The core module.
 * @param[out] dst The destination configuration struct.
 * @param[in] src The source configuration.
 * @param[in] len The length of dst and src.
 * @param[in] cbdata Callback data. Unused.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t core_config_copy(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *dst,
    const void  *src,
    size_t       len,
    void        *cbdata
)
{
    assert(ib != NULL);
    assert(dst != NULL);
    assert(src != NULL);

    ib_mm_t               mm = ib_engine_mm_main_get(ib);
    ib_core_cfg_t        *dst_cfg = (ib_core_cfg_t *)dst;
    const ib_core_cfg_t  *src_cfg = (const ib_core_cfg_t *)src;
    const ib_list_node_t *node;
    ib_status_t           rc;

    /* First, do a shallow copy. */
    memcpy(dst, src, len);

    rc = ib_list_create(&(dst_cfg->auditlog_handlers), mm);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to copy core configuration.");
        return rc;
    }

    /* Copy the handler list into the sub context. */
    IB_LIST_LOOP_CONST(src_cfg->auditlog_handlers, node) {
        ib_list_push(
            dst_cfg->auditlog_handlers,
            (void *)ib_list_node_data_const(node));
    }

    return IB_OK;
}

/**
 * Static core module structure.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /**< Default metadata */
    MODULE_NAME_STR,                     /**< Module name */
    NULL,                                /**< Config data. */
    0,                                   /**< Config data length. */
    core_config_copy,                    /**< Config copy function. */
    NULL,                                /**< Copy function cbdata. */
    core_config_map,                     /**< Configuration field map */
    core_directive_map,                  /**< Config directive map */
    core_init,                           /**< Initialize function */
    NULL,                                /**< Callback data */
    core_finish,                         /**< Finish function */
    NULL                                 /**< Callback data */
);
