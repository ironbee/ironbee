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

#ifndef _IB_STREAM_PUMP_H_
#define _IB_STREAM_PUMP_H_

/**
 * @file
 * @brief IronBee --- Stream Pump
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/engine_types.h>
#include <ironbee/mm.h>
#include <ironbee/stream_processor.h>
#include <ironbee/types.h>
#include <ironbee/stream_typedef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeStreamPump Stream Pump
 * @ingroup IronBeeEngine
 *
 * Stream processing using a Pump processing model.
 *
 * @{
 */

/**
 * Create a new pump.
 *
 * @param[out] pump The pump to create.
 * @param[in] registry Registry that provides the pump with @ref ib_stream_processor_t
 *            definitions.
 * @param[in] tx The transaction to use.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_create(
    ib_stream_pump_t               **pump,
    ib_stream_processor_registry_t  *registry,
    ib_tx_t                         *tx
) NONNULL_ATTRIBUTE(1, 2, 3);

/**
 * Create and add a processor to the end of the pump execution list.
 *
 * @param[in] pump The pump.
 * @param[in] name The name to create.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_processor_add(
    ib_stream_pump_t *pump,
    const char       *name
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Create and insert a processor at the given index in the processing list.
 *
 * @param[in] pump The pump.
 * @param[in] name The name to create.
 * @param[in] idx The index to insert at.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_processor_insert(
    ib_stream_pump_t *pump,
    const char       *name,
    size_t            idx
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Return the list of @ref ib_stream_processor_t in this processor.
 *
 * Use the returned list to determine what index to insert
 * a new processor at using ib_stream_pump_processor_insert().
 *
 * @sa ib_stream_pump_processor_insert()
 *
 * @param[in] pump The pump.
 *
 * @returns the list of @ref ib_stream_processor_t in this processor.
 */
const ib_list_t DLL_PUBLIC * ib_stream_pump_processor_list(
    ib_stream_pump_t *pump
) NONNULL_ATTRIBUTE(1);

/**
 * Copy @a data and process it through @a pump.
 *
 * This causes the data to be evaluated by each
 * @ref ib_stream_processor_t in the pump.
 *
 * @param[in] pump The pump that will do the processing.
 * @param[in] data The data to be processed.
 * @param[in] data_len The length of data.
 *
 * @return
 * - IB_OK On success.
 * - Other on failure.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_process(
    ib_stream_pump_t *pump,
    const uint8_t    *data,
    size_t            data_len
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Send flush data through @a pump.
 *
 * @param[in] pump The pump.
 *
 * @return
 * - IB_OK On success.
 * - Other on failure.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_flush(
    ib_stream_pump_t *pump
) NONNULL_ATTRIBUTE(1);

/** @} IronBeeStreamPump */

#ifdef __cplusplus
}
#endif

#endif /* _IB_STREAM_PUMP_H_ */
