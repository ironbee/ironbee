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

#ifndef _IB_MANAGED_COLLECTION_PRIVATE_H_
#define _IB_MANAGED_COLLECTION_PRIVATE_H_

/**
 * @file
 * @brief IronBee --- Manged Collection Private Declarations
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/managed_collection.h>


/**
 * A collection manager is a collection of functions and related data that can
 * be used to initialize and/or persist a TX data collection.
 */
struct ib_collection_manager_t {
    const char            *name;           /**< Collection manager name */
    const char            *uri_scheme;     /**< URI scheme to ID and strip off*/
    const ib_module_t     *module;         /**< The registering module */
    ib_collection_manager_register_fn_t register_fn;/**< Register function */
    void                  *register_data;  /**< Register function data */
    ib_collection_manager_unregister_fn_t unregister_fn;/**< Unregister func */
    void                  *unregister_data;/**< Unregister function data */
    ib_collection_manager_populate_fn_t populate_fn;  /**< Populate function */
    void                  *populate_data;  /**< Populate function data */
    ib_collection_manager_persist_fn_t  persist_fn;   /**< Persist function */
    void                  *persist_data;   /**< Persist function data */
};

/**
 * A managed collection is a collection in TX data that can be initialized
 * and/or persisted by a collection manager.
 */
struct ib_managed_collection_t {
    const char              *collection_name;  /**< Collection name */
    ib_list_t               *manager_inst_list;/**< list of ..manager_inst_t */
};
typedef struct ib_managed_collection_t ib_managed_collection_t;

/**
 * Collection manager instance (one per managed collection / manager)
 */
struct ib_collection_manager_inst_t {
    const ib_collection_manager_t *manager;    /**< Collection Manager */
    ib_managed_collection_t *collection;       /**< The parent collection */
    const char              *uri;              /**< Associated URI */
    void                    *manager_inst_data;/**< Manager-specific instance */
};
typedef struct ib_collection_manager_inst_t ib_collection_manager_inst_t;

/**
 * Managed collection instance (one per managed collection / tx)
 */
struct ib_managed_collection_inst_t {
    ib_list_t                     *collection_list; /**< TX data collection */
    const ib_managed_collection_t *collection;      /**< Collection object */
};
typedef struct ib_managed_collection_inst_t ib_managed_collection_inst_t;

/**
 * Initialize managed collection logic.
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
 * Destroy a managed collection object.
 *
 * This function will walk through the collection managers associated with @a
 * collection, and will invoke the unregister function for each, and removes
 * all manager associations from @a collection.
 *
 * @param[in] ib Engine.
 * @param[in] collection Managed collection to unregister
 *
 * @returns Status code
 */
ib_status_t ib_managed_collection_destroy(
    ib_engine_t                   *ib,
    const ib_managed_collection_t *collection);

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
ib_status_t DLL_PUBLIC ib_managed_collection_persist_tx(
    const ib_engine_t              *ib,
    ib_tx_t                        *tx);

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


#endif /* _IB_MANAGED_COLLECTION_PRIVATE_H_ */
