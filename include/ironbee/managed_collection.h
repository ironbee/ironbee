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
typedef struct ib_collection_manager_t ib_collection_manager_t;

/**
 * Register callback for managed collections
 *
 * This function is called when the collection manager has been matched
 * to the URI of a managed collection (at configuration time)
 *
 * @param[in] ib Engine
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object
 * @param[in] mp Memory pool to use for allocations
 * @param[in] collection_name Name of the collection
 * @param[in] uri Full URI from configuration
 * @param[in] uri_scheme URI scheme
 * @param[in] uri_data Hierarchical/data part of the URI (typically a path)
 * @param[in] params List of parameter strings
 * @param[in] register_data Register callback data
 * @param[out] pmanager_inst_data Pointer to manager-specific instance data
 *
 * @returns Status code:
 *   - IB_OK All OK
 *   - IB_Exxx Other error
 */
typedef ib_status_t (* ib_managed_collection_register_fn_t)(
    const ib_engine_t              *ib,
    const ib_module_t              *module,
    const ib_collection_manager_t  *manager,
    ib_mpool_t                     *mp,
    const char                     *collection_name,
    const char                     *uri,
    const char                     *uri_scheme,
    const char                     *uri_data,
    const ib_list_t                *params,
    void                           *register_data,
    void                          **pmanager_inst_data);

/**
 * Unregister callback for managed collections
 *
 * This function is called when the collection manager is shutting down.
 *
 * @param[in] ib Engine
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object
 * @param[in] collection_name Name of the collection
 * @param[in] manager_inst_data Manager instance data
 * @param[in] unregister_data Register callback data
 *
 * @returns Status code:
 *   - IB_OK All OK
 *   - IB_Exxx Other error
 */
typedef ib_status_t (* ib_managed_collection_unregister_fn_t)(
    const ib_engine_t              *ib,
    const ib_module_t              *module,
    const ib_collection_manager_t  *manager,
    const char                     *collection_name,
    void                           *manager_inst_data,
    void                           *unregister_data);

/**
 * Populate callback for managed collections
 *
 * This function is called during the creation of the managed collection
 * for the transaction (at transaction creation time)
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction to populate
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object
 * @param[in] collection_name Collection's name
 * @param[in,out] collection Collection to populate
 * @param[in] manager_inst_data Manager instance data
 * @param[in] populate_data Populate callback data
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_managed_collection_populate_fn_t)(
    const ib_engine_t             *ib,
    const ib_tx_t                 *tx,
    const ib_module_t             *module,
    const ib_collection_manager_t *manager,
    const char                    *collection_name,
    ib_list_t                     *collection,
    void                          *manager_inst_data,
    void                          *populate_data);

/**
 * Persist callback for managed collections
 *
 * This function is called at the end of the transaction to allow the
 * manager to persist the collection (at transaction destruction time)
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction to select a context for (or NULL)
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object
 * @param[in] collection_name Name of collection to persist
 * @param[in] collection Collection to populate
 * @param[in] manager_inst_data Manager instance data
 * @param[in] persist_data Persist callback data
 *
 * @returns Status code
 */
typedef ib_status_t (* ib_managed_collection_persist_fn_t)(
    const ib_engine_t             *ib,
    const ib_tx_t                 *tx,
    const ib_module_t             *module,
    const ib_collection_manager_t *manager,
    const char                    *collection_name,
    const ib_list_t               *collection,
    void                          *manager_inst_data,
    void                          *persist_data);

/**
 * Register a managed collection handler
 *
 * @param[in,out] ib Engine
 * @param[in] module Registering module
 * @param[in] name Name of collection manager being registered
 * @param[in] uri_scheme URI scheme for identification
 * @param[in] register_fn Function to use for manager registration
 * @param[in] register_data Data passed to @sa register_fn()
 * @param[in] unregister_fn Function to use for manager unregistration
 * @param[in] unregister_data Data passed to @sa unregister_fn()
 * @param[in] populate_fn Function to populate the collection (or NULL)
 * @param[in] populate_data Data passed to @sa populate_fn()
 * @param[in] persist_fn Function to persist the collection (or NULL)
 * @param[in] persist_data Data passed to @sa persist_fn()
 * @param[out] pmanager Pointer to new collection manager object (or NULL)
 *
 * @returns Status code
 */
