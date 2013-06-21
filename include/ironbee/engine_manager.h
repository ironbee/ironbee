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

#ifndef _IB_ENGINE_MANAGER_H_
#define _IB_ENGINE_MANAGER_H_

/**
 * @file
 * @brief IronBee --- Engine Manager
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/engine.h>
#include <ironbee/log.h>
#include <ironbee/types.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeEngineManager Engine Manager
 * @ingroup IronBee
 *
 * The engine manager provides services to manage multiple IronBee engines.
 *
 * In the current implementation, all of these engines run in the same process
 * space.
 *
 * @{
 */

/* Engine Manager type declarations */

/**
 * The engine manager.
 *
 * An engine manager is created via ib_manager_create().
 *
 * Servers which use the engine manager will typically create a single engine
 * manager at startup, and then use the engine manager to create engines when
 * the configuration has changed via ib_manager_engine_create().
 *
 * The engine manager will then manage the IronBee engines, with the most
 * recent one successfully created being the "current" engine.  An engine
 * managed by the manager is considered active if it is current or its
 * reference count is non-zero.
 *
 * ib_manager_engine_acquire() is used to acquire the current engine.  A
 * matching call to ib_manager_engine_release() is required to release it.  If
 * the released engine becomes inactive (e.g., the engine is not current and
 * its reference count becomes zero), the manager will destroy all inactive
 * engines.
 *
 */
typedef struct ib_manager_t ib_manager_t;

/**
 * Engine manager logger callback (@c va_list version).
 *
 * @param[in] fmt Format string
 * @param[in] ap Variable args list
 * @param[in] cbdata Callback data
 */
typedef void (*ib_vlogger_fn_t)(
    const char         *fmt,
    va_list             ap,
    void               *cbdata
)
VPRINTF_ATTRIBUTE(1);

/**
 * Engine manager logger callback (formatted buffer version).
 *
 * @param[in] buf Formatted buffer
 * @param[in] cbdata Callback data
 */
typedef void (*ib_logger_fn_t)(
    const char         *buf,
    void               *cbdata
);

/* Engine Manager API */

/**
 * Create an engine manager.
 *
 * @param[in] server IronBee server object
 * @param[in] max_engines Maximum number of simultaneous engines (0 for default)
 * @param[in] vlogger_fn Logger function (@c va_list version)
 * @param[in] logger_fn Logger function (Formatted buffer version)
 * @param[in] logger_cbdata Data to pass to logger function
 * @param[in] logger_level Initial log level
 * @param[out] pmanager Pointer to IronBee engine manager object
 *
 * If @a vlogger_fn is provided, the engine manager's IronBee logger will not
 * format the log message, but will instead pass the format (@a fmt) and args
 * (@a ap) directly to the logger function.
 *
 * If @a logger_fn is provided, the engine manager will do all of
 * the formatting, and will then call the logger with a formatted buffer.
 *
 * One of @a logger_va or @a logger_buf logger functions should be specified,
 * but not both.  Behavior is undefined if both are NULL or both are non-NULL.
 *
 * If the server provides a @c va_list logging facility, the @a vlogger_fn
 * should be specified.  The alternate @c formatted buffer logger function
 * @a logger_fn is provided for servers that don't provide a @c va_list logging
 * facility (e.g., Traffic Server).
 *
 * @returns Status code
 * - IB_OK if all OK
 * - IB_EALLOC for allocation problems
 */
ib_status_t DLL_PUBLIC ib_manager_create(
    const ib_server_t  *server,
    size_t              max_engines,
    ib_vlogger_fn_t     vlogger_fn,
    ib_logger_fn_t      logger_fn,
    void               *logger_cbdata,
    ib_log_level_t      logger_level,
    ib_manager_t      **pmanager
);

/**
 * Destroy an engine manager.
 *
 * Destroys IronBee engines managed by @a manager, and the engine manager
 * itself if all managed engines are destroyed.
 *
 * @note If server threads are still interacting with any of the IronBee
 * engines that are managed by @a manager, destroying @a manager can be
 * dangerous and can lead to undefined behavior.
 *
 * @param[in] manager IronBee engine manager
 *
 * @returns Status code
 * - IB_OK All OK.  All IronBee engines and the engine manager are destroyed.
 */
ib_status_t DLL_PUBLIC ib_manager_destroy(
    ib_manager_t *manager
);

