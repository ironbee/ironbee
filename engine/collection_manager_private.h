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

#ifndef _IB_COLLECTION_MANAGER_PRIVATE_H_
#define _IB_COLLECTION_MANAGER_PRIVATE_H_

/**
 * @file
 * @brief IronBee --- Collection Manager Private
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "managed_collection_private.h"

#include <ironbee/collection_manager.h>

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
 * Collection manager instance (one per managed collection / manager)
 */
struct ib_collection_manager_inst_t {
    const ib_collection_manager_t *manager;    /**< Collection Manager */
    ib_managed_collection_t *collection;       /**< The parent collection */
    const char              *uri;              /**< Associated URI */
    void                    *manager_inst_data;/**< Manager-specific instance */
};
typedef struct ib_collection_manager_inst_t ib_collection_manager_inst_t;

#endif /* _IB_COLLECTION_MANAGER_PRIVATE_H_ */
