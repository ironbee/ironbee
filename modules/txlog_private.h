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
 * @brief IronBee Modules --- Transaction Logs Private Header File.
 *
 * The TxLog private header file.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */
#ifndef __MODULES__TXLOG_PRIVATE_H__
#define __MODULES__TXLOG_PRIVATE_H__

#include "txlog.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Context configuration value for the TxLogModule.
 */
struct txlog_config_t {
    ib_txlog_module_cfg_t pub_cfg; /**< Public configuration information. */
};
typedef struct txlog_config_t txlog_config_t;

#ifdef __cplusplus
}
#endif

#endif /* __MODULES__TXLOG_PRIVATE_H__ */