ib_status_t ib_managed_collection_register_manager(
    ib_engine_t                            *ib,
    const ib_module_t                      *module,
    const char                             *name,
    const char                             *uri_scheme,
    ib_managed_collection_register_fn_t     register_fn,
    void                                   *register_data,
    ib_managed_collection_unregister_fn_t   unregister_fn,
    void                                   *unregister_data,
    ib_managed_collection_populate_fn_t     populate_fn,
    void                                   *populate_data,
    ib_managed_collection_persist_fn_t      persist_fn,
    void                                   *persist_data,
    const ib_collection_manager_t         **pmanager);


/**
 * Create a managed collection object
 *
 * @param[in] ib Engine.
 * @param[in] mp Memory pool to use for allocations
 * @param[in] collection_name Name of the managed collection
 * @param[out] pcollection Pointer to new managed collection object
 *
 * @returns Status code
 */
ib_status_t ib_managed_collection_create(
    ib_engine_t              *ib,
    ib_mpool_t               *mp,
    const char               *collection_name,
    ib_managed_collection_t **pcollection);


/**
 * Unregister all collection managers associated with a managed collection
 *
 * @param[in] ib Engine.
 * @param[in] module Collection manager's module object
 * @param[in] collection Managed collection to unregister
 *
 * @returns Status code
 */
ib_status_t ib_managed_collection_unregister(
    ib_engine_t                   *ib,
    ib_module_t                   *module,
    const ib_managed_collection_t *collection);

/**
 * Select an appropriate manager and create a managed collection
 *
 * @param[in] ib Engine.
 * @param[in] mp Memory pool to use for allocations
 * @param[in] collection_name Name of the managed collection
 * @param[in] uri The URI associated with the managed collection
 * @param[in] params Parameter list
 * @param[in,out] collection Managed collection object
 * @param[out] managers List of selected collection managers (or NULL)
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_managed_collection_select(
    ib_engine_t                    *ib,
    ib_mpool_t                     *mp,
    const char                     *collection_name,
    const char                     *uri,
    const ib_list_t                *params,
    ib_managed_collection_t        *collection,
    ib_list_t                      *managers);

/**
 * Populate a managed collection
 *
 * @param[in] ib Engine.
 * @param[in,out] tx Transaction to populate
 * @param[in] collection Managed collection object
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_managed_collection_populate(
    const ib_engine_t              *ib,
    ib_tx_t                        *tx,
    const ib_managed_collection_t  *collection);

/**
 * Get the name of the collection manager
 *
 * @param[in] manager Collection manager object
 *
 * @returns The name of the collection manager
 */
const char DLL_PUBLIC *ib_managed_collection_manager_name(
    const ib_collection_manager_t  *manager);


/**
 * Persist all managed collections
 *
 * @param[in] ib Engine.
 * @param[in] tx Transaction.
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_managed_collection_persist_all(
    const ib_engine_t              *ib,
    ib_tx_t                        *tx);

/**
 * Populate a collection from a list (helper function)
 *
 * @param[in] tx Transaction to populate
 * @param[in] field_list List of fields to populate from
 * @param[in,out] collection Collection to populate with fields from
 *                @a field_list.
 *
 * @returns
 *   - IB_OK on success
 *   - The first error returned by a call to ib_field_copy or ib_list_push.
 *     The first error is returned, but more errors may occur as the
 *     collection population continues.
 */
ib_status_t DLL_PUBLIC ib_managed_collection_populate_from_list(
    const ib_tx_t                  *tx,
    const ib_list_t                *field_list,
    ib_list_t                      *collection);


/**
 * Initialize managed collection logic
 *
 * @param[in,out] ib IronBee engine
 *
 * @returns
 *   - IB_OK on success
 */
ib_status_t ib_managed_collection_init(
    ib_engine_t *ib);

/**
 * Shutdown managed collection logic
 *
 * @param[in,out] ib IronBee engine
 *
 * @returns
 *   - IB_OK on success
 */
ib_status_t ib_managed_collection_finish(
    ib_engine_t *ib);


/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_MANAGED_COLLECTION_H_ */
