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
 */

#include <ironbee/engine_types.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeEngineLog Logging
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
} ib_log_level_t;

/**
 * String to level conversion.
 */
ib_log_level_t DLL_PUBLIC ib_log_string_to_level(const char* s);

/**
 * Level to string conversion
 */
const char DLL_PUBLIC *ib_log_level_to_string(ib_log_level_t level);

/**
 * Logger callback.
 *
 * @param ib IronBee engine.
 * @param level Log level
 * @param file Optional source filename (or NULL)
 * @param line Optional source line number (or 0)
 * @param fmt Formatting string
 * @param ap Variable args list
 * @param cbdata Callback data
 */
typedef void (*ib_log_logger_fn_t)(
    const ib_engine_t *ib,
    ib_log_level_t     level,
    const char        *file,
    int                line,
    const char        *fmt,
    va_list            ap,
    void              *cbdata
)
VPRINTF_ATTRIBUTE(5);

/**
 * Log level callback.
 *
 * @param ib IronBee engine
 * @param cbdata Callback data.
 * @return Log level.
 */
typedef ib_log_level_t (*ib_log_level_fn_t)(
    const ib_engine_t *ib,
    void              *cbdata
);

/**
 * Set logger callback.
 *
 * @param ib     IronBee engine.
 * @param logger Logger function.
 * @param cbdata Data to pass to logger function.
 */
void DLL_PUBLIC ib_log_set_logger_fn(
    ib_engine_t        *ib,
    ib_log_logger_fn_t  logger,
    void               *cbdata
);

/**
 * Set log level callback.
 *
 * @param ib        IronBee engine.
 * @param log_level Log level function.
 * @param cbdata    Data to pass to log level function.
 */
void DLL_PUBLIC ib_log_set_loglevel_fn(
    ib_engine_t       *ib,
    ib_log_level_fn_t  log_level,
    void              *cbdata
);

/** Log Generic */
#define ib_log(ib, lvl, ...) ib_log_ex((ib), (lvl), __FILE__, __LINE__, __VA_ARGS__)
/** Log Emergency */
#define ib_log_emergency(ib, ...) ib_log((ib), IB_LOG_EMERGENCY, __VA_ARGS__)
/** Log Alert */
#define ib_log_alert(ib, ...)     ib_log((ib), IB_LOG_ALERT, __VA_ARGS__)
/** Log Critical */
#define ib_log_critical(ib, ...)  ib_log((ib), IB_LOG_CRITICAL, __VA_ARGS__)
/** Log Error */
#define ib_log_error(ib, ...)     ib_log((ib), IB_LOG_ERROR, __VA_ARGS__)
/** Log Warning */
#define ib_log_warning(ib, ...)   ib_log((ib), IB_LOG_WARNING, __VA_ARGS__)
/** Log Notice */
#define ib_log_notice(ib, ...)    ib_log((ib), IB_LOG_NOTICE, __VA_ARGS__)
/** Log Info */
#define ib_log_info(ib, ...)      ib_log((ib), IB_LOG_INFO, __VA_ARGS__)
/** Log Debug */
#define ib_log_debug(ib, ...)     ib_log((ib), IB_LOG_DEBUG, __VA_ARGS__)
/** Log Debug2 */
#define ib_log_debug2(ib, ...)    ib_log((ib), IB_LOG_DEBUG2, __VA_ARGS__)
/** Log Debug3 */
#define ib_log_debug3(ib, ...)    ib_log((ib), IB_LOG_DEBUG3, __VA_ARGS__)
/** Log Trace */
#define ib_log_trace(ib, ...)     ib_log((ib), IB_LOG_TRACE, __VA_ARGS__)

