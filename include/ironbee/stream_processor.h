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

#ifndef _IB_STREAM_PROCESSOR_H_
#define _IB_STREAM_PROCESSOR_H_

/**
 * @file
 * @brief IronBee --- Stream Processor
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/engine_types.h>
#include <ironbee/mm.h>
#include <ironbee/types.h>
#include <ironbee/stream_typedef.h>
#include <ironbee/mpool_freeable.h>
#include <ironbee/stream_io.h>

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
 * @name Stream Processor
 *
 * Stream processors transform unbounded data presented in
 * chunks. They are managed by a @ref ib_stream_pump_t which manages
 * the passing of data.
 */

/**
 * Construct a processor instance.
 *
 * @param[out] instance_data The processor instance data. Return this
 *            by casting it to a void **. Eg `*(void **)instance_data = ...`
 * @param[in] tx Transaction this is being created for.
 * @param[in] cbdata Callback data.
 *
 * @return
 * - IB_OK on success.
 * - Other on error.
 */
typedef ib_status_t (*ib_stream_processor_create_fn)(
    void    *instance_data,
    ib_tx_t *tx,
    void    *cbdata
);

/**
 * Execute a processor.
 *
 * A processor may *not* keep references to any of the arguments passed in
 * with the exception of @ref ib_stream_processor_data_t elements of
 * @a in that have their reference counts increased either by
 * calling @ref ib_stream_processor_data_ref_slice() or
 * @ref ib_stream_processor_data_ref().
 *
 * @sa ib_stream_processor_data_ref_slice()
 * @sa ib_stream_processor_data_ref()
 *
 * @param[in] instance_data The instance data returned by the create call.
 * @param[in] tx The current transaction.
 * @param[in] mm_eval A memory manager that has the lifetime of
 *            only this call. Things allocated out of this
 *            will be destroyed after data has fully passed
 *            to the end of @a ib_stream_pump_t.
 * @param[in] in The list of ib_stream_processor_data_t inputs.
 * @param[out] out An empty list which should be populated with
 *             @ref ib_stream_processor_data_t.
 *             If elements from @a in are put in @a out,
 *             they should be referenced with
 *             @ref ib_stream_processor_data_ref().
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK When data is successfully returned in @a out.
 * - IB_DECLINED This processor declined to generate any output
 *               based on @a in, and @a in should be used at this
 *               processor's @a out list.
 *               In a chain of processors (such as in @ref ib_stream_pump_t)
 *               this effectively forwards @a in to the next
 *               @a ib_stream_processor_t without modification.
 * - Other on an error.
 */
typedef ib_status_t (*ib_stream_processor_execute_fn)(
    void                *instance_data,
    ib_tx_t             *tx,
    ib_mm_t              mm_eval,
    ib_stream_io_tx_t   *io_tx,
    void                *cbdata
);

/**
 * Destroy the instance data for a processor instance.
 *
 * @param[in,out] inst Instance data created by
 *                @ref ib_stream_processor_create_fn.
 * @param[in] cbdata Callback data.
 */
typedef void (*ib_stream_processor_destroy_fn)(
    void *instance_data,
    void *cbdata
);

/**
 * Execute this processor on @a in.
 *
 * @param[in] processor The processor.
 * @param[in] tx The current transaction.
 * @param[in] mm_eval A memory manager that has the lifetime of
 *            only this processing call. Things allocated out of this
 *            will be destroyed after data has fully passed
 *            to the end of @a ib_stream_pump_t.
 * @param[in] io_tx The io transaction to get and put data into.
 *
 * @returns
 * - IB_OK When data is successfully returned in @a out.
 * - IB_DECLINED This processor declined to generate any output
 *               based on @a in, and @a in should be used at this
 *               processor's @a out list.
 *               In a chain of processors (such as in @ref ib_stream_pump_t)
 *               this effectively forwards @a in to the next
 *               @a ib_stream_processor_t without modification.
 * - Other on an error.
 */
