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

#ifndef _IB_LOGGER_H_
#define _IB_LOGGER_H_

/**
 * @file
 * @brief IronBee --- Logger
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/engine_types.h>
#include <ironbee/lock.h>
#include <ironbee/log.h>
#include <ironbee/queue.h>

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeEngineLogging New Logging
 * @ingroup IronBeeEngine
 * @{
 */

typedef struct ib_logger_t ib_logger_t;
typedef struct ib_logger_rec_t ib_logger_rec_t;
typedef struct ib_logger_writer_t ib_logger_writer_t;

/**
 * Callback that returns part of a logging message.
 *
 * This function should not log messages.
 *
 * @param[in] rec Record used to logging.
 * @param[in] mp Memory pool to allocate out of. Freed after logging call.
 * @param[in] msg Message generated.
 * @param[in] msg_sz Message size.
 * @param[in] data The data to use to produce the message.
 *
 * @returns The message to be logged or NULL if any error occures.
 */
typedef ib_status_t (ib_logger_msg_fn_t)(
    ib_logger_rec_t  *rec,
    ib_mpool_t       *mp,
    uint8_t         **msg,
    size_t           *msg_sz,
    void             *data
);

/**
 * Called to open the logger's resources and prepare it for logging.
 *
 * @param[in] logger Logging object.
 * @param[in] data Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error. Defined by the implementation.
 */
typedef ib_status_t (*ib_logger_open_fn)(ib_logger_t *logger, void *data);

/**
 * Called to close and release logging resources.
 *
 * @param[in] logger Logging object.
 * @param[in] data Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error. Defined by the implementation.
 */
typedef ib_status_t (*ib_logger_close_fn)(ib_logger_t *logger, void *data);

/**
 * Signal a writer that its empty queue now has at leaste one element in it.
 *
 * @param[in] logger The logger.
 * @param[in] writer The writer that the message is enqueued in.
 * @param[in] data Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error. Defined by the implementation.
 */
typedef ib_status_t (*ib_logger_record_fn_t)(
    ib_logger_t        *logger, 
    ib_logger_writer_t *writer,
    void               *data
);

/**
 * Ask the log writter to format the message before it is written.
 *
 * The @a log_msg should be escaped if the log writer cannot write 
 * non-printable characters.
 *
 * @param[in] logger The logger.
 * @param[in] rec The logging record to use for formatting.
 *            This should be considered to be free'ed after this 
 *            function call.
 * @param[in] log_msg The user's log message.
 * @param[in] log_msg_sz The user's log message size.
 * @param[out] writer_record Out variable. @c *writer_record is assigned to.
 * @param[in] data Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - IB_DECLINE If no formatting is done and the message should be skipped.
 * - Other on error. Defined by the implementation.
 */
typedef ib_status_t (*ib_logger_format_fn_t)(
    ib_logger_t     *logger,
    ib_logger_rec_t *rec,
    const uint8_t   *log_msg,
    const size_t     log_msg_sz,
    void            *writer_record,
    void            *data
);

/**
 * Reopen logging resources.
 *
 * This is suitable for when files on disk are rotated or changed
 * and need to be reopened.
 *
 * @param[in] logger Logging object.
 * @param[in] data Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error. Defined by the implementation.
 */
typedef ib_status_t (*ib_logger_reopen_fn)(ib_logger_t *logger, void *data);

/**
 * A log record.
 *
 * This is populated and passed to individual loggers which will
 * convert this into a logged message. This is a public structure, but 
 * should be considered read-only.
 */
struct ib_logger_rec_t {
    size_t             line_number; /* Line number of the logging statement. */
    const char        *file;        /* File of the logging statement. */
    const char        *function;    /* The current function. */
    ib_time_t          timestamp;   /* When the logging statement was made.*/
    ib_module_t       *module;      /* The current module. May be null. */
    ib_conn_t         *conn;        /* The current connection. May be null. */
    ib_tx_t           *tx;          /* The current transaction. May be null. */
    ib_engine_t       *engine;      /* The IronBee engine. */
    ib_log_level_t     level;       /* The log level. */
};

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
 * A logger is what @ref ib_logger_rec are submitted to to produce a log.
 */
struct ib_logger_t {
    ib_log_level_t       level;       /**< The log level. */

    /**
     * Memory pool with a lifetime of the logger.
     */
    ib_mpool_t          *mp;

    ib_list_t           *writers;    /**< List of @ref ib_logger_writter_t. */
};