/** Log Generic (Transaction form) */
#define ib_log_tx(tx, lvl, ...) ib_log_tx_ex(tx,  (lvl), __FILE__, __LINE__, __VA_ARGS__)
/** Log Emergency (Transaction form) */
#define ib_log_emergency_tx(tx, ...) ib_log_tx(tx,  IB_LOG_EMERGENCY, __VA_ARGS__)
/** Log Alert (Transaction form) */
#define ib_log_alert_tx(tx, ...)     ib_log_tx(tx,  IB_LOG_ALERT, __VA_ARGS__)
/** Log Critical (Transaction form) */
#define ib_log_critical_tx(tx, ...)  ib_log_tx(tx,  IB_LOG_CRITICAL, __VA_ARGS__)
/** Log Error (Transaction form) */
#define ib_log_error_tx(tx, ...)     ib_log_tx(tx,  IB_LOG_ERROR, __VA_ARGS__)
/** Log Warning (Transaction form) */
#define ib_log_warning_tx(tx, ...)   ib_log_tx(tx,  IB_LOG_WARNING, __VA_ARGS__)
/** Log Notice (Transaction form) */
#define ib_log_notice_tx(tx, ...)    ib_log_tx(tx,  IB_LOG_NOTICE, __VA_ARGS__)
/** Log Info (Transaction form) */
#define ib_log_info_tx(tx, ...)      ib_log_tx(tx,  IB_LOG_INFO, __VA_ARGS__)
/** Log Debug (Transaction form) */
#define ib_log_debug_tx(tx, ...)     ib_log_tx(tx,  IB_LOG_DEBUG, __VA_ARGS__)
/** Log Debug2 (Transaction form) */
#define ib_log_debug2_tx(tx, ...)    ib_log_tx(tx,  IB_LOG_DEBUG2, __VA_ARGS__)
/** Log Debug3 (Transaction form) */
#define ib_log_debug3_tx(tx, ...)    ib_log_tx(tx,  IB_LOG_DEBUG3, __VA_ARGS__)
/** Log Trace (Transaction form) */
#define ib_log_trace_tx(tx, ...)     ib_log_tx(tx,  IB_LOG_TRACE, __VA_ARGS__)

/**
 * Generic Logger for engine.
 *
 * @param ib IronBee engine
 * @param level Log level.
 * @param file Filename.
 * @param line Line number.
 * @param fmt Printf-like format string
 */
void DLL_PUBLIC ib_log_ex(
    const ib_engine_t *ib,
    ib_log_level_t     level,
    const char        *file,
    int                line,
    const char        *fmt,
    ...
)
PRINTF_ATTRIBUTE(5, 6);

/**
 * Transaction Logger for engine.
 *
 * @param tx Transaction
 * @param level Log level.
 * @param file Filename.
 * @param line Line number.
 * @param fmt Printf-like format string
 */
void DLL_PUBLIC ib_log_tx_ex(
     const ib_tx_t  *tx,
     ib_log_level_t  level,
     const char     *file,
     int             line,
     const char     *fmt,
     ...
)
PRINTF_ATTRIBUTE(5, 6);

/**
 * Generic Logger for engine.  valist version.
 *
 * @param ib IronBee engine.
 * @param level Log level.
 * @param file Filename.
 * @param line Line number.
 * @param fmt Printf-like format string.
 * @param ap Argument list.
 */
void DLL_PUBLIC ib_log_vex_ex(
    const ib_engine_t *ib,
    ib_log_level_t     level,
    const char        *file,
    int                line,
    const char        *fmt,
    va_list            ap
)
VPRINTF_ATTRIBUTE(5);

/**
 * Transaction Logger for engine.  valist version.
 *
 * @param tx Transaction.
 * @param level Log level.
 * @param file Filename.
 * @param line Line number.
 * @param fmt Printf-like format string.
 * @param ap Argument list.
 */
void DLL_PUBLIC ib_log_tx_vex(
    const ib_tx_t*     tx,
    ib_log_level_t     level,
    const char        *file,
    int                line,
    const char        *fmt,
    va_list            ap
)
VPRINTF_ATTRIBUTE(5);

/**
 * Return the configured logging level.
 *
 * This is used to determine if optional complex processing should be
 * performed to log possibly option information.
 *
 * @param[in] ib The IronBee engine that would be used in a call to ib_log_ex.
 * @return The log level configured.
 */
ib_log_level_t DLL_PUBLIC ib_log_get_level(const ib_engine_t *ib);

/**
 * Translate a log level to a string.
 *
 * @param[in] level Log level.
 * @returns String form of @a level.
 */
const char DLL_PUBLIC *ib_log_level_to_string(ib_log_level_t level);

/**
 * @} IronBeeEngineLog
 */

#ifdef __cplusplus
}
#endif

#endif
