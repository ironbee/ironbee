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
 * Stream processors.
 */

/**
 * Callback function to create a processor.
 *
 * Not all processors need to be created.
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
 * calling @ref ib_stream_processor_data_slice() or
 * @ref ib_stream_processor_data_ref().
 *
 * @sa ib_stream_processor_data_slice()
 * @sa ib_stream_processor_data_ref()
 *
 * @param[in] instance_data The instance data returned by the create call.
 * @param[in] tx The current transaction.
 * @param[in] mp The memory pool for allocating @ref ib_stream_processor_data_t
 *            for @a out and referencing the contents of @a in.
 * @param[in] mm_eval A memory manager that has the lifetime of
 *            only this call. Things allocated out of this
 *            will be destroyed after data has fully passed
 *            to the end of @a ib_stream_pump_t.
 * @param[in] in The list of ib_stream_processor_data_t inputs.
 * @param[out] out An empty list which should be populated with
 *             @ref ib_stream_processor_data_t.
 *             If elements from @a in are put in out,
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
    ib_mpool_freeable_t *mp,
    ib_mm_t              mm_eval,
    ib_list_t           *in,
    ib_list_t           *out,
    void                *cbdata
);

/**
 * Execute this processor on @a in.
 *
 * @param[in] processor The processor.
 * @param[in] tx The current transaction.
 * @param[in] mp The memory pool for allocating @ref ib_stream_processor_data_t
 *            for @a out and referencing the contents of @a in.
 * @param[in] mm_eval A memory manager that has the lifetime of
 *            only this processing call. Things allocated out of this
 *            will be destroyed after data has fully passed
 *            to the end of @a ib_stream_pump_t.
 * @param[in] in List of @ref ib_stream_processor_data_t.
 * @param[out] out An empty list which should be populated with
 *             @ref ib_stream_processor_data_t.
 *             If elements from @a in are put in out,
 *             they should be referenced with
 *             @ref ib_stream_processor_data_ref().
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
    ib_mpool_freeable_t   *mp,
    ib_mm_t                mm_eval,
    ib_list_t             *in,
    ib_list_t             *out
) NONNULL_ATTRIBUTE(1, 2, 3, 5, 6);

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

/**
 * Destroy an instance of a filter.
 *
 * @param[out] inst Destroy the instance data for this filter.
 * @param[in] cbdata Callback data.
 */
typedef void (*ib_stream_processor_destroy_fn)(
    void *instance_data,
    void *cbdata
);

/** @} Stream Processor */

/**
 * @name Stream Processor Data
 * @{
 */

/**
 * Create a segment of filter data.
 *
 * @param[out] data The data.
 * @param[in] mp Memory pool.
 * @param[in] sz The size of the data segment to associate with this
 *            data.
 */
