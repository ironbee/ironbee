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
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/engine_types.h>
#include <ironbee/lock.h>
#include <ironbee/queue.h>

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeEngineLogging Logging
 * @ingroup IronBeeEngine
 * @{
 */

/**
 * Logger log level.
 **/
typedef enum {
    IB_LOG_EMERGENCY, /**< System unusable. */
    IB_LOG_ALERT,     /**< Crisis happened; immediate attention */
    IB_LOG_CRITICAL,  /**< Crisis coming; immediate attention */
    IB_LOG_ERROR,     /**< Error occurred; needs attention */
    IB_LOG_WARNING,   /**< Error likely to occur; needs attention */
    IB_LOG_NOTICE,    /**< Something unusual happened */
    IB_LOG_INFO,      /**< Something usual happened */
    IB_LOG_DEBUG,     /**< Developer oriented information */
    IB_LOG_DEBUG2,    /**< As above, lower priority */
    IB_LOG_DEBUG3,    /**< As above, lowest priority */
    IB_LOG_TRACE,     /**< Reserved for future use */

    /* Not a log level, but keeps track of the number of levels. */
    IB_LOG_LEVEL_NUM  /**< Number of levels */
} ib_logger_level_t;

/**
 * Different types of messages that flow through the logger.
 *
 * This type is used by formatters to know if they can or should
 * attempt to format an @ref ib_logger_rec_t.
 */
typedef enum {
    IB_LOGGER_ERRORLOG_TYPE, /**< Normal user error log. */
    IB_LOGGER_TXLOG_TYPE     /**< Transaction log. */
} ib_logger_logtype_t;

/**
 * String to level conversion.
 *
 * Attempts to convert @a s as both a number and a symbolic name (e.g. "debug")
 *
 * @param[in] s String to convert
 * @param[in] dlevel Default value in case conversion fails.
 *
 * @returns Converted log level (if successful), or @a default.
 */
ib_logger_level_t DLL_PUBLIC ib_logger_string_to_level(
    const char        *s,
    ib_logger_level_t  dlevel
);

typedef struct ib_logger_t ib_logger_t;
typedef struct ib_logger_rec_t ib_logger_rec_t;
typedef struct ib_logger_writer_t ib_logger_writer_t;

/**
 * Callback that returns part of a logging message.
 *
 * This function should not be confused with @ref ib_logger_format_fn_t
 * which is a function that formats log messages.
 *
 * @param[in] rec The log record used to produce @a msg and @a msg_sz.
 * @param[in] mp Memory pool to allocate out of. Released after logging call.
 * @param[out] msg Message generated.
 * @param[out] msg_sz Message size.
 * @param[in] data Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - Other values to signal an error.
 */
typedef ib_status_t (ib_logger_msg_fn_t)(
    const ib_logger_rec_t  *rec,
    ib_mpool_t             *mp,
    uint8_t               **msg,
    size_t                 *msg_sz,
    void                   *data
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
 * Signal a writer that its empty queue now has at least one element in it.
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
 * Ask the log writer to format the message before it is written.
 *
 * The @a log_msg should be escaped if the log writer cannot write
 * non-printable characters.
 *
 * The formatter also has the option to not handle the log message. If
 * IB_DECLINE is returned, then @a writer_record is not enqueued in the
 * writer's queue.
 *
 * @note If memory is allocated and IB_DECLINE is returned, there is no way
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
    ib_logger_t           *logger,
    const ib_logger_rec_t *rec,
    const uint8_t         *log_msg,
    const size_t           log_msg_sz,
    void                  *writer_record,
    void                  *data
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
    ib_logger_logtype_t type;        /**< Kind of message this is. */
    size_t              line_number; /**< Line num of the logging rec. */
    const char         *file;        /**< File of the logging statement. */
    const char         *function;    /**< The current function. */
    ib_time_t           timestamp;   /**< When the logging rec was made.*/
    const ib_module_t  *module;      /**< The current module. May be null. */
    const ib_conn_t    *conn;        /**< The current conn. May be null. */
    const ib_tx_t      *tx;          /**< The current tx. May be null. */
    const ib_engine_t  *engine;      /**< The IronBee engine. */
    ib_logger_level_t   level;       /**< The log level. */
};

/**
 * Submit a log message to a logger.
 *
 * This function takes @a msg and @a msg_fn as log message inputs.
 * The @a msg argument is a string from the user, whereas
 * @a msg_fn is function that will return a log message.
 *
 * When both are present, @a msg_fn's output is concatenated to @a msg and
 * passed on in the logging pipeline. If either is NULL, they are not included.
 *
 * If the resulting message is 0 length, then it is assumed that there is no
 * message and the message is not logged.
 *
 * @note This function may be used if the user would like to pass
 * binary data (a C struct) to the formatter as the user message. In this
 * use @a msg should be NULL and @a msg_sz should be 0. The resulting
 * data from @a msg_fn will then be forwarded, not copied, to the
 * formatting functions of all loggers.
 *
 * @param[in] logger The logger.
 * @param[in] type The type of log record being reported.
 * @param[in] file Optional current file (may be null).
 * @param[in] function Optional current function (may be null).
 * @param[in] line_number The current line number.
 * @param[in] engine The IronBee engine.
 * @param[in] module Optional module (may be null).
 * @param[in] conn Optional connection (may be null).
 * @param[in] tx Optional transaction (may be null).
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
    ib_logger_t         *logger,
    ib_logger_logtype_t  type,
    const char          *file,
    const char          *function,
    size_t               line_number,
    const ib_engine_t   *engine,
    const ib_module_t   *module,
    const ib_conn_t     *conn,
    const ib_tx_t       *tx,
    ib_logger_level_t    level,
    const uint8_t       *msg,
    size_t               msg_sz,
    ib_logger_msg_fn_t   msg_fn,
    void                *msg_fn_data
)
NONNULL_ATTRIBUTE(1, 6);

/**
 * Submit a log message using printf style arguments for the message.
 *
 * This function will compose the vargs into a single string
 * log message which will be passed down the logging pipeline.
 *
 * @param[in] logger The logger.
 * @param[in] type The type of log record being reported.
 * @param[in] file Optional current file (may be null).
 * @param[in] function Optional current function (may be null).
 * @param[in] line_number The current line number.
 * @param[in] engine The IronBee engine.
 * @param[in] module Optional module (may be null).
 * @param[in] conn Optional connection (may be null).
 * @param[in] tx Optional transaction (may be null).
 * @param[in] level The level the log record is at. If this value is
 *            below the log message level, this record is not logged.
 * @param[in] msg The user's format, followed by format arguments.
 */
void ib_logger_log_va(
    ib_logger_t         *logger,
    ib_logger_logtype_t  type,
    const char          *file,
    const char          *function,
    size_t               line_number,
    const ib_engine_t   *engine,
    const ib_module_t   *module,
    const ib_conn_t     *conn,
    const ib_tx_t       *tx,
    ib_logger_level_t    level,
    const char          *msg,
    ...
)
NONNULL_ATTRIBUTE(1, 6, 11)
PRINTF_ATTRIBUTE(11, 12);

/**
 * Submit a log message using vprintf style arguments for the message.
 *
 * This function will compose the vargs into a single string
 * log message which will be passed down the logging pipeline.
 *
 * @param[in] logger The logger.
 * @param[in] type The type of log record being reported.
 * @param[in] line_number The current line number.
 * @param[in] file Optional current file (may be null).
 * @param[in] function Optional current function (may be null).
 * @param[in] engine The IronBee engine.
 * @param[in] module Optional module (may be null).
 * @param[in] conn Optional connection (may be null).
 * @param[in] tx Optional transaction (may be null).
 * @param[in] level The level the log record is at. If this value is
 *            below the log message level, this record is not logged.
 * @param[in] msg The user's format, followed by format arguments.
 * @param[in] ap The list of arguments.
 */
void ib_logger_log_va_list(
    ib_logger_t         *logger,
    ib_logger_logtype_t  type,
    const char          *file,
    const char          *function,
    size_t               line_number,
    const ib_engine_t   *engine,
    const ib_module_t   *module,
    const ib_conn_t     *conn,
    const ib_tx_t       *tx,
    ib_logger_level_t    level,
    const char          *msg,
    va_list              ap
)
NONNULL_ATTRIBUTE(1, 6, 11)
VPRINTF_ATTRIBUTE(11);

/**
 * Create a new logger.
 *
 * @param[out] logger The logger.
 * @param[in] level The level the logger should allow to writers.
 * @param[in] mp Memory pool used to create resources used for the
 *            lifetime of this logger.
 *
 * @returns
 * - IB_OK success.
 */
ib_status_t ib_logger_create(
    ib_logger_t       **logger,
    ib_logger_level_t   level,
    ib_mpool_t      *mp
)
NONNULL_ATTRIBUTE(1, 3);

/**
 * Construct a log writer record in this logger using a set of callbacks.
 *
 * The writer API that the user is asked to supply allows for the user
 * to split logging across two threads, a formatting thread, and a writing
 * thread. The formatting thread block the ib_log_debug() (and similar) call
 * and the resulting message is put in a queue by the logging framework.
 *
 * The writer thread may, or may not, be a different thread from the
 * formatting thread. It is the user's decision. What this API provides
 * is @a record_fn to signal the user's implementation that the
 * record queue has gone from empty to non-empty.
 *
 * If the user empties the queue in the @a record_fn callback, this will
 * continue to block the formatting thread, thus blocking the client code
 * to the logger, until the writing is complete. A better implementation is
 * to use the @a record_fn to signal a sleeping thread that will do this work.
 *
 * @param[in] logger The logger to add the writer to.
 * @param[in] open_fn Signal the writer to open logging resources.
 * @param[in] open_data Callback data.
 * @param[in] close_fn Signal the writer to close logging resources.
 * @param[in] close_data Callback data.
 * @param[in] reopen_fn Signal the writer to reopen logging resources.
 * @param[in] reopen_data Callback data.
 * @param[in] format_fn Format a log message for the writer to write.
 *            Any allocations done by this function must be freed by the
 *            user's code when the logging message is written.
 *            Any arguments passed to this function should not
 *            be assumed to exist at log write time. A @ref ib_tx_t
 *            passed into this may be destroyed before the log
 *            records are able to be written, and so it should not be
 *            directly relied upon.
 * @param[in] format_data Callback data.
 * @param[in] record_fn Receive a signal that the queue of formatted
 *            log messages has gone from 0 to more-than-0.
 *            This function, like @a format_fn, fires
 *            in the logging thread. In order to not block the lo
 * @param[in] record_data Callback data.
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
 *            The default writer does not close the FILE provided. It is
 *            assumed to be managed externally.
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
 *
 * @returns
 * - IB_OK On success.
 * - Other on internal error.
 */
ib_status_t ib_logger_writer_clear(
    ib_logger_t *logger
);

/**
 * Open all logging resources. This is relayed to all writers.
 *
 * @param[in] logger The logger to commit messages in.
 *
 * @returns
 * - IB_OK On success.
 * - Other on implementation errors.
 */
ib_status_t ib_logger_open(
    ib_logger_t *logger
);

/**
 * Close all logging resources. This is relayed to all writers.
 *
 * @param[in] logger The logger to commit messages in.
 *
 * @returns
 * - IB_OK On success.
 * - Other on implementation errors.
 */
ib_status_t ib_logger_close(
    ib_logger_t *logger
);

/**
 * Reopen all logging resources. This is relayed to all writers.
 *
 * @param[in] logger The logger to commit messages in.
 *
 * @returns
 * - IB_OK On success.
 * - Other on implementation errors.
 */
ib_status_t ib_logger_reopen(
    ib_logger_t *logger
);

/**
 * Safely remove all messages from the queue and process them.
 *
 * This API must be called by a user to remove messages produced by
 * @ref ib_logger_format_fn_t, write them to a log store, and free them.
 *
 * @param[in] logger The logger.
 * @param[in] writer The logger writer.
 * @param[in] handler Callback function that handles the pointer to a
 *            log message produced by an implementation of
 *            @ref ib_logger_format_fn_t. This function drains the
 *            message queue and writes it to disk.
 * @param[in] cbdata Callback data for @a handler.
 *
 * @returns
 * - IB_OK On success.
 * - Other On failure.
 */
ib_status_t DLL_PUBLIC ib_logger_dequeue(
    ib_logger_t           *logger,
    ib_logger_writer_t    *writer,
    ib_queue_element_fn_t  handler,
    void                  *cbdata
);

/**
 * A standard logger log message format.
 *
 * This is the type of message produced by ib_logger_standard_formatter().
 * This structure should be freed with ib_logger_standard_msg_free().
 */
struct ib_logger_standard_msg_t {
    /**
     * A standard null terminated string that should precede
     * the user's message in the log file.
     */
    char    *prefix;

    /**
     * User's logging data. This is typically a string, but no
     * guarantee is made that it will not also include unprintable characters
     * or binary data. Users of the standard message should
     * escape this, if necessary, to log safely.
     */
    uint8_t *msg;

    /**
     * The length of @c msg.
     */
    size_t   msg_sz;
};

typedef struct ib_logger_standard_msg_t ib_logger_standard_msg_t;

/**
 * Free @a msg in a standard way.
 *
 * @param[out] msg Free this message structure.
 */
void DLL_PUBLIC ib_logger_standard_msg_free(ib_logger_standard_msg_t *msg);

/**
 * A standard implementation of @ref ib_logger_format_fn_t for IronBee.
 *
 * This format function is provided for implementers of writer callbacks
 * so they can easily produce a standard IronBee log entry for line-oriented
 * logs.
 *
 * @param[in] logger The logger.
 * @param[in] rec The logging record to use for formatting.
 *            This should be considered to be free'ed after this
 *            function call.
 * @param[in] log_msg The user's log message.
 * @param[in] log_msg_sz The user's log message size.
 * @param[out] writer_record On success an @a ib_logger_standard_msg_t is
 *             assigned here.
 * @param[in] data Unused.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On a memory error.
 */
ib_status_t DLL_PUBLIC ib_logger_standard_formatter(
    ib_logger_t           *logger,
    const ib_logger_rec_t *rec,
    const uint8_t         *log_msg,
    const size_t           log_msg_sz,
    void                  *writer_record,
    void                  *data
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
 * Get the current log level for this logger.
 *
 * This does not mean that other writer will not implement their own filtering.
 *
 * @param[in] logger The logger whose level will be returned.
 *
 * @returns The logger level.
 */
ib_logger_level_t DLL_PUBLIC ib_logger_level_get(ib_logger_t *logger);

/**
 * Set the current log level.
 *
 * @param[in] logger The logger whose level we are setting.
 * @param[in] level The level to set.
 */
void DLL_PUBLIC ib_logger_level_set(
    ib_logger_t *logger,
    ib_logger_level_t level
);

/**
 * Translate a log level to a string.
 *
 * @param[in] level Log level.
 * @returns String form of @a level.
 */
const char DLL_PUBLIC *ib_logger_level_to_string(ib_logger_level_t level);

/**
 * @} IronBeeEngineLogging
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_LOGGER_H_ */
