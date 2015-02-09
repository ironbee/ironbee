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

#ifndef _IB_STREAM_TYPEDEF_H_
#define _IB_STREAM_TYPEDEF_H_

/**
 * @file
 * @brief IronBee --- Stream Processor
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeStreamProcessor Stream Processor
 * @ingroup IronBeeEngine
 *
 * Stream processing model.
 *
 * @{
 */

/**
 * A processor created used by @ref ib_stream_pump_t in a @ref ib_tx_t.
 *
 * This is created by a @ref ib_stream_processor_registry_t.
 * @sa ib_stream_processor_registry_processor_create().
 */
typedef struct ib_stream_processor_t ib_stream_processor_t;

/**
 * Types that a @ref ib_stream_processor_data_t may be.
 */
enum ib_stream_processor_data_type_t {
     IB_STREAM_PROCESSOR_DATA, /**< Data contains a pointer and a length. */
     IB_STREAM_PROCESSOR_FLUSH /**< Contins NULL pointer and length = 0. */
};

/**
 * Types that a @ref ib_stream_processor_data_t may be.
 */
typedef enum ib_stream_processor_data_type_t ib_stream_processor_data_type_t;

/**
 * Reference counted, typed data segment fed to processors.
 *
 * The memory this object holds may be sliced or freed,
 * encouraging safe memory sharing and reuse.
 */
typedef struct ib_stream_processor_data_t ib_stream_processor_data_t;

//! Where processors are registered and how they are instantiated.
typedef struct ib_stream_processor_registry_t ib_stream_processor_registry_t;

/** @} Stream Processor */

/**
 * @defgroup IronBeeStreamPump Stream Pump
 * @ingroup IronBeeEngine
 *
 * Stream processing model.
 *
 * @{
 */

/**
 * A pump that moves data through @ref ib_stream_processor_t.
 */
typedef struct ib_stream_pump_t ib_stream_pump_t;

/** @} Stream Pump */


#ifdef __cplusplus
}
#endif

#endif