ib_status_t DLL_PUBLIC ib_stream_processor_execute(
    ib_stream_processor_t *processor,
    ib_tx_t               *tx,
    ib_mm_t                mm_eval,
    ib_stream_io_tx_t     *io_tx
) NONNULL_ATTRIBUTE(1, 2, 4);

/**
 * Returns the unique name this processor's definition is registered under.
 *
 * Use this name to create new instances of processors.
 *
 * @param[in] processor The processor.
 *
 * @returns the unique name this processor's definition is registered under.
 */
const char DLL_PUBLIC * ib_stream_processor_name(
    ib_stream_processor_t *processor
) NONNULL_ATTRIBUTE(1);

/**
 * Returns the immutable list of types this processor can handle.
 *
 * @param[in] processor The processor.
 *
 * @returns the immutable list of types this processor can handle.
 */
const ib_list_t DLL_PUBLIC * ib_stream_processor_types(
    ib_stream_processor_t *processor
) NONNULL_ATTRIBUTE(1);

/** @} Stream Processor */

/**
 * @name Stream Processor Registry
 *
 * How to store definitions of stream processors.
 */

/**
 * Create a registry.
 *
 * A registry holds the definition of @ref ib_stream_processor_t.
 *
 * @param[out] registry The registry to create.
 * @param[in] mm The memory manager to create @a registry from.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation.
 */
ib_status_t DLL_PUBLIC ib_stream_processor_registry_create(
    ib_stream_processor_registry_t **registry,
    ib_mm_t                          mm
) NONNULL_ATTRIBUTE(1);

/**
 * Register a processor definition that will be instantiated at runtime.
 *
 * @param[in] registry The registry to add this definition to.
 * @param[in] name A unique name used to create an instance of
 *            this processor.
 * @param[in] types A list of types (const char *) to register
 *            this processor as being able to handle.
 * @param[in] create_fn Create an instance of this processor.
 * @param[in] create_cbdata Callback data.
 * @param[in] execute_fn Execute this processor against some data.
 * @param[in] execute_cbdata Callback data.
 * @param[in] destroy_fn Destroy an instance of the processor.
 * @param[in] destroy_cbdata Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation error.
 * - IB_EINVAL If @a name is already defined or possibly another API
 *             failure.
 * - Other on an unexpected error.
 */
ib_status_t DLL_PUBLIC ib_stream_processor_registry_register(
    ib_stream_processor_registry_t *registry,
    const char                     *name,
    const ib_list_t                *types,
    ib_stream_processor_create_fn   create_fn,
    void                           *create_cbdata,
    ib_stream_processor_execute_fn  execute_fn,
    void                           *execute_cbdata,
    ib_stream_processor_destroy_fn  destroy_fn,
    void                           *destroy_cbdata
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Create @a processor from it's registered definition.
 *
 * @param[in] registry The registry.
 * @param[in] name The unique, registered name of the processor.
 * @param[in] processor The processor created.
 * @param[in] tx The transaction this processor is for.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC Allocation error.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_processor_registry_processor_create(
    ib_stream_processor_registry_t  *registry,
    const char                      *name,
    ib_stream_processor_t          **processor,
    ib_tx_t                         *tx
) NONNULL_ATTRIBUTE(1, 2, 3);

/**
 * Find a list of names that are registered under @a type.
 *
 * Use this to find a list of @ref ib_stream_processor_t that can handle
 * a particular type of data.
 *
 * @param[in] registry The registry.
 * @param[in] type The type to look up.
 * @param[out] names A list that const char * names of
 *             processors will be added to. These names
 *             may be passed to ib_stream_processor_registry_processor_create().
 *
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If no processors can handle @a type. In this case
 *   @a names is left unchanged.
 * - Other on failures to append to @a names.
 */
ib_status_t DLL_PUBLIC ib_stream_processor_registry_names_find(
    ib_stream_processor_registry_t  *registry,
    const char                      *type,
    ib_list_t                       *names
) NONNULL_ATTRIBUTE(1, 2, 3);

/** @} Stream Processor Registry */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_STREAM_PROCESSOR_H_ */
