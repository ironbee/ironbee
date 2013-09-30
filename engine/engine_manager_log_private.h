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

#include "engine_manager_private.h"

#include <ironbee/engine_manager.h>
#include <ironbee/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeEngineManagerLogPrivate IronBee Engine Manager Logging
 * private types and API
 *
 * @ingroup IronBeeEngineManagerPrivate
 *
 * @{
 */

ib_status_t DLL_LOCAL manager_logger_open(ib_logger_t *logger, void *data);
ib_status_t DLL_LOCAL manager_logger_close(ib_logger_t *logger, void *data);
ib_status_t DLL_LOCAL manager_logger_reopen(ib_logger_t *logger, void *data);
ib_status_t DLL_LOCAL manager_logger_format(
    ib_logger_t           *logger,
    const ib_logger_rec_t *rec,
    const uint8_t         *msg,
    const size_t           log_msg_sz,
    void                  *writer_record,
    void                  *data
);
ib_status_t DLL_LOCAL manager_logger_record(
    ib_logger_t        *logger,
    ib_logger_writer_t *writer,
    void *data
);

/**
 * Internal logger for the engine manager (ex version).
 *
 * @param[in] manager IronBee engine manager
 * @param[in] level Log level.
 * @param[in] file Filename.
 * @param[in] func Function name.
 * @param[in] line Line number.
 * @param[in] calldata Context-specific data
 * @param[in] fmt Printf-like format string
 */
void DLL_LOCAL ib_manager_log_ex(
    ib_manager_t       *manager,
    ib_log_level_t      level,
    const char         *file,
    const char         *func,
    int                 line,
    ib_log_call_data_t *calldata,
    const char         *fmt,
    ...
)
PRINTF_ATTRIBUTE(7, 8);

/**
 * Internal logger for the engine manager.
 *
 * @param[in] manager IronBee engine manager
 * @param[in] level Log level.
 */
#define ib_manager_log(manager, level, ...)                               \
    ib_manager_log_ex((manager), (level), __FILE__, __func__, __LINE__, &(ib_log_call_data_t) {IBLOG_MANAGER, {.m=(manager)}}, __VA_ARGS__)

/**
 * Log flush request to internal logger for the engine master.
 *
 * @param[in] manager IronBee engine manager
 */
void DLL_LOCAL ib_manager_log_flush(
    const ib_manager_t *manager
                                    );


/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_MANAGER_LOG_PRIVATE_H_ */
