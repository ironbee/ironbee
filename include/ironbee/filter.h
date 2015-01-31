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

#ifndef _IB_FILTER_H_
#define _IB_FILTER_H_

/**
 * @file
 * @brief IronBee --- Filter
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/mm.h>
#include <ironbee/types.h>
#include <ironbee/list.h>
#include <ironbee/mpool_freeable.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilFilter Filters
 * @ingroup IronBeeUtil
 *
 * Filter processing API for handling large data.
 *
 * @{
 */

/**
 * The definition of a filter which is made an instance used.
 */
typedef struct ib_filter_t ib_filter_t;

/**
 * An instances of @ref ib_filter_t with the duration of the engine.
 */
typedef struct ib_filter_inst_t ib_filter_inst_t;

/**
 * Callback function to create a filter if it is needed in a transaction.
 *
 * Not all filters need to be created, especially if they contain no state.
 *
 * @param[out] filter_inst The filter instance. Assign to this
 *            by casting it to a void **. Eg `*(void **)instance_data = ...`
 * @param[in] filter The filter to instantiate.
 * @param[in] arg Creation time argument.
 * @param[in] Callback data.
 *
 * @return
 * - IB_OK on success.
 * - Other on error.
 */
typedef ib_status_t (*ib_filter_create_fn)(
    void              *instance_data,
    ib_mm_t            mm,
    ib_filter_t       *filter,
    void              *arg,
    void              *cbdata
);

/**
 * Execute a filter.
 *
 * This may be called many times, and in parallel. It may even be
 * called for the same data segment depending on how the user
 * decides to construct the full pipeline.
 *
 * Document implementations carefully and make use of the instance and callback
 * data.
 *
 * @param[in] inst The instance.
 * @param[in] mm_eval A memory manager that exists only for the call of this
 *            execute function and for the use of @a out and @a out_type.
 * @param[in] mp Memory pool to create @ref ib_filter_data_t from.
 * @param[in] in The list of ib_stream_pump_data_t inputs.
 * @param[out] out The list of ib_stream_pump_data_t output values.
 *             This list may never contain elements of @a in.
 *             If you wish to reference the data found in @a in
 *             then use ib_stream_pump_data_slice() to reference
 *             data in a safe way without copying it.
 * @param[in] cbdata Callback data.
 *
 * @return
 * - IB_OK On success.
 * - Other on error.
 */
typedef ib_status_t (*ib_filter_execute_fn)(
    ib_filter_inst_t        *inst,
    void                    *instance_data,
    ib_mpool_freeable_t     *mp,
    ib_mm_t                  mm_eval,
    const ib_list_t         *in,
    ib_list_t               *out,
    void                    *cbdata
);

/**
 * Destroy an instance of a filter.
 *
 * @param[out] inst Destroy the instance data for this filter.
 * @param[in] cbdata Callback data.
 *
 */
typedef void (*ib_filter_destroy_fn)(
    void             *instance_data,
    void             *cbdata
);

enum ib_filter_data_type_t {
     IB_FILTER_DATA,
     IB_FILTER_FLUSH
};
typedef enum ib_filter_data_type_t ib_filter_data_type_t;

/**
 * A data segment used in filters.
 *
 * A useful API is defined for this datatype that
 * promotes buffer resuse and avoids copies.
 */
typedef struct ib_filter_data_t ib_filter_data_t;

/**
 * @name Filter
 *
 * Definitions of a filter.
 * @{
 */

/**
 * Create a filter.
 *
 * @param[out] filter The filter we are creating.
 * @param[in] mm Memory manager to allocate from.
 * @param[in] name The name of this filter.
 * @param[in] type The type of this filter.
 * @param[in] create_fn The create function. May be null.
 * @param[in] create_cbdata The callback data for the create function.
 * @param[in] execute_fn The execute function.
 * @param[in] execute_cbdata The callback data for the execute function.
 * @param[in] destroy_fn The destroy function. May be null.
 * @param[in] destroy_cbdata The callback data for the data destroy function.
 *
 * @return
 * - IB_OK On success.
 * - IB_EALLOC On allocation error from pump->mm.
 * - IB_EINVAL If there is a filter by the same name.
 */
ib_status_t DLL_PUBLIC ib_filter_create(
    ib_filter_t          **filter,
    ib_mm_t                mm,
    const char            *name,
    const char            *type,
    ib_filter_create_fn    create_fn,
    void                  *create_cbdata,
    ib_filter_execute_fn   execute_fn,
    void                  *execute_cbdata,
    ib_filter_destroy_fn   destroy_fn,
    void                  *destroy_cbdata
) NONNULL_ATTRIBUTE(3, 4);

/**
 * Return the name.
 *
 * @param[in] filter The filter to get the name from.
 *
 * @returns The name.
 */
const char DLL_PUBLIC *ib_filter_name(
    const ib_filter_t *filter
) NONNULL_ATTRIBUTE(1);

/**
 * Return the type.
 *
 * @param[in] filter The filter to get the type from.
 *
 * @returns The type.
 */
const char DLL_PUBLIC *ib_filter_type(
    const ib_filter_t *filter
) NONNULL_ATTRIBUTE(1);

/** @} Filter */

/**
 * @name Filter Instances
 *
 * Instances of @ ib_filter_t .
 * @{
 */

/**
 * Create an instance of a stream filter.
 *
 * @param[out] filter_inst The filter instance.
 * @param[in] mm Memory manager to create out of and that will
 *            destroy @a filter_inst.
 * @param[in] filter The filter to create.
 * @param[in] arg The argument to the create function.
 */
