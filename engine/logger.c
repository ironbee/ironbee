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
#include <ironbee/string.h>

#include <assert.h>
#include <stdlib.h>

/**
 * A collection of callbacks and function pointer that implement a logger.
 */
struct ib_logger_writer_t {
    ib_logger_open_fn      open_fn;     /**< Open the logger. */
    void                  *open_data;   /**< Callback data. */
    ib_logger_close_fn     close_fn;    /**< Close logs files. */
    void                  *close_data;  /**< Callback data. */
    ib_logger_reopen_fn    reopen_fn;   /**< Close and reopen log files. */
    void                  *reopen_data; /**< Callback data. */
    ib_logger_format_fn_t  format_fn;   /**< Signal that the queue has data. */
    void                  *format_data; /**< Callback data. */
    ib_logger_record_fn_t  record_fn;   /**< Signal a record is ready. */
    void                  *record_data; /**< Callback data. */
    ib_queue_t            *records;     /**< Records for the log writer. */
    ib_lock_t              records_lck; /**< Guard the queue. */
};

/**
 * A logger is what @ref ib_logger_rec_t are submitted to to produce a log.
 */
struct ib_logger_t {
    ib_logger_level_t    level;       /**< The log level. */

    /**
     * Memory pool with a lifetime of the logger.
     */
    ib_mpool_t          *mp;

    /**
     * List of @ref ib_logger_writer_t.
     *
     * A logger, by itself, cannot log anything. The writers
     * implement the actual logging functionality. This list is the list
     * of all writers that this logger will send messages to.
     *
     * Writers also get notified of flush, open, close, and reopen events.
     */
    ib_list_t           *writers;
};

/**
 * Writer function.
 *
 * @param[in] logger The logger.
 * @param[in] writer The writer.
 * @param[in] cbdata Callback data.
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
typedef ib_status_t (*writer_fn)(
    ib_logger_t *logger,
    ib_logger_writer_t *writer,
    void *cbdata);

/**
 * Iterate over every @ref ib_logger_writer_t in @a logger and apply @a fn.
 *
 * @param[in] logger The logger.
 * @param[in] fn The function to apply to every @ref ib_logger_writer_t.
 * @param[in] data Callback data.
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

    ib_status_t           rc = IB_OK;
    const ib_list_node_t *node;

    IB_LIST_LOOP_CONST(logger->writers, node) {
        ib_logger_writer_t *writer =
            (ib_logger_writer_t *)ib_list_node_data_const(node);
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

/**
 * The maximum depth of a message queue in a @ref ib_logger_writer_t.
 */
static const size_t MAX_QUEUE_DEPTH = 1000;

/**
 * The implementation for logger_log().
 *
 * This function will
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
    if (rc == IB_DECLINED || rec == NULL) {
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

    /* If the queue is size=1, unlock and notify writers. */
    if (ib_queue_size(writer->records) == 1) {
        ib_lock_unlock(&(writer->records_lck));
        rc = writer->record_fn(logger, writer, writer->record_data);
        return rc;
    }

    ib_lock_unlock(&(writer->records_lck));
    return rc;
}

/**
 * Determine if the logger message should be filtered (ignored) or not.
 *
 * This is common filtering code. While it seems simple now, it is
 * expected to grow as features are added to the logger api.
 *
 * @param[in] logger The logger.
 * @param[in] level The level of the message.
 *
 * @returns
 * - True if the message should be discarded.
 * - False if the message should not be discarded.
 */
static bool logger_filter(
    ib_logger_t       *logger,
    ib_logger_level_t  level
)
{
    assert(logger != NULL);

    if (level <= logger->level) {
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
    const char        *file,
    const char        *function,
    size_t             line_number,
    const ib_engine_t *engine,
    const ib_module_t *module,
    const ib_conn_t   *conn,
    const ib_tx_t     *tx,
    ib_logger_level_t  level,
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
        goto exit;
    }

    /* Do not log empty messages. */
    if (msg_sz + fn_msg_sz == 0) {
        goto exit;
    }

    log_msg_sz = msg_sz + fn_msg_sz;
    log_msg = ib_mpool_alloc(mp, log_msg_sz);
    if (log_msg == NULL) {
        goto exit;
    }

    /* Build msg. */
    memcpy(log_msg, msg, msg_sz);
    memcpy(log_msg+msg_sz, fn_msg, fn_msg_sz);

    logger_log(logger, &rec, log_msg, log_msg_sz);

exit:
    ib_mpool_release(mp);
}

void ib_logger_log_va(
    ib_logger_t       *logger,
    const char        *file,
    const char        *function,
    size_t             line_number,
    const ib_engine_t *engine,
    const ib_module_t *module,
    const ib_conn_t   *conn,
    const ib_tx_t     *tx,
    ib_logger_level_t  level,
    const char        *msg,
    ...
)
{
    va_list ap;

    va_start(ap, msg);
    ib_logger_log_va_list(
        logger,
        file,
        function,
        line_number,
        engine,
        module,
        conn,
        tx,
        level,
        msg,
        ap
    );
    va_end(ap);
}

