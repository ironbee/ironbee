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

#ifndef _IB_ENGINE_MANAGER_TESTAPI_H_
#define _IB_ENGINE_MANAGER_TESTAPI_H_

/**
 * @file
 * @brief IronBee --- Engine Manager Test API
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
 * @defgroup IronBeeEngineManagerTestAPI IronBee Engine Manager Test API
 * @ingroup IronBeeEngineManager
 *
 * These functions are used for testing the engine manager implementation
 * and its use in servers.
 *
 * @{
 */

/**
 * Engine manager engine destroy operations.
 */
typedef enum {
    IB_MANAGER_DESTROY_INACTIVE,    /**< Destroy only inactive engines */
    IB_MANAGER_DESTROY_ALL,         /**< Destroy all engines */
} ib_manager_destroy_ops;

/**
 * Disables the engine manager's current engine.
 *
 * This will cause ib_manager_engine_acquire() to return IB_DECLINED.
 * Used primarily to allow testing of the engine manager for memory leaks.
 *
 * @param[in] manager IronBee engine manager
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_manager_disable_current(
    ib_manager_t *manager
);

/**
 * Destroy zero or more IronBee engines managed by an engine manager.
 *
 * If @a op is IB_MANAGER_DESTROY_INACTIVE, only inactive, non-current engines
 * will be destroyed.  If @a op is IB_MANAGER_DESTROY_ALL, all engines will be
 * destroyed.
 *
 * @param[in] manager IronBee engine manager
 * @param[in] op Destroy operation (INACTIVE,ALL)
 * @param[out] pcount Pointer to engine count after destroy (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_manager_destroy_engines(
    ib_manager_t           *manager,
    ib_manager_destroy_ops  op,
    size_t                 *pcount
);

/**
 * Override the engine manager's logger.
 *
 * See ib_manager_create() for documentation on the callback function
 * arguments.
 *
 * @param[in] manager IronBee manager object
 * @param[in] logger_buf_fn Logger function (Formatted buffer version)
 * @param[in] logger_buf_cbdata Callback data.
 * @param[in] logger_flush_fn Logger flush function (or NULL)
 * @param[in] logger_flush_cbdata Callback data.
 */
void DLL_PUBLIC ib_manager_set_logger(
    ib_manager_t              *manager,
    ib_manager_log_buf_fn_t    logger_buf_fn,
    void                      *logger_buf_cbdata,
    ib_manager_log_flush_fn_t  logger_flush_fn,
    void                      *logger_flush_cbdata
);

/**
 * File logger for the engine manager (@c va_list version).
 *
 * This function implements a logger that will log to the FILE pointer
 * (specified as logger_cbdata to ib_manager_set_vlogger()).  This is a
 * convenience function, and is intended to be used during shutdown and
 * testing.
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
 * @param[in] level IronBee log level
 * @param[in] cbdata Callback data
 * @param[in] fmt Format string
 * @param[in] ap Var args list to match the format
 */
void DLL_PUBLIC ib_manager_file_vlogger(
    ib_logger_level_t  level,
    void              *cbdata,
    const char        *fmt,
    va_list            ap
);

/**
 * File logger for the engine manager.
 *
 * This function implements a logger that will log to the FILE pointer
 * (specified as logger_cbdata to ib_manager_set_logger()).  This is a
 * convenience function, and is intended to be used during shutdown and
 * testing.
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
 * @param[in] level IronBee log level
 * @param[in] cbdata Callback data
 * @param[in] buf Formatted buffer
 */
void DLL_PUBLIC ib_manager_file_logger(
    ib_logger_level_t  level,
    void              *cbdata,
    const char        *buf
);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_MANAGER_TESTAPI_H_ */
