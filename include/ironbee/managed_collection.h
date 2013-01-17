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
 * A collection manager is a collection of functions and related data that can
 * be used to initialize and/or persist TX data.
 */
typedef struct ib_collection_manager_t ib_collection_manager_t;

/**
 * A managed collection is a collection in TX data that can be initialized
 * and/or persisted by a collection manager.
 */
typedef struct ib_managed_collection_t ib_managed_collection_t;

/**
 * Register callback for managed collections
 *
 * This function is called when the collection manager has been matched to the
 * URI of a managed collection (at configuration time) with the URI scheme
 * (specified at registration time).  This function can return IB_DECLINED to
 * decline to manager the given collection.
 *
 * If the collection manager needs collection specific data associated with
 * each manager / collection instance, it should allocate storage for such
 * data, and assign @a pmanger_inst_data to point at this storage.  This
 * "manager instance data" will be passed to the unregister, populate and
 * persist functions in the manager_inst_data parameter.
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
 *   - IB_DECLINED Decline to manage the collection
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
 * This function is called when the collection manager is shutting down.  The
 * collection manager can use this to close file handles, database
 * connections, etc.
 *
 * @param[in] ib Engine
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object
 * @param[in] collection_name Name of the collection
 * @param[in] manager_inst_data Manager instance data from the register fn.
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
 * This function is called during the creation of the managed collection for
 * the transaction (at transaction creation time).  This function can return
 * IB_DECLINE to indicate that it was unable to populate the collection
 * (perhaps because the associated key was not found in the backing store).
 *
 * The @a tx memory pool should be used for allocations.
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction to populate
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object
 * @param[in] collection_name Collection's name
 * @param[in,out] collection Collection to populate
 * @param[in] manager_inst_data Manager instance data from the register fn.
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
 * This function is called at the end of the transaction to allow the manager
 * to persist the collection (at transaction destruction time).  The persist
 * function can return IB_DECLINE to indicate that it was unable to persist
 * the collection (perhaps because the expected fields were not present in the
 * collection, etc.).
 *
 * @param[in] ib Engine
 * @param[in] tx Transaction to select a context for (or NULL)
 * @param[in] module Collection manager's module object
 * @param[in] manager The collection manager object
 * @param[in] collection_name Name of collection to persist
 * @param[in] collection Collection to populate
 * @param[in] manager_inst_data Manager instance data from the register fn.
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
 * The register function (@a register_fn) will be invoked during the
 * configuration process, informing the collection manager that the manager's
 * URI scheme matches the URI; this function can return IB_DECLINED to decline
 * to manager the given collection.  The register function may not be NULL.
 *
 * The unregister function (@a unregister_fn) will be invoked during
 * IronBee shutdown, and the collection manager can use this to close file
 * handles, database connections, etc.  The unregister function may be NULL.
 *
 * The populate function (@a populate_fn) will be invoked after the creation
 * of a transaction's data.  The populate function should populate the
 * collection list with ib_field_t object pointers.  The populate function can
 * return IB_DECLINE to indicate that it was unable to populate the collection
 * (perhaps because the associated key was not found in the backing store).
 * The populate may be NULL.
 *
 * The persist function (@a persist_fn) will be invoked upon close of the
 * transaction.  This function should persist the collection (store it to a
 * file, in a database, etc).  The persist function can return IB_DECLINE to
 * indicate that it was unable to persist the collection (perhaps because the
 * expected fields were not present in the collection, etc.).  The persist
 * function may be NULL.
 *
 * @param[in,out] ib Engine
 * @param[in] module Registering module
 * @param[in] name Name of collection manager being registered
 * @param[in] uri_scheme URI scheme for identification
 * @param[in] register_fn Function to use for manager registration
 * @param[in] register_data Data passed to @a register_fn()
 * @param[in] unregister_fn Function to use for manager unregistration (or NULL)
 * @param[in] unregister_data Data passed to @a unregister_fn()
 * @param[in] populate_fn Function to populate the collection (or NULL)
 * @param[in] populate_data Data passed to @a populate_fn()
 * @param[in] persist_fn Function to persist the collection (or NULL)
 * @param[in] persist_data Data passed to @a persist_fn()
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
 * Create a managed collection object.
 *
 * A managed collection is used to populate and / or persist fields in a
 * collection (the name of which is specified in @a collection_name).  One or
 * more collection managers will be associated with the managed collection by
 * ib_managed_collection_select().
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
 * Select one or more collection managers associated with @a collection
 *
 * The selection process will match the registered URI scheme with each
 * registered collection manager against the URI in @a uri.  If the scheme
 * matches @a uri, the manager's register function is invoked to inform the
 * collection manager of the match.  Note that the register function can
 * return IB_DECLINED to decline to manage the given collection.  All matching
 * managers are then associated with collection.
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
 * Walk through the list of collection managers associate with the given
 * collection, and invoke each of their populate functions.  The first of the
 * populate functions to return IB_OK will cause the population to complete.
 * A populate function can return IB_DECLINED to indicate that it was unable
 * to populate the collection (perhaps because the associated key was not
 * found in the backing store).
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
 * Walk through the list of collection managers associate with the given
 * collection, and invoke each of their persist functions.  Unlinke
 * population, all managers are given the opportunity to populate the given
 * collection.
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
 * Populate a collection from a list (helper function).
 *
 * This function can be used by collection managers to populate a collection
 * from a list of fields.  This is particularly useful for collection managers
 * which build a list of fields, then need to populate the collection with the
 * fields from the list.  The fields will be appropriately copied, etc.
 *
 * @param[in] tx Transaction to populate
 * @param[in] field_list List of fields to populate from
 * @param[in,out] collection Collection to populate with fields from
 *                @a field_list.
 *
 * @returns
 *   - IB_OK on success
 *   - Error returned by ib_field_copy() or ib_list_push().
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