void ib_logger_log_va_list(
    ib_logger_t       *logger,
    const char        *file,
    const char        *function,
    size_t             line_number,
    const ib_engine_t *engine,
    const ib_module_t *module,
    const ib_conn_t   *conn,
    const ib_tx_t     *tx,
    ib_logger_level_t  level,
    const char        *msg,
    va_list            ap
)
{
    ib_status_t      rc;
    uint8_t         *log_msg;    /* Final message. */
    size_t           log_msg_sz = 1024;
    ib_logger_rec_t  rec;
    ib_mpool_t      *mp = NULL;

    if (logger_filter(logger, level)) {
        return;
    }

    rc = ib_mpool_create(&mp, "Temporary Logger MP", logger->mp);
    if (rc != IB_OK) {
        return;
    }

    log_msg = ib_mpool_alloc(mp, log_msg_sz);
    if (log_msg == NULL) {
        ib_mpool_release(mp);
        return;
    }

    log_msg_sz = vsnprintf((char *)log_msg, log_msg_sz, msg, ap);

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

    ib_mpool_release(mp);
}

ib_status_t ib_logger_create(
    ib_logger_t       **logger,
    ib_logger_level_t   level,
    ib_mpool_t         *mp
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

    writer = (ib_logger_writer_t *)ib_mpool_alloc(logger->mp, sizeof(*writer));
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
 * Implementation for ib_logger_open().
 *
 * @param[in] logger The logger.
 * @param[in] writer The writer to perform the function on.
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
 * Implementation for ib_logger_close().
 *
 * @param[in] logger The logger.
 * @param[in] writer The writer to perform the function on.
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
 * Implementation for ib_logger_reopen().
 *
 * @param[in] logger The logger.
 * @param[in] writer The writer to perform the function on.
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
    ib_logger_t           *logger,
    ib_logger_writer_t    *writer,
    ib_queue_element_fn_t  handler,
    void                  *cbdata
)
{
    assert(logger != NULL);
    assert(writer != NULL);
    assert(writer->records != NULL);

    ib_status_t rc;

    rc = ib_lock_lock(&(writer->records_lck));
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_queue_dequeue_all_to_function(writer->records, handler, cbdata);

    ib_lock_unlock(&(writer->records_lck));

    return rc;
}

size_t ib_logger_writer_count(ib_logger_t *logger) {
    assert(logger != NULL);
    assert(logger->writers != NULL);

    return ib_list_elements(logger->writers);
}

ib_logger_level_t ib_logger_level_get(ib_logger_t *logger) {
    assert(logger != NULL);

    return logger->level;
}

void ib_logger_level_set(ib_logger_t *logger, ib_logger_level_t level) {
    assert(logger != NULL);

    logger->level = level;
}

void ib_logger_standard_msg_free(ib_logger_standard_msg_t *msg) {
    if (msg != NULL) {
        if (msg->prefix != NULL) {
            free(msg->prefix);
        }
        if (msg->msg != NULL) {
            free(msg->msg);
        }
        free(msg);
    }
}

/**
 * Default logger configuration.
 */
typedef struct default_logger_cfg_t {
    FILE * file; /**< File to log to. */
} default_logger_cfg_t;

ib_status_t ib_logger_standard_formatter(
    ib_logger_t           *logger,
    const ib_logger_rec_t *rec,
    const uint8_t         *log_msg,
    const size_t           log_msg_sz,
    void                  *writer_record,
    void                  *data
)
{
    assert(logger != NULL);
    assert(rec != NULL);
    assert(log_msg != NULL);
    assert(writer_record != NULL);

    char                      time_info[32 + 1];
    struct tm                *tminfo;
    time_t                    timet;
    ib_logger_standard_msg_t *msg;

    msg = malloc(sizeof(*msg));
    if (msg == NULL) {
        goto out_of_mem;
    }

    msg->prefix = NULL;
    msg->msg = NULL;

    timet = time(NULL);
    tminfo = localtime(&timet);
    strftime(time_info, sizeof(time_info)-1, "%d%m%Y.%Hh%Mm%Ss", tminfo);

    /* 100 is more than sufficient. */
    msg->prefix = (char *)malloc(strlen(time_info) + 100);
    if (msg->prefix == NULL) {
        goto out_of_mem;
    }

    sprintf(
        msg->prefix,
        "%s %-10s- ",
        time_info,
        ib_logger_level_to_string(rec->level));

    /* Add the file name and line number if available and log level >= DEBUG */
    if ( (rec->file != NULL) &&
         (rec->line_number > 0) &&
         (logger->level >= IB_LOG_DEBUG) )
    {
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
        strcat(msg->prefix, line_info);
    }

    /* If this is a transaction, add the TX id */
    if (rec->tx != NULL) {
        static const size_t c_line_info_size = 43;
        char                line_info[c_line_info_size];

        strcpy(line_info, "[tx:");
        strcat(line_info, rec->tx->id);
        strcat(line_info, "] ");
        strcat(msg->prefix, line_info);
    }

    msg->msg_sz = log_msg_sz;
    msg->msg = malloc(log_msg_sz);
    if (msg->msg == NULL) {
        goto out_of_mem;
    }
    memcpy(msg->msg, log_msg, log_msg_sz);

    *(ib_logger_standard_msg_t **)writer_record = msg;
    return IB_OK;

out_of_mem:
    ib_logger_standard_msg_free(msg);
    return IB_EALLOC;
}

