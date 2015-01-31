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

#ifndef _IB_ENGINE_FILTER_PRIVATE_H_
#define _IB_ENGINE_FILTER_PRIVATE_H_

/**
 * @file
 * @brief IronBee --- Engine Filter Private Declarations
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */
#include "ironbee_config_auto.h"

#include <ironbee/engine_types.h>
#include <ironbee/core.h>

/**
 * Private initialization routine for the streams in the IronBee engine.
 *
 * This is locate in the file @c engine_filter.c.
 *
 * This will create ib->respons_body_stream and ib->request_body_stream.
 * To those will be added the "raw" filter which is the initial filter.
 *
 * @param[in] ib The engine.
 * @param[in] mm The memory manager to initialize.
 * @param[in] core_module Module structure for core.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_LOCAL ib_core_filter_init(
    ib_engine_t *ib,
    ib_mm_t      mm,
    ib_module_t *core_module
) NONNULL_ATTRIBUTE(1);

/**
 * Initialize the pump instance in @a tx for the given context.
 *
 * @param[in] tx The transaction to initialize.
 * @param[in] corecfg The core module configuration to use.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_LOCAL ib_core_filter_tx_init(
    ib_tx_t       *tx,
    ib_core_cfg_t *corecfg
) NONNULL_ATTRIBUTE(1, 2);

#endif /* _IB_ENGINE_FILTER_PRIVATE_H_ */
