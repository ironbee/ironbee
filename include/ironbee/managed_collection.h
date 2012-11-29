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

#ifndef _IB_MANAGED_COLLECTION_H_
#define _IB_MANAGED_COLLECTION_H_

/**
 * @file
 * @brief IronBee --- Managed Collection
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

/**
 * @defgroup IronBeeManagedCollection Managed Collection
 * @ingroup IronBee
 *
 * Definitions and functions related to managed collection
 *
 * @{
 */

#include <ironbee/engine_types.h>
#include <ironbee/types.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Types used only for managed collections
 */
typedef struct ib_managed_collection_t ib_managed_collection_t;

/**
 * Selection callback for managed collections
 *
 * @param[in] ib Engine
 * @param[in] module Collection manager's module object
 * @param[in] mp Memory pool to use for allocations
 * @param[in] collection_name Name of the collection
 * @param[in] params List of parameter strings
 * @param[in] data Selection callback data
 * @param[out] pcollection_data Pointer to manager specific collection data
 *
 * @returns Status code:
 *   - IB_DECLINED Parameters not recognized
 *   - IB_OK All OK, parameters recognized
 *   - IB_Exxx Other error
 */
typedef ib_status_t (* ib_managed_collection_selection_fn_t)(
    const ib_engine_t  *ib,
    const ib_module_t  *module,
    ib_mpool_t         *mp,
    const char         *collection_name,
    const ib_list_t    *params,
    void               *data,
    void              **pcollection_data);

/**
 * Populate callback for managed collections
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction to populate
 * @param[in] module Collection manager's module object
 * @param[in,out] collection Collection to populate
 * @param[in] managed_collection Managed collection data
 * @param[in] data Callback data
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_managed_collection_populate_fn_t)(
    const ib_engine_t *ib,
    const ib_tx_t *tx,
    const ib_module_t *module,
    const char *collection_name,
    void *collection_data,
    ib_list_t *collection,
    void *data);

/**
 * Persist callback for managed collections
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction to select a context for (or NULL)
 * @param[in] module Collection manager's module object
 * @param[in] collection Collection to populate
 * @param[in] managed_collection Managed collection data
 * @param[in] data Callback data
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_managed_collection_persist_fn_t)(
    const ib_engine_t *ib,
    const ib_tx_t *tx,
    const ib_module_t *module,
    const char *collection_name,
    void *collection_data,
    const ib_list_t *collection,
    void *data);

/**
 * Register a managed collection handler
 *
 * @param[in,out] ib Engine
 * @param[in] module Registerring module
 * @param[in] name Name of collection manager being registered
 * @param[in] selection_fn Function to use for manager selection
 * @param[in] selection_data Data passed to @sa selection_fn()
 * @param[in] populate_fn Function to populate the collection (or NULL)
 * @param[in] populate_data Data passed to @sa populate_fn()
 * @param[in] persist_fn Function to persist the collection (or NULL)
 * @param[in] persist_data Data passed to @sa persist_fn()
 *
 * @returns Status code
 */
ib_status_t ib_managed_collection_register_handler(
    ib_engine_t *ib,
    const ib_module_t *module,
    const char *name,
    ib_managed_collection_selection_fn_t selection_fn,
    void *selection_data,
    ib_managed_collection_populate_fn_t populate_fn,
    void *populate_data,
    ib_managed_collection_persist_fn_t persist_fn,
    void *persist_data);

/**
 * Select an appropriate manager and create a managed collection
 *
 * @param[in] ib Engine.
 * @param[in] mp Memory pool to use for allocations
 * @param[in] collection_name Name of the managed collection
 * @param[in] params Parameter list
 * @param[out] pcollection Pointer to new managed collection object
 *
 * @returns Status code
 */
ib_status_t ib_managed_collection_select(
    ib_engine_t *ib,
    ib_mpool_t *mp,
    const char *collection_name,
    const ib_list_t *params,
    const ib_managed_collection_t **pcollection);

/**
 * Populate a managed collection
 *
 * @param[in] ib Engine.
 * @param[in,out] tx Transaction to populate
 * @param[in] collection Managed collection object
 *
 * @returns Status code.
 */
ib_status_t ib_managed_collection_populate(
    const ib_engine_t *ib,
    ib_tx_t *tx,
    const ib_managed_collection_t *collection);

/**
 * Persist all managed collections
 *
 * @param[in] ib Engine.
 * @param[in] tx Transaction.
 *
 * @returns Status code.
 */
ib_status_t ib_managed_collection_persist_all(
    const ib_engine_t *ib,
    ib_tx_t *tx);


/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_MANAGED_COLLECTION_H_ */