/**
 * Submit a log message to a logger.
 *
 * This is an internal function that is typically wrapped by macros.
 *
 * @param[in] line_number The current line number.
 * @param[in] file The current file.
 * @param[in] function The current function.
 * @param[in] engine The IronBee engine.
 * @param[in] module Optional module.
 * @param[in] con Optional connection.
 * @param[in] tx Optional transaction.
 * @param[in] level The level the log record is at. If this value is
 *            below the log message level, this record is not logged.
 * @param[in] msg The optional first part of the user's log message.
 * @param[in] msg_fn An optional callback function that will return the last
 *            portion of the log message. This is only called
 *            if the message is actually formatted for logging.
 * @param[in] msg_fn_data Data passed to @a msg_fn.
*/
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
);

/**
 * Submit a log message using printf style arguments for the message.
 *
 * This function will compose the vargs into a single string
 * log message which will be passed down the logging pipeline.
 *
 * @param[in] line_number The current line number.
 * @param[in] file The current file.
 * @param[in] function The current function.
 * @param[in] engine The IronBee engine.
 * @param[in] module Optional module.
 * @param[in] con Optional connection.
 * @param[in] tx Optional transaction.
 * @param[in] level The level the log record is at. If this value is
 *            below the log message level, this record is not logged.
 * @param[in] msg The optional first part of the user's log message.
 * @param[in] msg_fn An optional callback function that will return the last
 *            portion of the log message. This is only called
 *            if the message is actually formatted for logging.
 * @param[in] msg_fn_data Data passed to @a msg_fn.
 */
void ib_logger_log_va(
    ib_logger_t       *logger,
    size_t             line_number,
    const char        *file,
    const char        *function,
    ib_engine_t       *engine,
    ib_module_t       *module,
    ib_conn_t         *con,
    ib_tx_t           *tx,
    ib_log_level_t     level,
    const char        *msg,
    ...
)
PRINTF_ATTRIBUTE(10, 11);

/**
 * Create a new logger.
 *
 * @param[in] logger
 * @param[in] level
 * @param[in] mp Memory pool used to create resources used for the 
 *            lifetime of this logger.
 *
 * @returns
 * - IB_OK success.
 */
ib_status_t ib_logger_create(
    ib_logger_t    **logger,
    ib_log_level_t   level,
    ib_mpool_t      *mp
);

/**
 * @param[in] logger The logger to add the writer to.
 * @param[in] open_fn
 * @param[in] open_data
 * @param[in] close_fn
 * @param[in] close_data
 * @param[in] reopen_fn
 * @param[in] reopen_data
 * @param[in] format_fn
 * @param[in] format_data
 * @param[in] record_fn
 * @param[in] record_data
 *
 * @returns
 * - IB_OK success.
 */
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
);

/**
 * Add the default writer.
 *
 * @param[in] logger The logger.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On memory allocation error.
 * - Other on unexpected failure.
 */
ib_status_t ib_logger_writer_add_default(
    ib_logger_t *logger
);

/**
 * Clear all logger writers.
 *
 * @param[in] logger The logger.
 */
ib_status_t ib_logger_writer_clear(
    ib_logger_t *logger
);

/**
 * @param[in] logger The logger to commit messages in.
 *
 * @returns
 * - IB_OK On succes.
 * - Other on implementation errors.
 */
ib_status_t ib_logger_open(
    ib_logger_t *logger
);

/**
 * @param[in] logger The logger to commit messages in.
 *
 * @returns
 * - IB_OK On succes.
 * - Other on implementation errors.
 */
ib_status_t ib_logger_close(
    ib_logger_t *logger
);

/**
 * @param[in] logger The logger to commit messages in.
 *
 * @returns
 * - IB_OK On succes.
 * - Other on implementation errors.
 */
ib_status_t ib_logger_reopen(
    ib_logger_t *logger
);

/**
 * Safely remove 1 message from the queue.
 *
 * @param[in]  logger The logger.
 * @param[in]  writer The logger writer.
 * @param[out] msg    A pointer to a pointer into which the 
 *             message in the queue, produced by the writer's
 *             format function, is stored. This will
 *             assign @c *msg the dequeued message.
 *
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If the queue is empty.
 * - Other on failure.
 */
ib_status_t DLL_PUBLIC ib_logger_dequeue(
    ib_logger_t        *logger,
    ib_logger_writer_t *writer,
    void               *msg
);

/**
 * @} IronBeeEngineLogging
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_LOGGER_H_ */
