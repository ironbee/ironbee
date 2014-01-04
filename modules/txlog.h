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
#define TXLOG_FORMAT_FN_NAME "TxLogModuleFormatFn"

#ifdef __cplusplus
} /* extern C */
#endif

#endif /* __MODULES__TXLOG_H__ */
