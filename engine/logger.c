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
 * @brief IronBee --- Logger
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/logger.h>

#include <assert.h>
#include <stdlib.h>

typedef ib_status_t (*writer_fn)(
    ib_logger_t *logger,
    ib_logger_writer_t *writer,
    void *cbdata);

/**
 * Iterate over every @ref ib_logger_writer_t in @a logger and apply @a fn.
 *
 * @param[in] logger The logger.
 * @param[in] fn The function to apply to every @ref ib_logger_writer_t.
 *
 * @returns
 * - IB_OK On success.
 * - The first non-IB_OK code returned by @a fn. The @a fn function is
 *   always applied to all writers.
 */
static ib_status_t for_each_writer(
    ib_logger_t *logger,
    writer_fn fn,
    void *data
)
{
    assert(logger != NULL);

    ib_status_t     rc = IB_OK;
    ib_list_node_t *node;

    IB_LIST_LOOP(logger->writers, node) {
        ib_logger_writer_t *writer =
            (ib_logger_writer_t *)ib_list_node_data(node);
        if (fn != NULL) {
            ib_status_t trc = fn(logger, writer, data);

            if (trc != IB_OK && rc == IB_OK) {
                rc = trc;
            }
        }
    }

    return rc;
}

/**
 * Structure to hold the user's log message to format.
 */
typedef struct logger_write_cbdata_t {
    const uint8_t   *msg;    /**< The message. */
    size_t           msg_sz; /**< The size of the message. */
    ib_logger_rec_t *rec;    /**< Record used to format. */
} logger_write_cbdata_t;

static const size_t MAX_QUEUE_DEPTH = 1000;

/**
 * The implementation for logger_log().
 *
 * This fucntion will
 * - Format the message stored in @a cbdata as a @ref logger_write_cbdata_t.
 * - Lock the message queue.
 * - Enqueue the formatted message.
 * - If the message is the only message in the queue,
 *   ib_logger_writer_t::record_fn is called to signal the
 *   writer that at least one record is available.
 *
 * @param[in] logger The logger.
 * @param[in] writer The logger writer to send the message to.
 * @param[in] cbdata @ref logger_write_cbdata_t containing the user's message.
 *
 * @returns
 * - IB_OK On success.
 * - Other on implementation failure.
 */
static ib_status_t logger_write(
    ib_logger_t *logger,
    ib_logger_writer_t *writer,
    void *cbdata
)
{
    ib_status_t rc;
    void *rec = NULL;
    logger_write_cbdata_t *logger_write_data = (logger_write_cbdata_t *)cbdata;

    rc = writer->format_fn(
        logger,
        logger_write_data->rec,
        logger_write_data->msg,
        logger_write_data->msg_sz,
        &rec,
        writer->format_data);
    if (rc == IB_DECLINED) {
        return IB_OK;
    }
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_lock_lock(&(writer->records_lck));
    if (rc != IB_OK) {
        return rc;
    }

    /* Busy-wait until the queue has space available.
     * This is emergency code to avoid a crash at the cost of a slowdown. */
    while (ib_queue_size(writer->records) >= MAX_QUEUE_DEPTH) {
        rc = ib_lock_unlock(&(writer->records_lck));
        if (rc != IB_OK) {
            return rc;
        }

        /* TODO - The number of times we need to sleep should be 
         *        audited. It is a good indicator of excessive logging or
         *        proxy load. */
        sleep(1);
        rc = ib_lock_lock(&(writer->records_lck));
        if (rc != IB_OK) {
            return rc;
        }
    }

    rc = ib_queue_push_front(writer->records, rec);
    if (rc != IB_OK) {
        return rc;
    }

    if (ib_queue_size(writer->records) == 1) {
        rc = writer->record_fn(logger, writer, writer->record_data);
        if (rc != IB_OK) {
            goto locked_exit;
        }
    }

locked_exit:
    ib_lock_unlock(&(writer->records_lck));
    return rc;
}

/**
 * Deterimine if the logger message should be filtered (ignored) or not.
 *
 * This is common filtering code. While it seems simple now, it is
 * expected to grow as features are added to the logger api.
 *
 * @param[in] logger The logger.
 * @param[in] level The level of the message.
 *
 * @returns
 * - True if the message should be discarded.
 * - Faulse if the message should not be discarded.
 */
static bool logger_filter(
    ib_logger_t *logger,
    ib_log_level_t level
)
{
    assert(logger != NULL);

    if (level > logger->level) {
        return false;
    }

    return true;
}

/**
 * Actually perform the logging.
 */
