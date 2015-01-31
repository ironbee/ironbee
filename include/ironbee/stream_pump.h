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
#include <ironbee/mm.h>
#include <ironbee/types.h>
#include <ironbee/filter.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeStreamPump Stream Pump
 * @ingroup IronBeeUtil
 *
 * Stream processing using a Pump processing model.
 *
 * @{
 */

/**
 * A container for registering and creating @ref ib_filter_t objects.
 */
typedef struct ib_stream_pump_t ib_stream_pump_t;

/**
 * Holds @ref ib_filter_inst_t objects and passed data through them.
 *
 * In the context of IronBee, this is a transaction scoped object.
 */
typedef struct ib_stream_pump_inst_t ib_stream_pump_inst_t;

/**
 * @name Stream Pump
 * @{
 */

/**
 * Create a pump.
 *
 * @param[out] pump The out-value. Unchanged if there is an error.
 * @param[in] mm The memory manager to schedule the destruction of pump in.
 *            No allocation is actually done from this mm, it is just a
 *            lifetime management element.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation error.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_create(
    ib_stream_pump_t **pump,
    ib_mm_t            mm
) NONNULL_ATTRIBUTE(1);

/**
 * Add a filter to the pump.
 *
 * The name and type of @a filter are used to register this filter.
 *
 * This will make the filter available to ib_stream_pump_add().
 *
 * @param[in] pump
 * @param[in] filter The filter to add.
 *
 * @sa ib_filter_name()
 * @sa ib_filter_type()
 *
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If a filter exists with the same name.
 * - Other on internal error.
 */
ib_status_t ib_stream_pump_add(
    ib_stream_pump_t        *pump,
    ib_filter_t             *filter
);

/**
 * Find @a filter by @a name.
 *
 * Names are unique, so there may be at most a single filter.
 *
 * @param[in] pump The pump.
 * @param[in] name The name to search with.
 * @param[out] filter The filter found.
 *
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If filter was not found.
 * - Other on another error.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_filter_find(
    ib_stream_pump_t  *pump,
    const char        *name,
    ib_filter_t      **filter
) NONNULL_ATTRIBUTE(1, 2, 3);

/**
 * Find all @a filters by @a type.
 *
 * Find the recorded types and return them.
 *
 * @param[in] pump The pump.
 * @param[in] type The type to search with.
 * @param[out] filters A list into which found @ref ib_filter_t
 *             pointers are deposited.
 *
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If filter was not found.
 * - Other on another error.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_filters_find(
    ib_stream_pump_t *pump,
    const char       *type,
    ib_list_t        *filters
) NONNULL_ATTRIBUTE(1, 2, 3);

/** @} Stream Pump */

/**
 * @name Stream Pump Instance
 * @{
 */

/**
 * Create a stream pump instance for use in a transaction.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_inst_create(
    ib_stream_pump_inst_t **stream_pump_inst,
    ib_stream_pump_t       *stream_pump,
    ib_mm_t                 mm
) NONNULL_ATTRIBUTE(1,2);

/**
 * Add a filter instance to the initial filters list of @a pump_isnt.
 *
 * This filter instance will get the first, raw data.
 *
 * @param[in] pump_inst The pump to add to .
 * @param[in] filter The filter to add.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_inst_add(
    ib_stream_pump_inst_t *pump_inst,
    ib_filter_inst_t      *filter
) NONNULL_ATTRIBUTE(1, 2);


/**
 * Process @a data through @a pump.
 *
 * @param[in] pump_inst The pump that will do the processing.
 * @param[in] data The data to be processed.
 * @param[in] data_len The length of data.
 *
 * @return
 * - IB_OK On success.
 * - Other on failure.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_inst_process(
    ib_stream_pump_inst_t *pump_inst,
    const uint8_t         *data,
    size_t                 data_len
) NONNULL_ATTRIBUTE(1, 2);

/**
 * Send flush data through @a pump.
 *
 * @param[in] pump_inst The pump.
 *
 * @return
 * - IB_OK On success.
 * - Other on failure.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_inst_flush(
    ib_stream_pump_inst_t *pump_inst
) NONNULL_ATTRIBUTE(1);

/**
 * Add filter by name to @a pump_inst.
 *
 * The @ref ib_filter_t must have been added to the @ref ib_stream_pump_t
 * that created @a pump_inst using ib_stream_pump_add() before this call.
 *
 * The created @ref ib_filter_inst_t is added to the list of
 * initial filters used. See ib_stream_pump_inst_filter_add()
 * if you only want the filter to be created and not added.
 *
 * Filters created in this manner are destroyed when @a pump_inst is
 * destroyed.
 *
 * @param[in] pump_inst The pump to use.
 * @param[in] name The name to find.
 * @param[in] arg The argument to pass to the create function.
 *
 * @sa ib_stream_pump_add().
 * @sa ib_stream_pump_inst_add().
 * @sa ib_stream_pump_inst_name_add().
 * @sa ib_stream_pump_inst_name_create().
 * @sa ib_stream_pump_inst_type_add().
 * @sa ib_stream_pump_inst_type_create().
 *
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If the named filter is not found.
 * - Other on error.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_inst_name_add(
    ib_stream_pump_inst_t *pump_inst,
    const char            *name,
    void                  *arg
) NONNULL_ATTRIBUTE(1, 2, 3);

/**
 * Create filter by name with a lifetime of @a pump_inst.
 * See ib_stream_pump_inst_name_add() for documentation.
 */
ib_status_t DLL_PUBLIC ib_stream_pump_inst_name_create(
    ib_stream_pump_inst_t *pump_inst,
    const char            *name,
    void                  *arg,
    ib_filter_inst_t     **filter_inst
) NONNULL_ATTRIBUTE(1, 2, 3, 4);


/** @} Stream Pump Instance. */

/** @} IronBeeStreamPump */

#ifdef __cplusplus
}
#endif

#endif /* _IB_STREAM_PUMP_H_ */