/**
 * The default logger format function.
 *
 * This wraps ib_logger_standard_formatter() and reports errors to the
 * log file defined by @a data.
 *
 * param[in] logger The logger.
 * param[in] rec The record.
 * param[in] log_msg The user's log message.
 * param[in] log_msg_sz The length of @a log_msg.
 * param[in] writer_record A @a ib_logger_standard_msg_t will be written here
 *           on success.
 * param[in] data A @a default_logger_cfg_t holding default logger
 *           configuration information.
 */
static ib_status_t default_logger_format(
    ib_logger_t           *logger,
    const ib_logger_rec_t *rec,
    const uint8_t         *log_msg,
    const size_t           log_msg_sz,
    void                  *writer_record,
    void                  *data
)
{
    assert(logger != NULL);
    assert(rec != NULL);
    assert(log_msg != NULL);
    assert(writer_record != NULL);
    assert(data != NULL);

    ib_status_t rc;
    default_logger_cfg_t *cfg = (default_logger_cfg_t *)data;

    rc = ib_logger_standard_formatter(
        logger,
        rec,
        log_msg,
        log_msg_sz,
        writer_record,
        data);

    if (rc == IB_EALLOC) {
        ib_logger_standard_msg_free(
            *(ib_logger_standard_msg_t **)writer_record);
        fprintf(cfg->file, "Out of memory.  Unable to log.");
        fflush(cfg->file);
    }
    else if (rc != IB_OK) {
        ib_logger_standard_msg_free(
            *(ib_logger_standard_msg_t **)writer_record);
        fprintf(cfg->file, "Unexpected error.");
        fflush(cfg->file);
    }

    return rc;
}

static void default_log_writer(void *record, void *cbdata) {
    assert(record != NULL);
    assert(cbdata != NULL);

    default_logger_cfg_t     *cfg = (default_logger_cfg_t *)cbdata;
    ib_logger_standard_msg_t *msg = (ib_logger_standard_msg_t *)record;

    fprintf(
        cfg->file,
        "%s %.*s\n",
        msg->prefix,
        (int)msg->msg_sz,
        (char *)msg->msg);

    ib_logger_standard_msg_free(msg);
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

    return ib_logger_dequeue(logger, writer, default_log_writer, data);
}

ib_status_t ib_logger_writer_add_default(
    ib_logger_t *logger,
    FILE        *logfile
)
{
    assert(logger != NULL);
    assert(logger->mp != NULL);

    default_logger_cfg_t *cfg;

    cfg = ib_mpool_alloc(logger->mp, sizeof(*cfg));
    if (cfg == NULL) {
        return IB_EALLOC;
    }

    cfg->file = logfile;

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

static const char* c_log_levels[] = {
    "EMERGENCY",
    "ALERT",
    "CRITICAL",
    "ERROR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG",
    "DEBUG2",
    "DEBUG3",
    "TRACE"
};
static size_t c_num_levels = sizeof(c_log_levels)/sizeof(*c_log_levels);

ib_logger_level_t ib_logger_string_to_level(
    const char        *s,
    ib_logger_level_t  dlevel
)
{
    unsigned int i;
    ib_num_t     level;

    /* First, if it's a number, just do a numeric conversion */
    if (ib_string_to_num(s, 10, &level) == IB_OK) {
        return (ib_logger_level_t)level;
    }

    /* Now, string compare to level names */
    for (i = 0; i < c_num_levels; ++i) {
        if (
            strncasecmp(s, c_log_levels[i], strlen(c_log_levels[i])) == 0 &&
            strlen(s) == strlen(c_log_levels[i])
        ) {
            return i;
        }
    }

    /* No match, return the default */
    return dlevel;
}

const char *ib_logger_level_to_string(ib_logger_level_t level)
{
    if (level < c_num_levels) {
        return c_log_levels[level];
    }
    else {
        return "UNKNOWN";
    }
}