static void logger_log(
    ib_logger_t       *logger,
    ib_logger_rec_t   *rec,
    const uint8_t     *msg,
    size_t             msg_sz
)
{
    assert(logger != NULL);

    logger_write_cbdata_t logger_write_data = { msg, msg_sz, rec };

    /* For each logger,
     * - format the log message
     * - enqueue the log message
     * - signal the log writer if it was waiting on an empty queue.
     */
    for_each_writer(logger, logger_write, &logger_write_data);
}

void ib_logger_log_msg(
    ib_logger_t       *logger,
    size_t             line_number,
    const char        *file,
    const char        *function,
    ib_engine_t       *engine,
    ib_module_t       *module,
    ib_conn_t         *conn,
    ib_tx_t           *tx,
    ib_log_level_t     level,
    const uint8_t     *msg,
    size_t             msg_sz,
    ib_logger_msg_fn_t msg_fn,
    void              *msg_fn_data
)
{
    ib_status_t      rc;
    uint8_t         *log_msg;    /* Final message. */
    size_t           log_msg_sz;
    uint8_t         *fn_msg;     /* Message generated by msg_fn. */
    size_t           fn_msg_sz;
    ib_logger_rec_t  rec;
    ib_mpool_t      *mp = NULL;

    if (logger_filter(logger, level)) {
        return;
    }

    rc = ib_mpool_create(&mp, "Temporary Logger MP", logger->mp);
    if (rc != IB_OK) {
        return;
    }

    rec.line_number = line_number;
    rec.file        = file;
    rec.function    = function;
    rec.timestamp   = ib_clock_get_time();
    rec.module      = module;
    rec.conn        = conn;
    rec.tx          = tx;
    rec.engine      = engine;
    rec.level       = level;

    /* Build the message using the user's function. */
    rc = msg_fn(&rec, mp, &fn_msg, &fn_msg_sz, msg_fn_data);
    if (rc != IB_OK) {
        return;
    }

    /* Do not log empty messages. */
    if (msg_sz + fn_msg_sz == 0) {
        return;
    }

    log_msg_sz = msg_sz + fn_msg_sz;
    log_msg = ib_mpool_alloc(mp, log_msg_sz);
    if (log_msg == NULL) {
        return;
    }

    /* Build msg. */
    memcpy(log_msg, msg, msg_sz);
    memcpy(log_msg+msg_sz, fn_msg, fn_msg_sz);

    logger_log(logger, &rec, log_msg, log_msg_sz);

    ib_mpool_destroy(mp);
}

void ib_logger_log_va(
    ib_logger_t       *logger,
    size_t             line_number,
    const char        *file,
    const char        *function,
    ib_engine_t       *engine,
    ib_module_t       *module,
    ib_conn_t         *conn,
    ib_tx_t           *tx,
    ib_log_level_t     level,
    const char        *msg,
    ...
)
{
    ib_status_t      rc;
    uint8_t         *log_msg;    /* Final message. */
    size_t           log_msg_sz;
    ib_logger_rec_t  rec;
    ib_mpool_t      *mp = NULL;
    const size_t     buffer_sz = 1024;
    va_list ap;

    if (logger_filter(logger, level)) {
        return;
    }

    rc = ib_mpool_create(&mp, "Temporary Logger MP", logger->mp);
    if (rc != IB_OK) {
        return;
    }

    log_msg = ib_mpool_alloc(mp, buffer_sz);
    if (log_msg == NULL) {
        return;
    }

    va_start(ap, msg);
    log_msg_sz = vsnprintf((char *)log_msg, buffer_sz, msg, ap);
    va_end(ap);

    rec.line_number = line_number;
    rec.file        = file;
    rec.function    = function;
    rec.timestamp   = ib_clock_get_time();
    rec.module      = module;
    rec.conn        = conn;
    rec.tx          = tx;
    rec.engine      = engine;
    rec.level       = level;

    logger_log(logger, &rec, log_msg, log_msg_sz);

    ib_mpool_destroy(mp);
}

ib_status_t ib_logger_create(
    ib_logger_t    **logger,
    ib_log_level_t   level,
    ib_mpool_t      *mp
)
{
    assert(logger != NULL);
    assert(mp != NULL);

    ib_logger_t *l;
    ib_status_t  rc;

    l = (ib_logger_t *)ib_mpool_alloc(mp, sizeof(*l));
    if (l == NULL) {
        return IB_EALLOC;
    }

    l->level = level;
    l->mp = mp;
    rc = ib_list_create(&(l->writers), mp);
    if (rc != IB_OK) {
        return rc;
    }

    *logger = l;
    return IB_OK;
}

