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
 * Managed collection typedefs that aren't in managed_collection.h
 */
typedef struct ib_managed_collection_inst_t ib_managed_collection_inst_t;
typedef struct ib_collection_manager_inst_t ib_collection_manager_inst_t;

/**
 * Collection manager instance (one per managed collection / manager)
 */
struct ib_collection_manager_inst_t {
    const ib_collection_manager_t *manager;    /**< Collection Manager */
    ib_managed_collection_t *collection;       /**< The parent collection */
    const char              *uri;              /**< Associated URI */
    void                    *manager_inst_data;/**< Manager-specific instance */
};

/**
 * Managed collection
 */
struct ib_managed_collection_t {
    const char              *collection_name;  /**< Collection name */
    ib_list_t               *manager_inst_list;/**< list of ..manager_inst_t */
};

/**
 * Managed collection handler data
 */
struct ib_collection_manager_t {
    const char            *name;           /**< Collection manager name */
    const char            *uri_scheme;     /**< URI scheme to ID and strip off*/
    const ib_module_t     *module;         /**< The registering module */
    ib_managed_collection_register_fn_t register_fn;/**< Register function */
    void                  *register_data;  /**< Register function data */
    ib_managed_collection_unregister_fn_t unregister_fn;/**< Unregister func */
    void                  *unregister_data;/**< Unregister function data */
    ib_managed_collection_populate_fn_t populate_fn;  /**< Populate function */
    void                  *populate_data;  /**< Populate function data */
    ib_managed_collection_persist_fn_t  persist_fn;   /**< Persist function */
    void                  *persist_data;   /**< Persist function data */
};

/**
 * Managed collection instance (one per managed collection / tx)
 */
struct ib_managed_collection_inst_t {
    ib_list_t                     *collection_list; /**< TX data collection */
    const ib_managed_collection_t *collection;      /**< Collection object */
};


#endif /* _IB_MANAGED_COLLECTION_PRIVATE_H_ */
