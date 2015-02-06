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
 * @brief IronBee
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/rule_logger.h>
#include "rule_logger_private.h"
#include "rule_engine_private.h"

#include <ironbee/action.h>
#include <ironbee/bytestr.h>
#include <ironbee/context.h>
#include <ironbee/core.h>
#include <ironbee/engine_state.h>
#include <ironbee/escape.h>
#include <ironbee/field.h>
#include <ironbee/flags.h>
#include <ironbee/logevent.h>
#include <ironbee/mm_mpool_lite.h>
#include <ironbee/operator.h>
#include <ironbee/rule_engine.h>
#include <ironbee/string.h>
#include <ironbee/transformation.h>
#include <ironbee/util.h>

#include <assert.h>
#include <inttypes.h>

/**
 * Length of buffer used for formatting fields
 */
static const size_t MAX_FIELD_BUF = 64;

/**
 * If any of these flags are set, enable rule logging
 */
#define RULE_LOG_FLAG_RULE_ENABLE                            \
    ( IB_RULE_LOG_FLAG_RULE |                                \
      IB_RULE_LOG_FLAG_TARGET |                              \
      IB_RULE_LOG_FLAG_TFN |                                 \
      IB_RULE_LOG_FLAG_OPERATOR |                            \
      IB_RULE_LOG_FLAG_ACTION |                              \
      IB_RULE_LOG_FLAG_EVENT |                               \
      IB_RULE_LOG_FLAG_AUDIT |                               \
      IB_RULE_LOG_FLAG_TIMING )

/**
 * If any of these flags are set, enable target gathering
 */
#define RULE_LOG_FLAG_TARGET_ENABLE                  \
    ( IB_RULE_LOG_FLAG_TARGET |                      \
      IB_RULE_LOG_FLAG_TFN |                         \
      IB_RULE_LOG_FLAG_OPERATOR |                    \
      IB_RULE_LOG_FLAG_EVENT |                       \
      IB_RULE_LOG_FLAG_AUDIT |                       \
      IB_RULE_LOG_FLAG_ACTION )

/**
 * If any of these flags are set, enable result gathering
 */
#define RULE_LOG_FLAG_RESULT_ENABLE                  \
    ( IB_RULE_LOG_FLAG_TARGET |                      \
      IB_RULE_LOG_FLAG_OPERATOR |                    \
      IB_RULE_LOG_FLAG_EVENT |                       \
      IB_RULE_LOG_FLAG_ACTION )

/**
 * Mapping of valid rule logging names to flag values.
 */