ib_status_t ib_logger_writer_add(
    ib_logger_t           *logger,
    ib_logger_open_fn      open_fn,
    void                  *open_data,
    ib_logger_close_fn     close_fn,
    void                  *close_data,
    ib_logger_reopen_fn    reopen_fn,
    void                  *reopen_data,
    ib_logger_format_fn_t  format_fn,
    void                  *format_data,
    ib_logger_record_fn_t  record_fn,
    void                  *record_data
)
{
    assert(logger != NULL);
    assert(logger->mp != NULL);
    assert(logger->writers != NULL);

    ib_status_t         rc;
    ib_logger_writer_t *writer;

    writer = (ib_logger_writer_t *)ib_mpool_alloc(logger->mp, sizeof(writer));
    if (writer == NULL) {
        return IB_EALLOC;
    }

    writer->open_fn     = open_fn;
    writer->open_data   = open_data;
    writer->close_fn    = close_fn;
    writer->close_data  = close_data;
    writer->reopen_fn   = reopen_fn;
    writer->reopen_data = reopen_data;
    writer->format_fn   = format_fn;
    writer->format_data = format_data;
    writer->record_fn   = record_fn;
    writer->record_data = record_data;
    rc = ib_queue_create(&(writer->records), logger->mp, IB_QUEUE_NEVER_SHRINK);
    if (rc != IB_OK) {
        return rc;
    }
    rc = ib_lock_init(&(writer->records_lck));
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_list_push(logger->writers, writer);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_logger_writer_clear(
    ib_logger_t *logger
)
{
    assert(logger != NULL);
    assert(logger->writers != NULL);

    ib_list_clear(logger->writers);

    return IB_OK;
}

/**
 * Implementation for ib_logger_open.
 *
 * @param[in] logger The logger.
 * @param[in] The writer to perform the function on.
 * @param[in] data Callback data. NULL.
 *
 * @returns
 * - IB_OK On success.
 * - Other on implementation failure.
 */
static ib_status_t logger_open(
    ib_logger_t        *logger,
    ib_logger_writer_t *writer,
    void               *data
)
{
    assert(logger != NULL);
    assert(writer != NULL);
    assert(data == NULL);

    if (writer->open_fn == NULL) {
        return IB_OK;
    }

    return writer->open_fn(logger, writer->open_data);
}

ib_status_t ib_logger_open(
    ib_logger_t *logger
)
{
    assert(logger != NULL);

    return for_each_writer(logger, logger_open, NULL);
}

/**
 * Implementation for ib_logger_close.
 *
 * @param[in] logger The logger.
 * @param[in] The writer to perform the function on.
 * @param[in] data Callback data. NULL.
 *
 * @returns
 * - IB_OK On success.
 * - Other on implementation failure.
 */
static ib_status_t logger_close(
    ib_logger_t        *logger,
    ib_logger_writer_t *writer,
    void               *data
)
{
    assert(logger != NULL);
    assert(writer != NULL);
    assert(data == NULL);

    if (writer->close_fn == NULL) {
        return IB_OK;
    }

    return writer->close_fn(logger, writer->close_data);
}

ib_status_t ib_logger_close(
    ib_logger_t *logger
)
{
    assert(logger != NULL);

    return for_each_writer(logger, logger_close, NULL);
}

/**
 * Implementation for ib_logger_reopen.
 *
 * @param[in] logger The logger.
 * @param[in] The writer to perform the function on.
 * @param[in] data Callback data. NULL.
 *
 * @returns
 * - IB_OK On success.
 * - Other on implementation failure.
 */
static ib_status_t logger_reopen(
    ib_logger_t        *logger,
    ib_logger_writer_t *writer,
    void               *data
)
{
    assert(logger != NULL);
    assert(writer != NULL);
    assert(data == NULL);

    if (writer->reopen_fn == NULL) {
        return IB_OK;
    }

    return writer->reopen_fn(logger, writer->reopen_data);
}

ib_status_t ib_logger_reopen(
    ib_logger_t *logger
)
{
    assert(logger != NULL);

    return for_each_writer(logger, logger_reopen, NULL);
}

ib_status_t ib_logger_dequeue(
    ib_logger_t        *logger,
    ib_logger_writer_t *writer,
    void               *msg
)
{
    assert(logger != NULL);
    assert(writer != NULL);
    assert(writer->records != NULL);

    ib_status_t rc;
    size_t      q_sz;

    rc = ib_lock_lock(&(writer->records_lck));
    if (rc != IB_OK) {
        return rc;
    }

    q_sz = ib_queue_size(writer->records);
    if (q_sz > 0) {
        rc = ib_queue_pop_front(writer->records, msg);
    }
    else {
        rc = IB_ENOENT;
    }

    ib_lock_unlock(&(writer->records_lck));
    return rc;
}

size_t ib_logger_writer_count(ib_logger_t *logger) {
    assert(logger != NULL);
    assert(logger->writers != NULL);

    return ib_list_elements(logger->writers);
}

ib_log_level_t ib_logger_level(ib_logger_t *logger) {
    assert(logger != NULL);

    return logger->level;
}

/**
 * Default logger message structure.
 */
typedef struct default_logger_msg_t {
    uint8_t *prefix; /**< Prefix of the log message. A string. */
    uint8_t *msg;    /**< User's logging data. */
    size_t   msg_sz; /**< The message length. */
} default_logger_msg_t;

/**
 * Default logger configuration.
 */
typedef struct default_logger_cfg_t {
    FILE * file; /**< File to log to. */
} default_logger_cfg_t;

/**
 * The default logger format function.
 */
static ib_status_t default_logger_format(
    ib_logger_t     *logger,
    ib_logger_rec_t *rec,
    const uint8_t   *log_msg,
    const size_t     log_msg_sz,
    void            *writer_record,
    void            *data
)
{
    assert(logger != NULL);
    assert(rec != NULL);
    assert(log_msg != NULL);
    assert(writer_record != NULL);
    assert(data != NULL);

    char      *msg_prefix;
    char       time_info[32 + 1];
    struct tm *tminfo;
    time_t     timet;
    default_logger_cfg_t *cfg = (default_logger_cfg_t *)data;
    default_logger_msg_t *msg = malloc(sizeof(*msg));

    if (msg == NULL) {
        goto out_of_mem;
    }

    timet = time(NULL);
    tminfo = localtime(&timet);
    strftime(time_info, sizeof(time_info)-1, "%d%m%Y.%Hh%Mm%Ss", tminfo);

    /* 100 is more than sufficient. */
    msg_prefix = (char *)malloc(strlen(time_info) + 100);
    if (msg_prefix == NULL) {
        goto out_of_mem;
    }

    sprintf(
        msg_prefix,
        "%s %-10s- ",
        time_info,
        ib_log_level_to_string(rec->level));

    if ( (rec->file != NULL) && (rec->line_number > 0) ) {
        const char *file = rec->file;
        size_t flen;
        while (strncmp(file, "../", 3) == 0) {
            file += 3;
        }
        flen = strlen(file);
        if (flen > 23) {
            file += (flen - 23);
        }

        static const size_t c_line_info_length = 35;
        char line_info[c_line_info_length];
        snprintf(
            line_info,
            c_line_info_length,
            "(%23s:%-5d) ",
            file,
            (int)rec->line_number
        );
        strcat(msg_prefix, line_info);
    }

    msg->msg_sz = log_msg_sz;
    msg->msg = malloc(log_msg_sz);
    if (msg->msg == NULL) {
        goto out_of_mem;
    }
    memcpy(msg->msg, log_msg, log_msg_sz);

    msg->prefix = malloc(strlen(msg_prefix));
    if (msg->prefix == NULL) {
        goto out_of_mem;
    }

    *(void **)writer_record = msg;
    free(msg_prefix);
    return IB_OK;
out_of_mem:
    fprintf(cfg->file, "Out of memory.  Unable to log.");
    fflush(cfg->file);
    return IB_EALLOC;

}

/**
 * The default logger's record call.
 */
static ib_status_t default_logger_record(
    ib_logger_t        *logger,
    ib_logger_writer_t *writer,
    void               *data
)
{
    assert(logger != NULL);
    assert(writer != NULL);
    assert(data != NULL);

    ib_status_t           rc;
    default_logger_msg_t *msg = NULL;
    default_logger_cfg_t *cfg = (default_logger_cfg_t *)data;

    for (
        rc = ib_logger_dequeue(logger, writer, &msg);
        rc == IB_OK;
        rc = ib_logger_dequeue(logger, writer, &msg)
        )
    {
        fprintf(
            cfg->file,
            "%s %.*s\n",
            msg->prefix,
            (int)msg->msg_sz,
            (char *)msg->msg);
        fflush(cfg->file);
        free(msg->msg);
        free(msg->prefix);
        free(msg);
    }

    /* Check for an unexpected failure. */
    if (rc != IB_ENOENT) {
        return rc;
    }
    return IB_OK;
}

ib_status_t ib_logger_writer_add_default(
    ib_logger_t *logger
)
{
    assert(logger != NULL);
    assert(logger->mp != NULL);

    default_logger_cfg_t *cfg;

    cfg = ib_mpool_alloc(logger->mp, sizeof(*cfg));
    if (cfg == NULL) {
        return IB_EALLOC;
    }

    cfg->file = stderr;

    return ib_logger_writer_add(
        logger,
        NULL, /* Open. */
        NULL,
        NULL, /* Close. */
        NULL,
        NULL, /* Reopen. */
        NULL,
        default_logger_format,
        cfg,
        default_logger_record,
        cfg
    );
}
