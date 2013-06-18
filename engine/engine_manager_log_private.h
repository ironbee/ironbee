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

#ifndef _IB_ENGINE_MANAGER_LOG_PRIVATE_H_
#define _IB_ENGINE_MANAGER_LOG_PRIVATE_H_

/**
 * @file
 * @brief IronBee --- Engine Manager logging private
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */
#include <ironbee/engine_manager.h>

#include "engine_manager_private.h"

#include <ironbee/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
* @defgroup IronBeeEngineManagerLogPrivate Engine Manager logging
* private types and API
*
* @ingroup IronBeeEngineManagerPrivate
*
* @{
*/

/**
 * IronBee Engine Manager logger.
 *
 * Performs IronBee logging for the engine manager
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] level Debug level
 * @param[in] file File name
 * @param[in] line Line number
 * @param[in] fmt Format string
 * @param[in] ap Var args list to match the format
 * @param[in] cbdata Callback data (engine manager handle)
 */
void DLL_LOCAL ib_engine_manager_logger(
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
 * Internal logger for the engine manager (ex version)
 *
 * @param[in] manager IronBee engine manager
 * @param[in] level Log level.
 * @param[in] file Filename.
 * @param[in] line Line number.
 * @param[in] fmt Printf-like format string
 */
void DLL_LOCAL ib_manager_log_ex(
    const ib_manager_t *manager,
    ib_log_level_t      level,
    const char         *file,
    int                 line,
    const char         *fmt,
    ...
)
PRINTF_ATTRIBUTE(5, 6);

/**
 * Internal logger for the engine manager
 *
 * @param[in] manager IronBee engine manager
 * @param[in] level Log level.
 */
#define ib_manager_log(manager, level, ...)                               \
    ib_manager_log_ex((manager), (level), __FILE__, __LINE__, __VA_ARGS__)


/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_MANAGER_LOG_PRIVATE_H_ */