ib_status_t DLL_PUBLIC ib_stream_processor_data_create(
    ib_stream_processor_data_t **data,
    ib_mpool_freeable_t         *mp,
    size_t                       sz
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Create a data segment that contains no data, but signals a data flush.
 *
 * @param[out] data The data.
 * @param[in] mp The memory pool used to create the flush object.
 */
ib_status_t DLL_PUBLIC ib_stream_processor_data_flush_create(
    ib_stream_processor_data_t **data,
    ib_mpool_freeable_t         *mp
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Return the type of this data segment.
 *
 * @param[in] data The data segment to examine.
 *
 * @returns the type of this data segment.
 */
ib_stream_processor_data_type_t DLL_PUBLIC ib_stream_processor_data_type(
    const ib_stream_processor_data_t *data
) NONNULL_ATTRIBUTE(1);

/**
 * Create a segment of pump data that holds a copy of @a src.
 *
 * The lifetime of this data is that of the associated @a pump
 * or until this is explicitly destroyed.
 *
 * @param[out] data The data.
 * @param[in] mp Memory pool
 * @param[in] src The source data to initialize the pump data to.
 * @param[in] sz The size of the data segment to associate with this
 *            data.
 */
ib_status_t DLL_PUBLIC ib_stream_processor_data_cpy(
    ib_stream_processor_data_t **data,
    ib_mpool_freeable_t         *mp,
    const uint8_t               *src,
    size_t                       sz
) NONNULL_ATTRIBUTE(1, 2, 3);

/**
 * Create a new data slice that aliases part of the data of @a src.
 *
 * The reference count to the backing memory store is increased so
 * so that the memory segment will not be freed unexpectedly.
 *
 * If you are slicing a data segment that is of type
 * @ref IB_STREAM_PROCESSOR_FLUSH or similar, where there is
 * no data, just the type information, consider using
 * ib_stream_processor_data_ref() instead. It saves an allocation
 * for the new @a dst structure.
 *
 * @param[out] dst The out value.
 * @param[in] mp The memory pool to slice the data from.
 * @param[in] src The source of the data that will be referenced in @a dst.
 * @param[in] start The start in @a src data to point @a dst at.
 *            Ignored if this is not a data segment.
 * @param[in] length The length after @a start to include in @a dst.
 *            Ignored if this is not a data segment.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If @a start + @a length is greater than the length of @a src.
 * - IB_EALLOC On allocation error.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_processor_data_slice(
    ib_stream_processor_data_t       **dst,
    ib_mpool_freeable_t               *mp,
    const ib_stream_processor_data_t  *src,
    size_t                             start,
    size_t                             length
) NONNULL_ATTRIBUTE(1, 2, 3);

/**
 * Return the pointer to the data.
 *
 * @param[in] data The pump data to examine.
 *
 * @return the pointer to the data.
 */
void DLL_PUBLIC * ib_stream_processor_data_ptr(
    const ib_stream_processor_data_t *data
) NONNULL_ATTRIBUTE(1);

/**
 * Return the length in bytes of the data stored in @a data.
 *
 * @param[in] data The pump data to examine.
 *
 * @return the length in bytes of the data stored in @a data.
 */
size_t DLL_PUBLIC ib_stream_processor_data_len(
    const ib_stream_processor_data_t *data
) NONNULL_ATTRIBUTE(1);

/**
 * Destroy the data.
 *
 * @param[out] data The data object to destroy. If other
 *             @ref ib_stream_processor_data_t structs point at the memory
 *             associated with @a data, then no memory is actually freed.
 * @param[in] mp Memory pool the data came from.
 */
void DLL_PUBLIC ib_stream_processor_data_destroy(
    ib_stream_processor_data_t *data,
    ib_mpool_freeable_t        *mp
) NONNULL_ATTRIBUTE(1,2);

/**
 * Decrease the reference count to @a data, if it hits 0, it will be destroyed.
 *
 * @param[in] data The data segment to manipulate.
 * @param[in] mp The memory pool this data is an allocation from.
 */
void DLL_PUBLIC ib_stream_processor_data_unref(
    ib_stream_processor_data_t *data,
    ib_mpool_freeable_t        *mp
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Increase the reference count to @a data so it will not be destroyed.
 *
 * This is similar to calling ib_stream_processor_data_slice() for the
 * whole range of @a data, but does not require more allocations.
 *
 * @param[in] data The data segment to manipulate.
 * @param[in] mp The memory pool this data is an allocation from.
 *
 * @return
 * - IB_OK On success.
 * - Other on failure.
 */
ib_status_t DLL_PUBLIC ib_stream_processor_data_ref(
    ib_stream_processor_data_t *data,
    ib_mpool_freeable_t        *mp
) NONNULL_ATTRIBUTE(1, 2);

/** @} Stream Processor Data */

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
 * Register a definition of a processor so that it can be constructed.
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
 * Create @a processor by its unique name.
 *
 * @param[in] registry The registry.
 * @param[in] name The unique name of the processor.
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