ib_status_t DLL_PUBLIC ib_filter_inst_create(
    ib_filter_inst_t **filter_inst,
    ib_mm_t            mm,
    ib_filter_t       *filter,
    void              *arg
) NONNULL_ATTRIBUTE(1, 3);

/**
 * Connect two filters.
 *
 * @param[in] filter The filter that will pass data to the next filter.
 * @param[in] next The next filter.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_filter_inst_add(
    ib_filter_inst_t *filter,
    ib_filter_inst_t *next
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Return the @ref ib_filter_t that defines @a filter_inst.
 *
 * @param[in] filter_inst The filter instance to interrogate.
 *
 * @return the @ref ib_filter_t that defines @a filter_inst.
 */
ib_filter_t DLL_PUBLIC *ib_filter_inst_filter(
    ib_filter_inst_t *filter_inst
) NONNULL_ATTRIBUTE(1);

/**
 * Process a @ref ib_filter_inst_t.
 *
 * This is typically called by an @ref ib_stream_pump_inst_t.
 *
 * @param[in] filter_inst The filter instance to execute on the data.
 * @param[in] mp The memory pool for creating @ref ib_filter_data_t from.
 * @param[in] mm_eval Allocations for this evaluation should come from here.
 *            This should be considered destroyed bewteen calls.
 * @param[in] data An @ref ib_list_t of @ref ib_filter_data_t.
 *
 * @returns
 * - IB_OK On success.
 * - IB_DECLINED If the user implementation returns IB_DECLINED. Processing
 *   stops gracefully.
 * - Other on error.
 *
 * @sa ib_filter_insts_process().
 */
ib_status_t ib_filter_inst_process(
    ib_filter_inst_t   *filter_inst,
    ib_mpool_freeable_t *mp,
    ib_mm_t              mm_eval,
    const ib_list_t     *data
) NONNULL_ATTRIBUTE(1, 2, 4);

/**
 * Process a list of @ref ib_filter_inst_t s.
 *
 * This is typically called by an @ref ib_stream_pump_inst_t.
 *
 * @param[in] filter_insts An @ref ib_list_t of @ref ib_filter_inst_t.
 * @param[in] mp The memory pool for creating @ref ib_filter_data_t from.
 * @param[in] mm_eval Allocations for this evaluation should come from here.
 *            This should be considered destroyed bewteen calls.
 * @param[in] data An @ref ib_list_t of @ref ib_filter_data_t.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 *
 * @sa ib_filter_inst_process().
 */
ib_status_t ib_filter_insts_process(
    ib_list_t           *filter_insts,
    ib_mpool_freeable_t *mp,
    ib_mm_t              mm_eval,
    const ib_list_t     *data
) NONNULL_ATTRIBUTE(1, 2, 4);


/** @} Filter Instances */


/**
 * @name Filter Data
 *
 * A way to represent filter data in a way to allow multiple references
 * but without encouraging memory leaks.
 *
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
ib_status_t DLL_PUBLIC ib_filter_data_create(
    ib_filter_data_t    **data,
    ib_mpool_freeable_t  *mp,
    size_t                sz
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Create a data segment that contains no data, but signals a data flush.
 *
 * @param[out] data The data.
 * @param[in] mp The memory pool used to create the flush object.
 */
ib_status_t DLL_PUBLIC ib_filter_data_flush_create(
    ib_filter_data_t    **data,
    ib_mpool_freeable_t  *mp
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Return the type of this data segment.
 *
 * @param[in] data The data segment to examine.
 *
 * @returns the type of this data segment.
 */
ib_filter_data_type_t DLL_PUBLIC ib_filter_data_type(
    const ib_filter_data_t *data
) NONNULL_ATTRIBUTE(1);

/**
 * Create a segment of pump data.
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
ib_status_t DLL_PUBLIC ib_filter_data_cpy(
    ib_filter_data_t    **data,
    ib_mpool_freeable_t  *mp,
    const uint8_t        *src,
    size_t                sz
) NONNULL_ATTRIBUTE(1, 2, 3);

/**
 * Create a new data slice that aliases part of the data of @a src.
 *
 * Create pump data that points at the same memory used by @a src.
 * This API guarantees that the memory backing @a dst and @a src will
 * not be freed until all @ref ib_filter_data_t structs are destroyed.
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
ib_status_t DLL_PUBLIC ib_filter_data_slice(
    ib_filter_data_t      **dst,
    ib_mpool_freeable_t    *mp,
    const ib_filter_data_t *src,
    size_t                  start,
    size_t                  length
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Return the pointer to the data.
 *
 * @param[in] data The pump data to examine.
 *
 * @return the pointer to the data.
 */
void DLL_PUBLIC * ib_filter_data_ptr(
    const ib_filter_data_t *data
) NONNULL_ATTRIBUTE(1);

/**
 * Return the length in bytes of the data stored in @a data.
 *
 * @param[in] data The pump data to examine.
 *
 * @return the length in bytes of the data stored in @a data.
 */
size_t DLL_PUBLIC ib_filter_data_len(
    const ib_filter_data_t *data
) NONNULL_ATTRIBUTE(1);

/**
 * Destroy the data.
 *
 * @param[out] data The data object to destroy. If other
 *             @ref ib_filter_data_t structs point at the memory
 *             associated with @a data, then no memory is actually freed.
 * @param[in] mp Memory pool the data came from.
 */
void DLL_PUBLIC ib_filter_data_destroy(
    ib_filter_data_t    *data,
    ib_mpool_freeable_t *mp
) NONNULL_ATTRIBUTE(1,2);

/** @} Filter Data */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_FILTER_H_ */
