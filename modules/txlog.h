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
 * @brief IronBee Modules --- Transaction Logs Public API
 *
 * The TxLog module public api.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */
#ifndef __MODULES__TXLOG_H__
#define __MODULES__TXLOG_H__

#include <ironbee/field.h>
#include <ironbee/logger.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TXLOG_MODULE_NAME "TxLogModule"

/**
 * The public configuration data for the txlog module.
 */
struct ib_txlog_module_cfg_t {
    bool        is_enabled;   /**< Logging enabled for this context? */
    const char *log_basename; /**< Base name to log files as. */
    const char *log_basedir;  /**< Base directory to log files in. */
    ib_num_t    max_size;     /**< Maximum size of a log file, in bytes. */
    ib_num_t    max_age;      /**< Maximum age of a log file, in seconds. */

    /**
     * A function pointer managed to format TxLog output for the logger API.
     */
    ib_logger_format_fn_t logger_format_fn;
};
typedef struct ib_txlog_module_cfg_t ib_txlog_module_cfg_t;

/**
 * Fetch the configuration stored for the given context.
 *
 * @param[in] ib IronBee engine.
 * @param[in] ctx The context.
 * @param[out] cfg The user configuration.
 */
ib_status_t DLL_PUBLIC ib_txlog_get_config(
    const ib_engine_t            *ib,
    const ib_context_t           *ctx,
    const ib_txlog_module_cfg_t **cfg
) NONNULL_ATTRIBUTE(1, 2, 3);

#ifdef __cplusplus
} /* extern C */
#endif

#endif /* __MODULES__TXLOG_H__ */