static IB_STRVAL_MAP(flags_map) = {
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

static ib_status_t ib_field_format_quote(
    ib_mm_t            mm,
    const ib_field_t  *field,
    char             **buffer,
    size_t            *buffer_sz
)
{
    ib_status_t   rc;
    const size_t  bufsize = MAX_FIELD_BUF+1;
    char         *buf;

    assert(field != NULL);
    assert(buffer != NULL);
    assert(buffer_sz != NULL);

    buf = ib_mm_alloc(mm, MAX_FIELD_BUF+1);
    if (buf == NULL) {
        return IB_EALLOC;
    }

    *buf = '\0';
    if (field == NULL) {
        strncpy(buf, "\"\"", bufsize-1);
        *(buf+bufsize) = '\0';
    }
    else {
        switch (field->type) {

        case IB_FTYPE_NULSTR :
        {
            const char *s;
            rc = ib_field_value(field, ib_ftype_nulstr_out(&s));
            if (rc != IB_OK) {
                break;
            }
            snprintf(buf, bufsize, "\"%s\"", (s?s:""));

            break;
        }

        case IB_FTYPE_BYTESTR:
        {
            const ib_bytestr_t *bs;

            rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
            if (rc != IB_OK) {
                break;
            }

            snprintf(buf, bufsize, "\"%.*s\"",
                     (int)ib_bytestr_length(bs),
                     (const char *)ib_bytestr_const_ptr(bs));
            break;
        }

        case IB_FTYPE_NUM :          /**< Numeric value */
        {
            ib_num_t n;
            rc = ib_field_value(field, ib_ftype_num_out(&n));
            if (rc != IB_OK) {
                break;
            }
            snprintf(buf, bufsize, "%"PRId64, n);
            break;
        }

        case IB_FTYPE_TIME :          /**< Time value */
        {
            ib_time_t t;
            rc = ib_field_value(field, ib_ftype_time_out(&t));
            if (rc != IB_OK) {
                break;
            }
            snprintf(buf, bufsize, "%"PRIu64, t);
            break;
        }

        case IB_FTYPE_FLOAT :        /**< Float numeric value */
        {
            ib_float_t f;
            rc = ib_field_value(field, ib_ftype_float_out(&f));
            if (rc != IB_OK) {
                break;
            }
            snprintf(buf, bufsize, "%Lf", f);
            break;
        }

        case IB_FTYPE_LIST :         /**< List */
        {
            const ib_list_t *lst;
            size_t len;

            rc = ib_field_value(field, ib_ftype_list_out(&lst));
            if (rc != IB_OK) {
                break;
            }
            len = ib_list_elements(lst);
            if (len == 0) {
                snprintf(buf, bufsize, "list[%zd]", len);
            }
            else {
                const ib_list_node_t *node;
                node = ib_list_last_const(lst);
                if (node == NULL) {
                    snprintf(buf, bufsize, "list[%zd]", len);
                }
                else {
                    ib_field_format_quote(
                        mm,
                        (const ib_field_t *)ib_list_node_data_const(node),
                        buffer,
                        buffer_sz);
                }
            }
            break;
        }

        default:
            snprintf(buf, bufsize, "type = %d", field->type);
            break;
        }
    }

    /* Return the buffer */
    *buffer = buf;
    *buffer_sz = bufsize;
    return IB_OK;
}

/**
 * Format a field into a string.
 *
 * @param[in] mm Memory manager to allocate @a buf out of.
 * @param[in] field Field to convert to a string.
 * @param[out] buffer Buffer for output.
 * @param[out] buffer_sz Size of @a buf.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On error.
 */
static ib_status_t ib_field_format_escape(
    ib_mm_t            mm,
    const ib_field_t  *field,
    char             **buffer,
    size_t            *buffer_sz
)
{
    ib_status_t   rc;
    const size_t  bufsize = MAX_FIELD_BUF+1;
    char         *buf;

    assert(field != NULL);
    assert(buffer != NULL);
    assert(buffer_sz != NULL);

    buf = ib_mm_alloc(mm, MAX_FIELD_BUF+1);
    if (buf == NULL) {
        return IB_EALLOC;
    }

    *buf = '\0';
    if (field == NULL) {
        strncpy(buf, "\"\"", bufsize-1);
        *(buf+bufsize) = '\0';
    }
    else {
        switch (field->type) {

        case IB_FTYPE_NULSTR :
        {
            const char *s;
            rc = ib_field_value(field, ib_ftype_nulstr_out(&s));
            if (rc != IB_OK) {
                break;
            }
            ib_string_escape_json_buf((const uint8_t *)s, strlen(s), buf, bufsize, NULL);

            break;
        }

        case IB_FTYPE_BYTESTR:
        {
            const ib_bytestr_t *bs;

            rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
            if (rc != IB_OK) {
                break;
            }

            ib_string_escape_json_buf(ib_bytestr_const_ptr(bs),
                                      ib_bytestr_length(bs),
                                      buf, bufsize, NULL);
            break;
        }

        case IB_FTYPE_NUM :          /**< Numeric value */
        {
            ib_num_t n;
            rc = ib_field_value(field, ib_ftype_num_out(&n));
            if (rc != IB_OK) {
                break;
            }
            snprintf(buf, bufsize, "%"PRId64, n);
            break;
        }

        case IB_FTYPE_TIME :          /**< Time value */
        {
            ib_time_t t;
            rc = ib_field_value(field, ib_ftype_time_out(&t));
            if (rc != IB_OK) {
                break;
            }
            snprintf(buf, bufsize, "%"PRIu64, t);
            break;
        }

        case IB_FTYPE_FLOAT :        /**< Float numeric value */
        {
            ib_float_t f;
            rc = ib_field_value(field, ib_ftype_float_out(&f));
            if (rc != IB_OK) {
                break;
            }
            snprintf(buf, bufsize, "%Lf", f);
            break;
        }

        case IB_FTYPE_LIST :         /**< List */
        {
            const ib_list_t *lst;
            size_t len;

            rc = ib_field_value(field, ib_ftype_list_out(&lst));
            if (rc != IB_OK) {
                break;
            }
            len = ib_list_elements(lst);
            if (len == 0) {
                snprintf(buf, bufsize, "list[%zd]", len);
            }
            else {
                const ib_list_node_t *node;
                node = ib_list_last_const(lst);
                if (node == NULL) {
                    snprintf(buf, bufsize, "list[%zd]", len);
                }
                else {
                    rc = ib_field_format_escape(
                        mm,
                        (const ib_field_t *)ib_list_node_data_const(node),
                        buffer,
                        buffer_sz
                    );
                    if (rc != IB_OK) {
                        break;
                    }
                }
            }
            break;
        }

        default:
            snprintf(buf, bufsize, "type = %d", field->type);
            break;
        }
    }

    /* Return the buffer */
    *buffer = buf;
    *buffer_sz = bufsize;
    return IB_OK;
}

/**
 * Generic Logger for rules.
 *
 * @warning There is currently a 1024 byte formatter limit when prefixing the
 *          log header data.
 *
 * @param[in] rule_log_level Rule debug log level
 * @param[in] log_level Log level to log at
 * @param[in] tx Transaction information
 * @param[in] rule Rule to log (or NULL)
 * @param[in] target Rule target (or NULL)
 * @param[in] file Filename (or NULL)
 * @param[in] func Function name (or NULL)
 * @param[in] line Line number (or 0)
 * @param[in] fmt Printf-like format string
 * @param[in] ap Argument list
 *
 * @note: Separate prototype for the format attribute handling
 */
static void rule_vlog_tx(
    ib_rule_dlog_level_t rule_log_level,
    ib_logger_level_t log_level,
    const ib_tx_t *tx,
    const ib_rule_t *rule,
    const ib_rule_target_t *target,
    const char *file,
    const char *func,
    int line,
    const char *fmt,
    va_list ap
) VPRINTF_ATTRIBUTE(9);

static void rule_vlog_tx(
    ib_rule_dlog_level_t rule_log_level,
    ib_logger_level_t log_level,
    const ib_tx_t *tx,
    const ib_rule_t *rule,
    const ib_rule_target_t *target,
    const char *file,
    const char *func,
    int line,
    const char *fmt,
    va_list ap
)
{
    char *fmtbuf = NULL;
    size_t fmtlen;
    void *freeptr = NULL;
    ib_rule_dlog_level_t dlog_level =
        (tx->ctx == NULL) ? IB_RULE_DLOG_INFO : ib_rule_dlog_level(tx->ctx);

    /* Ignore this message? */
    if (rule_log_level > dlog_level) {
        return;
    }

    /* Allocate new format buffer */
    if (rule == NULL) {
        fmtlen = strlen(fmt) + 8;
    }
    else {
        const char *id = ib_rule_id(rule);
        fmtlen = strlen(fmt) + strlen(id) + 24;
    }
    fmtbuf = malloc(fmtlen);

    if (fmtbuf != NULL) {
        if (rule == NULL) {
            strcpy(fmtbuf, "[] ");
        }
        else {
            snprintf(fmtbuf, fmtlen, "[rule:\"%s\" rev:%d] ",
                     ib_rule_id(rule), rule->meta.revision);
        }
        strcat(fmtbuf, fmt);
        fmt = fmtbuf;
        freeptr = fmtbuf;
    }

    ib_log_tx_vex(tx, log_level, file, func, line, fmt, ap);

    if (freeptr != NULL) {
        free(freeptr);
    }

    return;
}

/* Generic Logger for rules. */
void ib_rule_log_tx(
    ib_rule_dlog_level_t rule_log_level,
    const ib_tx_t *tx,
    const char *file,
    const char *func,
    int line,
    const char *fmt, ...
)
{
    va_list ap;
    ib_core_cfg_t *corecfg = NULL;
    ib_logger_level_t ib_log_level;
    ib_status_t rc;


    switch(rule_log_level) {
    case IB_RULE_DLOG_ERROR:
        ib_log_level = IB_LOG_ERROR;
        break;
    case IB_RULE_DLOG_WARNING:
        ib_log_level = IB_LOG_WARNING;
        break;
    case IB_RULE_DLOG_NOTICE:
        ib_log_level = IB_LOG_NOTICE;
        break;
    case IB_RULE_DLOG_INFO:
        ib_log_level = IB_LOG_INFO;
        break;
    case IB_RULE_DLOG_DEBUG:
        ib_log_level = IB_LOG_DEBUG;
        break;
    case IB_RULE_DLOG_TRACE:
        ib_log_level = IB_LOG_TRACE;
        break;
    case IB_RULE_DLOG_ALWAYS:
    default:
        rc = ib_core_context_config(tx->ctx, &corecfg);
        ib_log_level = (rc == IB_OK) ?  corecfg->rule_log_level : IB_LOG_DEBUG;
        break;
    }

    va_start(ap, fmt);
    rule_vlog_tx(rule_log_level, ib_log_level,
                 tx, NULL, NULL, file, func,line, fmt, ap);
    va_end(ap);

    return;
}

void ib_rule_log_exec(
    ib_rule_dlog_level_t level,
    const ib_rule_exec_t *rule_exec,
    const char *file,
    const char *func,
    int line,
    const char *fmt, ...
)
{
    va_list ap;
    ib_logger_level_t log_level =
        (rule_exec->tx_log == NULL) ? IB_LOG_INFO : rule_exec->tx_log->level;

    va_start(ap, fmt);
    rule_vlog_tx(level, log_level,
                 rule_exec->tx, rule_exec->rule, rule_exec->target,
                 file, func, line, fmt, ap);
    va_end(ap);

    return;
}

/**
 * Log a rule execution line
 *
 * @param[in] rule_exec  Rule execution information
 * @param[in] fmt Printf-like format string
 */
static void rule_log_exec(const ib_rule_exec_t *rule_exec,
                          const char *fmt, ...) PRINTF_ATTRIBUTE(2, 3);

static void rule_log_exec(
    const ib_rule_exec_t *rule_exec,
    const char *fmt, ...
)
{
    va_list ap;

    va_start(ap, fmt);
    rule_vlog_tx(IB_RULE_DLOG_ALWAYS, rule_exec->tx_log->level,
                 rule_exec->tx, rule_exec->rule, rule_exec->target,
                 NULL, NULL, 0, fmt, ap);
    va_end(ap);

    return;
}

void ib_rule_log_flags_dump(const ib_engine_t *ib,
                            const ib_context_t *ctx)
{
    const ib_strval_t *rec;
    ib_flags_t flags;

    if (ib_rule_dlog_level(ctx) < IB_RULE_DLOG_DEBUG) {
        return;
    }
    flags = ib_rule_log_flags(ctx);
    if (ib_flags_all(flags, IB_RULE_LOG_FILT_ALL)) {
        ib_flags_clear(flags, IB_RULE_LOG_FILTER_MASK);
    }
    else if (ib_flags_any(flags, IB_RULE_LOG_FILTER_MASK) == false) {
        ib_flags_set(flags, IB_RULE_LOG_FILT_ALL);
    }

    for (rec = flags_map; rec->str != NULL; ++rec) {
        bool enabled = ib_flags_all(flags, rec->val);
        ib_log_trace(ib,
                     "Rule logging flag %s [0x%08lx]: %s",
                     rec->str,
                     (unsigned long)rec->val,
                     enabled ? "enabled" : "disabled");
    }
    return;
}

ib_flags_t ib_rule_log_flags(const ib_context_t *ctx)
{
    ib_core_cfg_t *corecfg = NULL;
    ib_status_t rc;

    if (ctx == NULL) {
        /* Always return 0 if there is not a context as
         * ib_core_context_config() will otherwise assert. */
        return 0;
    }

    rc = ib_core_context_config(ctx, &corecfg);
    if (rc != IB_OK) {
        /* Always return 0 if there is not a context config. */
        return 0;
    }

    return corecfg->rule_log_flags;
}

ib_logger_level_t ib_rule_log_level(const ib_context_t *ctx)
{
    ib_core_cfg_t *corecfg = NULL;
    ib_status_t rc;

    if (ctx == NULL) {
        /* Always log if there is not a context as
         * ib_core_context_config() will otherwise assert. */
        return IB_LOG_DEBUG;
    }

    rc = ib_core_context_config(ctx, &corecfg);
    if (rc != IB_OK) {
        /* Always log if there is not a context config. */
        return IB_LOG_DEBUG;
    }

    return corecfg->rule_log_level;
}

ib_rule_dlog_level_t ib_rule_dlog_level(const ib_context_t *ctx)
{
    ib_core_cfg_t *corecfg = NULL;
    ib_status_t rc;

    if (ctx == NULL) {
        /* Always log if there is not a context as
         * ib_core_context_config() will otherwise assert. */
        return IB_RULE_DLOG_ALWAYS;
    }

    rc = ib_core_context_config(ctx, &corecfg);
    if (rc != IB_OK) {
        /* Always log if there is not a context config. */
        return IB_RULE_DLOG_ALWAYS;
    }

    return corecfg->rule_debug_level;
}

/* Log TX start */
ib_status_t ib_rule_log_tx_create(
    const ib_rule_exec_t *rule_exec,
    ib_rule_log_tx_t **tx_log
)
{
    ib_flags_t          flags;
    ib_rule_log_tx_t   *object;

    assert(rule_exec != NULL);
    assert(rule_exec->tx != NULL);
    assert(tx_log != NULL);

    /* Get the flags from the transaction */
    flags = (ib_flags_t)ib_rule_log_flags(rule_exec->tx->ctx);

    /* Allocate the object */
    object = ib_mm_calloc(rule_exec->tx->mm, sizeof(*object), 1);
    if (object == NULL) {
        return IB_EALLOC;
    }

    /* Get the start time */
    if (ib_flags_all(flags, IB_RULE_LOG_FLAG_TIMING)) {
        ib_clock_gettimeofday(&object->start_time);
    }

    /* If ALL is set, clear the other filter flags */
    if (ib_flags_all(flags, IB_RULE_LOG_FILT_ALL)) {
        ib_flags_clear(flags, IB_RULE_LOG_FILTER_MASK);
    }
    else if (ib_flags_any(flags, IB_RULE_LOG_FILTER_MASK) == false) {
        ib_flags_set(flags, IB_RULE_LOG_FILT_ALL);
    }

    /* Complete the new object, store pointer to it */
    object->level = ib_rule_log_level(rule_exec->tx->ctx);
    object->flags = flags;
    object->filter = (flags & IB_RULE_LOG_FILTER_ALLMASK);
    object->cur_phase = IB_PHASE_NONE;
    object->phase_name = NULL;
    object->mm = rule_exec->tx->mm;
    object->empty_tx = true;
    *tx_log = object;

    return IB_OK;
}

ib_status_t ib_rule_log_exec_create(const ib_rule_exec_t *rule_exec,
                                    ib_rule_log_exec_t **exec_log)
{
    ib_status_t rc;
    ib_rule_log_exec_t *new;
    ib_rule_log_tx_t *tx_log;

    assert(rule_exec != NULL);
    assert(exec_log != NULL);

    *exec_log = NULL;
    tx_log = rule_exec->tx_log;

    /* If no transaction, nothing to do */
    if (tx_log == NULL) {
        return IB_OK;
    }

    /* Flag check */
    if (ib_flags_any(tx_log->flags, RULE_LOG_FLAG_RULE_ENABLE) == false) {
        return IB_OK;
    }

    /* Allocate the object */
    new = ib_mm_calloc(tx_log->mm, sizeof(*new), 1);
    if (new == NULL) {
        return IB_EALLOC;
    }

    /* Create the list of target fields */
    if (ib_flags_any(tx_log->flags, RULE_LOG_FLAG_TARGET_ENABLE)) {
        rc = ib_list_create(&(new->tgt_list), tx_log->mm);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Complete the new object, store pointer to it */
    new->tx_log = tx_log;
    new->rule = rule_exec->rule;
    if (ib_rule_is_stream(rule_exec->rule)) {
        new->filter = IB_RULE_LOG_FILT_OPEXEC;
    }
    else {
        new->filter = tx_log->filter;
    }

    /* Don't need to initialize xxx_count because we use calloc() above */
    *exec_log = new;

    return IB_OK;
}

void ib_rule_log_fatal_ex(
    const ib_rule_exec_t *rule_exec,
    const char *file,
    const char *func,
    int line,
    const char *fmt, ...
)
{

    va_list ap;
    ib_rule_log_exec_t *exec_log = rule_exec->exec_log;
    ib_logger_level_t log_level =
        (rule_exec->tx_log == NULL) ? IB_LOG_INFO : rule_exec->tx_log->level;

    ib_flags_set(exec_log->flags, IB_RULE_EXEC_FATAL);

    va_start(ap, fmt);
    rule_vlog_tx(IB_RULE_DLOG_ERROR, log_level,
                 rule_exec->tx, rule_exec->rule, rule_exec->target,
                 file, func, line, fmt, ap);
    va_end(ap);

    return;
}

ib_status_t ib_rule_log_exec_add_target(
    ib_rule_log_exec_t *exec_log,
    const ib_rule_target_t *target,
    const ib_field_t *original
)
{
    ib_status_t rc = IB_OK;
    ib_rule_log_tgt_t *tgt;

    if (exec_log == NULL) {
        return IB_OK;
    }
    ++(exec_log->tgt_count);
    if (exec_log->tgt_list == NULL) {
        return IB_OK;
    }

    tgt = (ib_rule_log_tgt_t *)
        ib_mm_calloc(exec_log->tx_log->mm, sizeof(*tgt), 1);
    if (tgt == NULL) {
        return IB_EALLOC;
    }
    tgt->target = target;
    tgt->original = original;
    tgt->transformed = NULL;

    /* Initialize the result list */
    if (ib_flags_any(exec_log->tx_log->flags, RULE_LOG_FLAG_RESULT_ENABLE) ) {
        rc = ib_list_create(&tgt->rslt_list, exec_log->tx_log->mm);
        if (rc != IB_OK) {
            return rc;
        }
    }
    tgt->rslt_count = 0;

    /* Initialize the transformation list */
    if (ib_flags_any(exec_log->tx_log->flags, IB_RULE_LOG_FLAG_TFN) ) {
        rc = ib_list_create(&tgt->tfn_list, exec_log->tx_log->mm);
        if (rc != IB_OK) {
            return rc;
        }
    }
    tgt->tfn_count = 0;

    rc = ib_list_push(exec_log->tgt_list, tgt);
    if (rc != IB_OK) {
        return rc;
    }
    exec_log->tgt_cur = tgt;
    return rc;
}

ib_status_t ib_rule_log_exec_set_tgt_final(ib_rule_log_exec_t *exec_log,
                                           const ib_field_t *final)
{
    if ( (exec_log == NULL) || (exec_log->tgt_cur == NULL) ) {
        return IB_OK;
    }
    exec_log->tgt_cur->transformed = final;

    return IB_OK;
}

ib_status_t ib_rule_log_exec_add_stream_tgt(ib_engine_t *ib,
                                            ib_rule_log_exec_t *exec_log,
                                            const ib_field_t *field)
{
    ib_status_t rc = IB_OK;
    ib_rule_target_t *target;
    char *fname;

    if (exec_log == NULL) {
        return IB_OK;
    }
    if (exec_log->tgt_list == NULL) {
        ++(exec_log->tgt_count);  /* Normally done by add_target() call below */
        return IB_OK;
    }

    target = ib_mm_alloc(exec_log->tx_log->mm, sizeof(*target));
    if (target == NULL) {
        return IB_EALLOC;
    }

    fname = ib_mm_alloc(exec_log->tx_log->mm, field->nlen+1);
    if (fname == NULL) {
        return IB_EALLOC;
    }
    strncpy(fname, field->name, field->nlen);
    *(fname + field->nlen) = '\0';
    target->target_str = fname;

    rc = ib_var_target_acquire_from_string(
        &target->target,
        exec_log->tx_log->mm,
        ib_engine_var_config_get(ib),
        field->name, field->nlen
    );
    if (rc != IB_OK) {
        return rc;
    }

    target->tfn_list = NULL;

    rc = ib_rule_log_exec_add_target(exec_log, target, field);
    if (rc != IB_OK) {
        return rc;
    }

    return rc;
}

ib_status_t ib_rule_log_exec_tfn_inst_add(ib_rule_log_exec_t *exec_log,
                                          const ib_transformation_inst_t *tfn_inst)
{
    ib_status_t rc = IB_OK;
    ib_rule_log_tfn_t *object;
    ib_rule_log_tgt_t *tgt;
    ib_list_t *value_list;

    if (exec_log == NULL) {
        return IB_OK;
    }

    tgt = exec_log->tgt_cur;
    if ( (tgt == NULL) || (tgt->tfn_list == NULL) ) {
        return IB_OK;
    }

    rc = ib_list_create(&value_list, exec_log->tx_log->mm);
    if (rc != IB_OK) {
        return rc;
    }

    object = (ib_rule_log_tfn_t *)
        ib_mm_alloc(exec_log->tx_log->mm, sizeof(*object));
    if (object == NULL) {
        return IB_EALLOC;
    }
    object->tfn_inst = tfn_inst;
    object->value_list = value_list;
    tgt->tfn_cur = object;

    return rc;
}

ib_status_t ib_rule_log_exec_tfn_value(ib_rule_log_exec_t *exec_log,
                                       const ib_field_t *in,
                                       const ib_field_t *out,
                                       ib_status_t status)
{
    ib_status_t rc = IB_OK;
    ib_rule_log_tgt_t *tgt;
    ib_rule_log_tfn_t *tfn;
    ib_rule_log_tfn_val_t *object;

    if (exec_log == NULL) {
        return IB_OK;
    }
    tgt = exec_log->tgt_cur;
    if (tgt == NULL) {
        return IB_OK;
    }
    tfn = tgt->tfn_cur;
    if ( (tfn == NULL) || (tfn->value_list == NULL) ) {
        return IB_OK;
    }

    object = (ib_rule_log_tfn_val_t *)
        ib_mm_alloc(exec_log->tx_log->mm, sizeof(*object));
    if (object == NULL) {
        return IB_EALLOC;
    }
    object->in = in;
    object->out = out;
    object->status = status;

    rc = ib_list_push(tfn->value_list, object);
    return rc;
}

ib_status_t ib_rule_log_exec_tfn_inst_fin(ib_rule_log_exec_t *exec_log,
                                          const ib_transformation_inst_t *tfn_inst,
                                          const ib_field_t *in,
                                          const ib_field_t *out,
                                          ib_status_t status)
{
    ib_status_t rc = IB_OK;
    ib_rule_log_tfn_t *tfn_log;
    ib_rule_log_tgt_t *tgt;

    if (exec_log == NULL) {
        return IB_OK;
    }
    tgt = exec_log->tgt_cur;
    if ( (tgt == NULL) || (tgt->tfn_list == NULL) ) {
        return IB_OK;
    }
    tfn_log = tgt->tfn_cur;
    if ( (tfn_log == NULL) || (tfn_log->tfn_inst != tfn_inst) ) {
        return IB_EINVAL;
    }
    tfn_log->value.in = in;
    tfn_log->value.out = out;
    tfn_log->value.status = status;

    rc = ib_list_push(tgt->tfn_list, tfn_log);
    tgt->tfn_cur = NULL;
    return rc;
}

static void count_result(ib_rule_log_count_t *counts,
                         ib_num_t result,
                         ib_status_t status)
{
    if (status != IB_OK) {
        ++counts->error_count;
    }
    else if (result) {
        ++counts->true_count;
    }
    else {
        ++counts->false_count;
    }

    return;
}

ib_status_t ib_rule_log_exec_add_result(ib_rule_log_exec_t *exec_log,
                                        const ib_field_t *value,
                                        ib_num_t result)
{
    ib_status_t rc = IB_OK;
    ib_rule_log_rslt_t *object;
    ib_rule_log_tgt_t *tgt;
    ib_status_t status;

    if (exec_log == NULL) {
        return IB_OK;
    }
    status = exec_log->op_status;
    count_result(&exec_log->counts, result, status);

    tgt = exec_log->tgt_cur;
    if (tgt == NULL) {
        return IB_OK;
    }
    count_result(&(tgt->counts), result, status);

    if (tgt->rslt_list == NULL) {
        return IB_OK;
    }

    object = (ib_rule_log_rslt_t *)
        ib_mm_calloc(exec_log->tx_log->mm, sizeof(*object), 1);
    if (object == NULL) {
        return IB_EALLOC;
    }
    object->value = value;
    object->result = result;
    object->status = status;

    if (ib_flags_all(exec_log->tx_log->flags, IB_RULE_LOG_FLAG_ACTION) ) {
        rc = ib_list_create(&(object->act_list), exec_log->tx_log->mm);
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        object->act_list = NULL;
    }

    if (ib_flags_all(exec_log->tx_log->flags, IB_RULE_LOG_FLAG_EVENT) ) {
        rc = ib_list_create(&(object->event_list), exec_log->tx_log->mm);
        if (rc != IB_OK) {
            return rc;
        }
    }
    else {
        object->event_list = NULL;
    }

    tgt->rslt_cur = object;
    rc = ib_list_push(tgt->rslt_list, object);
    return rc;
}

ib_status_t ib_rule_log_exec_add_action(ib_rule_log_exec_t *exec_log,
                                        const ib_action_inst_t *act_inst,
                                        ib_status_t status)
{
    ib_status_t rc = IB_OK;
    ib_rule_log_tgt_t *tgt;
    ib_rule_log_rslt_t *rslt;
    ib_rule_log_act_t *object;

    if (exec_log == NULL) {
        return IB_OK;
    }
    ++(exec_log->counts.act_count);

    tgt = exec_log->tgt_cur;
    if (tgt == NULL) {
        return IB_OK;
    }
    ++(tgt->counts.act_count);

    if (tgt->rslt_list == NULL) {
        return IB_OK;
    }

    rslt = tgt->rslt_cur;
    if (rslt == NULL) {
        return IB_OK;
    }
    ++(rslt->act_count);

    if (rslt->act_list == NULL) {
        return IB_OK;
    }

    object = (ib_rule_log_act_t *)
        ib_mm_calloc(exec_log->tx_log->mm, sizeof(*object), 1);
    if (object == NULL) {
        return IB_EALLOC;
    }
    object->status = status;
    object->act_inst = act_inst;

    rc = ib_list_push(rslt->act_list, object);
    return rc;
}

ib_status_t ib_rule_log_exec_add_event(ib_rule_log_exec_t *exec_log,
                                       const ib_logevent_t *event)
{
    ib_status_t rc = IB_OK;
    ib_rule_log_tgt_t *tgt;
    ib_rule_log_rslt_t *rslt;

    if (exec_log == NULL) {
        return IB_OK;
    }
    ++(exec_log->counts.event_count);

    tgt = exec_log->tgt_cur;
    if (tgt == NULL) {
        return IB_OK;
    }
    ++(tgt->counts.event_count);

    rslt = tgt->rslt_cur;
    if (rslt == NULL) {
        return IB_OK;
    }
    ++(rslt->event_count);

    if (rslt->event_list == NULL) {
        return IB_OK;
    }

    rc = ib_list_push(rslt->event_list, (ib_logevent_t *)event);
    return rc;
}

/* Log audit log file */
void ib_rule_log_add_audit(
    const ib_rule_exec_t *rule_exec,
    const char * const    audit_log,
    bool                  failed
)
{
    assert(rule_exec != NULL);
    assert(rule_exec->tx_log != NULL);
    assert(audit_log != NULL);

    if (rule_exec->tx_log == NULL) {
        return;
    }

    if (ib_flags_any(rule_exec->tx_log->flags, IB_RULE_LOG_FLAG_AUDIT))
    {
        rule_log_exec(rule_exec, "AUDIT %s", audit_log);
    }

    return;
}

ib_status_t ib_rule_log_exec_op(ib_rule_log_exec_t *exec_log,
                                const ib_rule_operator_inst_t *opinst,
                                ib_status_t status)
{
    if (exec_log == NULL) {
        return IB_OK;
    }
    ++(exec_log->counts.exec_count);
    exec_log->op_status = status;

    if (exec_log->tgt_cur != NULL) {
        ++(exec_log->tgt_cur->counts.exec_count);
    }

    return IB_OK;
}

static void log_tx_start(
    const ib_rule_exec_t *rule_exec
)
{
    ib_tx_t *tx = rule_exec->tx;

    if (ib_flags_all(rule_exec->tx_log->flags, IB_RULE_LOG_FLAG_TX)) {
        rule_log_exec(rule_exec,
                      "TX_START %s:%d %s",
                      tx->conn->remote_ipstr,
                      tx->conn->remote_port,
                      tx->hostname);
    }
    return;
}

static void log_tx_end(
    const ib_rule_exec_t *rule_exec
)
{
    if ( (ib_flags_all(rule_exec->tx_log->flags, IB_RULE_LOG_FLAG_TX)) &&
         (!rule_exec->tx_log->empty_tx) )
    {
        rule_log_exec(rule_exec, "TX_END");
    }
    return;
}

static void log_tx_request_line(
    const ib_rule_exec_t *rule_exec
)
{
    ib_tx_t *tx = rule_exec->tx;

    if (ib_flags_all(rule_exec->tx_log->flags, IB_RULE_LOG_FLAG_REQ_LINE)) {
        if (tx->request_line == NULL) {
            /* Do nothing */
        }
        else if (ib_bytestr_length(tx->request_line->protocol) == 0) {
            rule_log_exec(rule_exec,
                          "REQ_LINE %.*s %.*s",
                          IB_BYTESTR_FMT_PARAM(tx->request_line->method),
                          IB_BYTESTR_FMT_PARAM(tx->request_line->uri));
        }
        else {
            rule_log_exec(rule_exec,
                          "REQ_LINE %.*s %.*s %.*s",
                          IB_BYTESTR_FMT_PARAM(tx->request_line->method),
                          IB_BYTESTR_FMT_PARAM(tx->request_line->uri),
                          IB_BYTESTR_FMT_PARAM(tx->request_line->protocol));
        }
    }
    return;
}

static void log_tx_response_line(
    const ib_rule_exec_t *rule_exec
)
{
    ib_tx_t *tx = rule_exec->tx;

    if (ib_flags_all(rule_exec->tx_log->flags, IB_RULE_LOG_FLAG_RSP_LINE)) {
        /* No response line means 0.9 */
        if (tx->response_line == NULL) {
            rule_log_exec(rule_exec, "RES_LINE HTTP/0.9");
        }
        else {
            rule_log_exec(rule_exec,
                          "RES_LINE %.*s %.*s %.*s",
                          IB_BYTESTR_FMT_PARAM(tx->response_line->protocol),
                          IB_BYTESTR_FMT_PARAM(tx->response_line->status),
                          IB_BYTESTR_FMT_PARAM(tx->response_line->msg));
        }
    }
    return;
}

static void log_tx_header(
    const ib_rule_exec_t *rule_exec,
    const char *label,
    const ib_parsed_headers_t *wrap
)
{
    ib_parsed_header_t *nvpair;

    for (nvpair = wrap->head;  nvpair != NULL;  nvpair = nvpair->next) {
        rule_log_exec(rule_exec,
                      "%s %.*s: %.*s",
                      label,
                      IB_BYTESTR_FMT_PARAM(nvpair->name),
                      IB_BYTESTR_FMT_PARAM(nvpair->value));
    }
}

static void log_tx_request_header(
    const ib_rule_exec_t *rule_exec
)
{
    if (ib_flags_all(rule_exec->tx_log->flags, IB_RULE_LOG_FLAG_REQ_HEADER)) {
        if (rule_exec->tx->request_header != NULL) {
            log_tx_header(rule_exec, "REQ_HEADER",
                          rule_exec->tx->request_header);
        }
    }
    return;
}

static void log_tx_response_header(
    const ib_rule_exec_t *rule_exec
)
{
    if (ib_flags_all(rule_exec->tx_log->flags, IB_RULE_LOG_FLAG_RSP_HEADER)) {
        if (rule_exec->tx->response_header != NULL) {
            log_tx_header(rule_exec, "RES_HEADER",
                          rule_exec->tx->response_header);
        }
    }
    return;
}

static void log_tx_body(
    const ib_rule_exec_t *rule_exec,
    const char *label,
    const ib_stream_t *body
)
{
    ib_sdata_t *sdata;
    ib_status_t rc;

    if (body == NULL) {
        return;
    }
    rc = ib_stream_peek(body, &sdata);
    if (rc != IB_OK) {
        return;
    }
    if (sdata->type == IB_STREAM_DATA) {
        char *buf;
        size_t buf_size = sdata->dlen * 2 + 3;

        buf = ib_mm_alloc(rule_exec->tx_log->mm, buf_size);
        if (buf == NULL) {
            return;
        }
        ib_string_escape_json_buf(sdata->data, sdata->dlen,
                                  buf, buf_size, NULL);
        if (rc == IB_OK) {
            rule_log_exec(rule_exec, "%s %zd %s",
                          label, sdata->dlen, buf);
        }
    }
    return;
}

static void log_tx_request_body(
    const ib_rule_exec_t *rule_exec
)
{
    if (ib_flags_all(rule_exec->tx_log->flags, IB_RULE_LOG_FLAG_REQ_BODY)) {
        log_tx_body(rule_exec, "REQ_BODY", rule_exec->tx->request_body);
    }
    return;
}

static void log_tx_response_body(
    const ib_rule_exec_t *rule_exec
)
{
    if (ib_flags_all(rule_exec->tx_log->flags, IB_RULE_LOG_FLAG_RSP_BODY)) {
        log_tx_body(rule_exec, "RES_BODY", rule_exec->tx->response_body);
    }
    return;
}

/* Log TX events (start of phase) */
void ib_rule_log_tx_event_start(
    const ib_rule_exec_t *rule_exec,
    ib_state_t state
)
{
    assert(rule_exec != NULL);
    assert(rule_exec->tx != NULL);

    if (rule_exec->tx_log == NULL) {
        return;
    }

    switch (state) {
        case handle_request_header_state:
            log_tx_start(rule_exec);
            log_tx_request_line(rule_exec);
            log_tx_request_header(rule_exec);
            rule_exec->tx_log->empty_tx = false;
            break;

        case handle_request_state:
            log_tx_request_body(rule_exec);
            rule_exec->tx_log->empty_tx = false;
            break;

        case handle_response_header_state:
            log_tx_response_line(rule_exec);
            log_tx_response_header(rule_exec);
            rule_exec->tx_log->empty_tx = false;
            break;

        case handle_response_state:
            log_tx_response_body(rule_exec);
            rule_exec->tx_log->empty_tx = false;
            break;

        default :
            break;       /* Do nothing */
    }
    return;
}

/* Log TX events (end of phase) */
void ib_rule_log_tx_event_end(
    const ib_rule_exec_t *rule_exec,
    ib_state_t state
)
{
    if (rule_exec->tx_log == NULL) {
        return;
    }

    switch (state) {
        case handle_logging_state:
            log_tx_end(rule_exec);
            break;

        default:
            break;       /* Do nothing */
    }
    return;
}

/* Log phase start */
void ib_rule_log_phase(
    const ib_rule_exec_t *rule_exec,
    ib_rule_phase_num_t phase_num,
    const char *phase_name,
    size_t num_rules
)
{
    ib_flags_t flags;

    if (rule_exec->tx_log == NULL) {
        return;
    }
    flags = rule_exec->tx_log->flags;

    if (phase_num != rule_exec->tx_log->cur_phase) {

        if (ib_flags_any(flags, IB_RULE_LOG_FLAG_PHASE)) {
            bool is_postprocess = (phase_num == IB_PHASE_POSTPROCESS);
            bool is_logging = (phase_num == IB_PHASE_LOGGING);
            bool empty_tx = rule_exec->tx_log->empty_tx;

            /* Inhibit logging of "PHASE: postprocess/logging" for empty tx */
            if
            (
                /* Log if we are not in post processing or logging phases. */
                ((!is_postprocess) && (!is_logging)) ||

                /* If we are in post processing or logging, do not
                 * log empty transactions. */
                ((num_rules != 0) || (!empty_tx))
            )
            {
                rule_log_exec(rule_exec, "PHASE %s", phase_name);
            }
            rule_exec->tx_log->cur_phase = phase_num;
            rule_exec->tx_log->phase_name = phase_name;
        }
    }
    return;
}

/**
 * Log a rule's transformations
 *
 * @param[in] mm Memory manager.
 * @param[in] rule_exec Rule execution object
 * @param[in] tgt Rule target logging object
 * @param[in] rslt Matching result field (or NULL)
 */
static void log_tfns(
    ib_mm_t                  mm,
    const ib_rule_exec_t    *rule_exec,
    const ib_rule_log_tgt_t *tgt,
    const ib_field_t        *rslt
)
{
    assert(rule_exec != NULL);
    assert(rule_exec->exec_log != NULL);
    assert(tgt != NULL);
    assert(tgt->tfn_list != NULL);
    assert(rule_exec->tx != NULL);

    const ib_list_node_t *tfn_node;
    ib_status_t           rc;

    if (ib_flags_all(rule_exec->tx_log->flags, IB_RULE_LOG_FLAG_TFN) == false) {
        return;
    }

    IB_LIST_LOOP_CONST(tgt->tfn_list, tfn_node) {
        char                    *buf;
        size_t                   sz;
        const   ib_list_node_t  *value_node;
        const ib_rule_log_tfn_t *tfn;

        tfn = (const ib_rule_log_tfn_t *)ib_list_node_data_const(tfn_node);

        if (ib_list_elements(tfn->value_list) > 0) {

            IB_LIST_LOOP_CONST(tfn->value_list, value_node) {

                const ib_rule_log_tfn_val_t *value =
                    (const ib_rule_log_tfn_val_t *)
                        ib_list_node_data_const(value_node);

                if ( (rslt != NULL) &&
                    ((rslt->nlen != value->in->nlen) ||
                    (memcmp(rslt->name, value->in->name, rslt->nlen) != 0)) )
                {
                    continue;
                }

                rc = ib_field_format_escape(mm, value->out, &buf, &sz);
                if (rc != IB_OK) {
                    return;
                }

                rule_log_exec(rule_exec,
                              "TFN %s() %s \"%.*s:%.*s\" %s %s",
                              ib_transformation_name(ib_transformation_inst_transformation(tfn->tfn_inst)),
                              ib_field_type_name(value->in->type),
                              (tgt->original ? (int)tgt->original->nlen : 0),
                              (tgt->original ? tgt->original->name : ""),
                              (int)value->in->nlen, value->in->name,
                              buf,
                              ( value->status == IB_OK ?
                                  "" : ib_status_to_string(value->status)));
            }
        }
        else {
            rc = ib_field_format_escape(mm,tfn->value.out, &buf, &sz);
            if (rc != IB_OK) {
                return;
            }

            if (tgt->original) {
                rule_log_exec(
                    rule_exec,
                    "TFN %s() %s \"%.*s\" %s %s",
                    ib_transformation_name(ib_transformation_inst_transformation(tfn->tfn_inst)),
                    ib_field_type_name(tgt->original->type),
                    (int)tgt->original->nlen,
                    tgt->original->name,
                    buf,
                    ( tfn->value.status == IB_OK ?
                        "" : ib_status_to_string(tfn->value.status))
                );
            }
        }
    }

    return;
}

/**
 * Log a rule result's actions
 *
 * @param[in] mm Memory manager to use.
 * @param[in] rule_exec Rule execution object
 * @param[in] rslt Rule result logging object
 */
static void log_actions(
    ib_mm_t mm,
    const ib_rule_exec_t *rule_exec,
    const ib_rule_log_rslt_t *rslt
)
{
    const ib_list_node_t *act_node;
    ib_rule_log_tx_t *tx_log;

    assert(rule_exec != NULL);
    assert(rule_exec->tx_log != NULL);
    assert(rslt != NULL);
    assert(rslt->act_list != NULL);

    tx_log = rule_exec->tx_log;

    if (ib_flags_all(tx_log->flags, IB_RULE_LOG_FLAG_ACTION) == false) {
        return;
    }

    IB_LIST_LOOP_CONST(rslt->act_list, act_node) {
        const ib_rule_log_act_t *act =
            (const ib_rule_log_act_t *)ib_list_node_data_const(act_node);
        const char *status =
            act->status == IB_OK ?"" : ib_status_to_string(act->status);

        rule_log_exec(
            rule_exec,
            "ACTION %s(%s) %s",
            ib_action_name(ib_action_inst_action(act->act_inst)),
            ib_action_inst_parameters(act->act_inst),
            status);
    }

    return;
}

/**
 * Log a rule result's events
 *
 * @param[in] rule_exec Rule execution object
 * @param[in] rslt Rule result logging object
 */
static void log_events(
    const ib_rule_exec_t *rule_exec,
    const ib_rule_log_rslt_t *rslt
)
{
    const ib_list_node_t *event_node;
    ib_rule_log_tx_t *tx_log;

    assert(rule_exec != NULL);
    assert(rule_exec->tx != NULL);
    assert(rule_exec->tx_log != NULL);
    assert(rslt != NULL);
    assert(rslt->event_list != NULL);

    tx_log = rule_exec->tx_log;

    if (ib_flags_all(tx_log->flags, IB_RULE_LOG_FLAG_EVENT) == false) {
        return;
    }

    IB_LIST_LOOP_CONST(rslt->event_list, event_node) {
        const ib_logevent_t *event =
            (const ib_logevent_t *)ib_list_node_data_const(event_node);

        if (event->msg == NULL) {
            rule_log_exec(rule_exec, "EVENT");
        }
        else {
            const char *tags;
            size_t      tags_size;
            ib_status_t rc;

            if (event->tags == NULL) {
                tags = "";
            }
            else {
                rc = ib_string_join(
                    ", ",
                    event->tags,
                    rule_exec->tx_log->mm,
                    &tags,
                    &tags_size);
                if (rc != IB_OK) {
                    ib_log_error_tx(
                        rule_exec->tx,
                        "Failed to join tag list. Tags not printed.");
                    tags = "[]";
                }
            }

            rule_log_exec(rule_exec,
                          "EVENT %s %s %s [%u/%u] [%s] \"%s\"",
                          event->rule_id,
                          ib_logevent_type_name(event->type),
                          ib_logevent_action_name(event->rec_action),
                          event->confidence, event->severity,
                          tags, event->msg);
        }
    }

    return;
}

/**
 * Log a rule result
 *
 * @param[in] mm Memory manager to allocate buffers out of.
 * @param[in] rule_exec Rule execution object
 * @param[in] tgt Rule target logging object
 * @param[in] rslt Rule result logging object
 */
static void log_result(
    ib_mm_t                   mm,
    const ib_rule_exec_t     *rule_exec,
    const ib_rule_log_tgt_t  *tgt,
    const ib_rule_log_rslt_t *rslt
)
{
    assert(rule_exec != NULL);
    assert(rule_exec->tx != NULL);
    assert(rule_exec->tx_log != NULL);
    assert(rule_exec->exec_log != NULL);
    assert(tgt != NULL);
    assert(rslt != NULL);

    char             *buf    = NULL;
    size_t            buf_sz;
    ib_rule_log_tx_t *tx_log = rule_exec->tx_log;
    ib_status_t       rc;

    if (ib_flags_all(tx_log->flags, IB_RULE_LOG_FLAG_TARGET) ) {
        if (rslt->value == NULL) {
            if (tgt->tfn_list != NULL) {
                log_tfns(mm, rule_exec, tgt, NULL);
            }
            if (tgt->original != NULL) {
                rule_log_exec(rule_exec,
                              "TARGET \"%s\" %s \"%.*s\" %s",
                              tgt->target->target_str,
                              "N/A",
                              (int)tgt->original->nlen,
                              tgt->original->name,
                              "NULL");
            }
        }
        else if (ib_rule_is_stream(rule_exec->rule) ) {
            if (tgt->tfn_list != NULL) {
                log_tfns(mm, rule_exec, tgt, NULL);
            }

            rc = ib_field_format_escape(mm, rslt->value, &buf, &buf_sz);
            if (rc != IB_OK) {
                return;
            }

            rule_log_exec(rule_exec,
                          "TARGET \"%s\" %s \"%.*s\" %s",
                          rule_exec->tx_log->phase_name,
                          ib_field_type_name(rslt->value->type),
                          (int)rslt->value->nlen, rslt->value->name,
                          buf);
        }
        else if ( (tgt->original != NULL) &&
                  (tgt->original->type == IB_FTYPE_LIST) &&
                  (rslt->value->type != IB_FTYPE_LIST) )
        {
            if (tgt->tfn_list != NULL) {
                log_tfns(mm, rule_exec, tgt, rslt->value);
            }

            rc = ib_field_format_escape(mm, rslt->value, &buf, &buf_sz);
            if (rc != IB_OK) {
                return;
            }

            rule_log_exec(rule_exec,
                          "TARGET \"%s\" %s \"%.*s:%.*s\" %s",
                          tgt->target->target_str,
                          ib_field_type_name(rslt->value->type),
                          (int)tgt->original->nlen, tgt->original->name,
                          (int)rslt->value->nlen, rslt->value->name,
                          buf);
        }
        else  {
            if (tgt->tfn_list != NULL) {
                log_tfns(mm, rule_exec, tgt, NULL);
            }

            rc = ib_field_format_escape(mm, rslt->value, &buf, &buf_sz);
            if (rc != IB_OK) {
                return;
            }

            rule_log_exec(rule_exec,
                          "TARGET \"%s\" %s \"%.*s\" %s",
                          tgt->target->target_str,
                          ib_field_type_name(rslt->value->type),
                          (int)rslt->value->nlen, rslt->value->name,
                          buf);
        }
    }

    if ( (tgt->original != NULL) &&
         (ib_flags_all(tx_log->flags, IB_RULE_LOG_FLAG_OPERATOR)) )
    {
        const char *is_inverted = (rule_exec->rule->opinst->invert)? "!":"";
        if (rslt->status == IB_OK) {
            const char *op_result = (rslt->result == 0) ? "FALSE" : "TRUE";

            rc = ib_field_format_quote(
                mm,
                rule_exec->exec_log->rule->opinst->fparam,
                &buf,
                &buf_sz);
            if (rc != IB_OK) {
                return;
            }

            rule_log_exec(
                rule_exec,
                "OP %s%s(%s) %s",
                is_inverted,
                ib_operator_name(ib_operator_inst_operator(rule_exec->exec_log->rule->opinst->opinst)),
                buf,
                op_result);
        }
        else {
            const char *error_status = ib_status_to_string(rslt->status);

            rc = ib_field_format_quote(
                mm,
                rule_exec->exec_log->rule->opinst->fparam,
                &buf,
                &buf_sz);
            if (rc != IB_OK) {
                return;
            }

            rule_log_exec(
                rule_exec,
                "OP %s%s(%s) ERROR %s",
                is_inverted,
                ib_operator_name(ib_operator_inst_operator(rule_exec->exec_log->rule->opinst->opinst)),
                buf,
                error_status);
        }
    }

    if (rslt->act_list != NULL) {
        log_actions(mm, rule_exec, rslt);
    }
    if (rslt->event_list != NULL) {
        log_events(rule_exec, rslt);
    }

    return;
}

static bool filter(
    const ib_rule_log_exec_t *exec_log,
    const ib_rule_log_count_t *counts
)
{
    if (ib_flags_all(exec_log->filter, IB_RULE_LOG_FILT_ALL) ) {
        return true;
    }

    if (ib_flags_all(exec_log->filter, IB_RULE_LOG_FILT_ACTIONABLE) ) {
        if (counts->act_count != 0) {
            return true;
        }
    }
    if (ib_flags_all(exec_log->filter, IB_RULE_LOG_FILT_OPEXEC) ) {
        if (counts->exec_count != 0) {
            return true;
        }
    }
    if (ib_flags_all(exec_log->filter, IB_RULE_LOG_FILT_ERROR) ) {
        if (counts->error_count != 0) {
            return true;
        }
    }
    if (ib_flags_all(exec_log->filter, IB_RULE_LOG_FILT_TRUE) ) {
        if (counts->true_count != 0) {
            return true;
        }
    }
    if (ib_flags_all(exec_log->filter, IB_RULE_LOG_FILT_FALSE) ) {
        if (counts->false_count != 0) {
            return true;
        }
    }
    return false;
}

/* Log rule execution: exec. */
void ib_rule_log_execution(
    const ib_rule_exec_t *rule_exec
)
{
    assert(rule_exec != NULL);
    const ib_list_node_t *tgt_node;
    const ib_rule_log_exec_t *exec_log = rule_exec->exec_log;
    const ib_rule_log_tx_t *tx_log;
    const ib_rule_t *rule;
    ib_mpool_lite_t  *mpl   = NULL;
    ib_mm_t           mpl_mm;
    ib_status_t       rc;

    rc = ib_mpool_lite_create(&mpl);
    if (rc != IB_OK) {
        return;
    }
    mpl_mm = ib_mm_mpool_lite(mpl);

    if ( (exec_log == NULL) || (exec_log->rule == NULL) ) {
        goto cleanup;
    }

    tx_log = rule_exec->tx_log;
    if (filter(exec_log, &exec_log->counts) == false) {
        goto cleanup;
    }

    rule = exec_log->rule;

    /*
     * Log the rule start and/or data
     */
    if (ib_flags_all(tx_log->flags, IB_RULE_LOG_FLAG_RULE)) {
        const char *rule_type;
        if (ib_flags_all(exec_log->rule->flags, IB_RULE_FLAG_STREAM)) {
            rule_type = "STREAM";
        }
        else if (ib_flags_all(exec_log->rule->flags, IB_RULE_FLAG_EXTERNAL)) {
            rule_type = "EXTERNAL";
        }
        else {
            rule_type = "PHASE";
        }

        rule_log_exec(rule_exec, "RULE_START %s %s",
                      rule_type,
                      ib_rule_phase_name(rule_exec->phase));
    }

    /*
     * Log all of the targets whose result that matched the result type.
     */
    if (exec_log->tgt_list != NULL) {
        IB_LIST_LOOP_CONST(exec_log->tgt_list, tgt_node) {
            const ib_rule_log_tgt_t *tgt =
                (const ib_rule_log_tgt_t *)ib_list_node_data_const(tgt_node);
            const ib_list_node_t *rslt_node;
            assert(tgt != NULL);

            if (filter(exec_log, &tgt->counts) == false) {
                continue;
            }

            if (ib_flags_all(tx_log->flags, IB_RULE_LOG_FLAG_TARGET)) {
                bool allow_null = ib_flags_all(
                    ib_operator_capabilities(ib_operator_inst_operator(rule->opinst->opinst)),
                    IB_OP_CAPABILITY_ALLOW_NULL
                );
                if ( (tgt->original == NULL) && (allow_null == false) ) {
                    rule_log_exec(rule_exec,
                                  "TARGET %s NOT_FOUND",
                                  tgt->target->target_str);
                }
            }

            if (tgt->rslt_list != NULL) {
                IB_LIST_LOOP_CONST(tgt->rslt_list, rslt_node) {
                    const ib_rule_log_rslt_t *rslt =
                        (const ib_rule_log_rslt_t *)
                            ib_list_node_data_const(rslt_node);
                    log_result(mpl_mm, rule_exec, tgt, rslt);
                }
            }
        }
    }

    if (ib_flags_all(tx_log->flags, IB_RULE_LOG_FLAG_RULE)) {
        rule_log_exec(rule_exec, "RULE_END");
    }

    if (ib_flags_all(exec_log->flags, IB_RULE_EXEC_FATAL)) {
        ib_rule_log_error(rule_exec, "Fatal rule execution error");
        assert(0 && "Fatal rule execution error");
    }

cleanup:
    ib_mpool_lite_destroy(mpl);
    return;
}
