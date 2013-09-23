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
 * The formatter also has the option to not handle the log message. If
 * IB_DECLINE is returned then @a writer_record is not enqueued in the
 * writer's queue.
 *
 * Note: If memory is allocated and IB_DECLINE is returned, there is no way
 * to get that memory back to free it through this API.
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
    const ib_module_t *module;      /* The current module. May be null. */
    const ib_conn_t   *conn;        /* The current connection. May be null. */
    const ib_tx_t     *tx;          /* The current transaction. May be null. */
    const ib_engine_t *engine;      /* The IronBee engine. */
    ib_log_level_t     level;       /* The log level. */
};

/**
 * Submit a log message to a logger.
 *
 * This function takes @a msg and @a msg_fn as log message inputs.
 * The @a msg argument is a simple string from the user where as
 * @a msg_fn is function that will return a log message.
 *
 * When both are present @a msg_fn's output is contactiated to @a msg and
 * passed on in the logging pipeline. If either is NULL, they are not included.
 *
 * If the resulting message is 0 length, then it is assumed that there is no
 * message and the message is not logged.
 *
 * @param[in] logger The logger.
 * @param[in] line_number The current line number.
 * @param[in] file The current file.
 * @param[in] function The current function.
 * @param[in] engine The IronBee engine.
 * @param[in] module Optional module.
 * @param[in] conn Optional connection.
 * @param[in] tx Optional transaction.
 * @param[in] level The level the log record is at. If this value is
 *            below the log message level, this record is not logged.
 * @param[in] msg The optional first part of the user's log message.
 * @param[in] msg_sz The message size.
 * @param[in] msg_fn An optional callback function that will return the last
 *            portion of the log message. This is only called
 *            if the message is actually formatted for logging.
 * @param[in] msg_fn_data Data passed to @a msg_fn.
*/
void ib_logger_log_msg(
    ib_logger_t       *logger,
    const char        *file,
    const char        *function,
    size_t             line_number,
    const ib_engine_t *engine,
    const ib_module_t *module,
    const ib_conn_t   *conn,
    const ib_tx_t     *tx,
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
 * @param[in] logger The logger.
 * @param[in] line_number The current line number.
 * @param[in] file The current file.
 * @param[in] function The current function.
 * @param[in] engine The IronBee engine.
 * @param[in] module Optional module.
 * @param[in] conn Optional connection.
 * @param[in] tx Optional transaction.
 * @param[in] level The level the log record is at. If this value is
 *            below the log message level, this record is not logged.
 * @param[in] msg The user's format, followed by format arguments.
 */
void ib_logger_log_va(
    ib_logger_t       *logger,
    const char        *file,
    const char        *function,
    size_t             line_number,
    const ib_engine_t *engine,
    const ib_module_t *module,
    const ib_conn_t   *conn,
    const ib_tx_t     *tx,
    ib_log_level_t     level,
    const char        *msg,
    ...
)
PRINTF_ATTRIBUTE(10, 11);

/**
 * Submit a log message using vprintf style arguments for the message.
 *
 * This function will compose the vargs into a single string
 * log message which will be passed down the logging pipeline.
 *
 * @param[in] logger The logger.
 * @param[in] line_number The current line number.
 * @param[in] file The current file.
 * @param[in] function The current function.
 * @param[in] engine The IronBee engine.
 * @param[in] module Optional module.
 * @param[in] conn Optional connection.
 * @param[in] tx Optional transaction.
 * @param[in] level The level the log record is at. If this value is
 *            below the log message level, this record is not logged.
 * @param[in] msg The user's format, followed by format arguments.
 * @param[in] ap The list of arguments.
 */
void ib_logger_log_va_list(
    ib_logger_t       *logger,
    const char        *file,
    const char        *function,
    size_t             line_number,
    const ib_engine_t *engine,
    const ib_module_t *module,
    const ib_conn_t   *conn,
    const ib_tx_t     *tx,
    ib_log_level_t     level,
    const char        *msg,
    va_list            ap
)
VPRINTF_ATTRIBUTE(10);

/**
 * Create a new logger.
 *
 * @param[in] logger The logger.
 * @param[in] level The level the logger should allow to writers.
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
 * @param[in] logfile The log file to write to. May be stderr or similar.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On memory allocation error.
 * - Other on unexpected failure.
 */
ib_status_t ib_logger_writer_add_default(
    ib_logger_t *logger,
    FILE        *logfile
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
 * Get the count of writers in this logger.
 *
 * @param[in] logger The logger to get the count from.
 *
 * @returns The count of the writers in this logger.
 */
size_t ib_logger_writer_count(ib_logger_t *logger);

/**
 * Return the current log level for this logger.
 *
 * This does not mean that other writer will not implement their own filtering.
 *
 * @param[in] logger The logger whose level will be returned.
 *
 * @returns The logger level.
 */
ib_log_level_t DLL_PUBLIC ib_logger_level_get(ib_logger_t *logger);

/**
 * Set the current log level.
 *
 * @param[in] logger The logger whose level we are setting.
 * @param[in] level The level to set.
 */
void DLL_PUBLIC ib_logger_level_set(ib_logger_t *logger, ib_log_level_t level);

/**
 * @} IronBeeEngineLogging
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_LOGGER_H_ */