/**
 * Create a new IronBee engine.
 *
 * The engine manager will destroy old engines when a call to
 * ib_manager_engine_release() results in a non-current engine having a
 * reference count of zero.
 *
 * If the engine creation is successful, this new engine becomes the current
 * engine.  This new engine is created with a zero reference count, but will
 * not be destroyed by ib_manager_engine_cleanup() operations as long as it
 * remains the current engine.
 *
 * @param[in] manager IronBee engine manager
 * @param[in] config_file Configuration file path
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_manager_engine_create(
    ib_manager_t  *manager,
    const char    *config_file
);

/**
 * Acquire the current IronBee engine.
 *
 * This function increments the reference count associated with the current
 * engine, and then returns that engine.
 *
 * @note A matching call to ib_manager_engine_release() is required to
 * decrement the reference count.
 *
 * @param[in] manager IronBee engine manager
 * @param[out] pengine Pointer to the current engine
 *
 * @returns Status code
 *   - IB_OK All OK
 *   - IB_ENOENT No current IronBee engine exists
 */
ib_status_t DLL_PUBLIC ib_manager_engine_acquire(
    ib_manager_t  *manager,
    ib_engine_t  **pengine
);

/**
 * Release the specified IronBee engine when no longer required.
 *
 * This function decrements the reference count associated with the specified
 * engine.
 *
 * Behavior is undefined if @a engine is not known to the engine manager or
 * if the reference count of @a engine is zero.
 *
 * @note A matching prior call to ib_manager_engine_acquire() is required to
 * increment the reference count.
 *
 * @param[in] manager IronBee engine manager
 * @param[in] engine IronBee engine to release
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_manager_engine_release(
    ib_manager_t *manager,
    ib_engine_t  *engine
);

/**
 * Cleanup and destroy any inactive engines.
 *
 * This will destroy any non-current engines with a zero reference count.
 *
 * @param[in] manager IronBee engine manager
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_manager_engine_cleanup(
    ib_manager_t *manager
);

/**
 * Get the count of IronBee engines.
 *
 * @param[in] manager IronBee engine manager
 *
 * @returns Count of total IronBee engines
 */
size_t DLL_PUBLIC ib_manager_engine_count(
    ib_manager_t *manager
);

/**
 * @defgroup IronBeeEngineManagerLogging Engine Manager logging
 * @ingroup IronBeeEngineManager
 *
 * These functions can be used by servers to change the logger at run-time.
 *
 * @{
 */

/**
 * Override the engine manager's vlogger.
 *
 * @param[in] manager IronBee manager object
 * @param[in] vlogger_fn Logger function (@c va_list version)
 * @param[in] logger_cbdata Data to pass to logger function
 */
void DLL_PUBLIC ib_manager_set_vlogger(
    ib_manager_t       *manager,
    ib_vlogger_fn_t     vlogger_fn,
    void               *logger_cbdata
);

/**
 * Override the engine manager's logger.
 *
 * @param[in] manager IronBee manager object
 * @param[in] logger_fn Logger function (Formatted buffer version)
 * @param[in] logger_cbdata Data to pass to logger function
 */
void DLL_PUBLIC ib_manager_set_logger(
    ib_manager_t       *manager,
    ib_logger_fn_t      logger_fn,
    void               *logger_cbdata
);

/**
 * File logger for the engine manager (@c va_list version).
 *
 * This function implements a logger that will log to the @c FILE pointer
 * (specified as @c logger_cbdata to ib_manager_set_vlogger()).  This is a
 * convenience function, and is intended to be used if the server doesn't
 * provide a logger or the logger becomes unavailable (e.g., during shutdown).
 *
 * Example usages:
 * @code
 * ib_manager_set_vlogger(manager, ib_manager_file_vlogger, stderr);
 * @endcode
 * @code
 * FILE *fp = open( ... );
 * ib_manager_set_vlogger(manager, ib_manager_file_vlogger, fp);
 * @endcode
 *
 * @param[in] fmt Format string
 * @param[in] ap Var args list to match the format
 * @param[in] cbdata Callback data
 */
void DLL_PUBLIC ib_manager_file_vlogger(
    const char        *fmt,
    va_list            ap,
    void              *cbdata
);

/**
 * File logger for the engine manager.
 *
 * This function implements a logger that will log to the @c FILE pointer
 * (specified as @c logger_cbdata to ib_manager_set_logger()).  This is a
 * convenience function, and is intended to be used if the server doesn't
 * provide a logger or the logger becomes unavailable (e.g., during shutdown).
 *
 * Example usages:
 * @code
 * ib_manager_set_logger(manager, ib_manager_file_logger, stderr);
 * @endcode
 * @code
 * FILE *fp = open( ... );
 * ib_manager_set_logger(manager, ib_manager_file_logger, fp);
 * @endcode
 *
 * @param[in] buf Formatted buffer
 * @param[in] cbdata Callback data
 */
void DLL_PUBLIC ib_manager_file_logger(
    const char        *buf,
    void              *cbdata
);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_MANAGER_H_ */
