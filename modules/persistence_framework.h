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

#ifndef _ENGINE__PERSISTENCE_FRAMEWORK_H_
#define _ENGINE__PERSISTENCE_FRAMEWORK_H_

/**
 * @file
 * @brief IronBee Engine --- Persistence Framework
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/engine_types.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>

#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeEnginePersistenceFramework IronBee Persistence Framework
 * @ingroup IronBeeEngine
 *
 * This is the API for the IronBee persistence framework.
 *
 * The persistence framework is designed to make it easy for a module
 * writer to...
 *
 * - register persistence implementations at configuration time,
 * - instantiate named instances of those implementations,
 * - link those named instances to collections or instantiate anonymous
 *   instances of an implementation,
 * @{
 */

typedef struct ib_persist_fw_t ib_persist_fw_t;

typedef ib_status_t (* ib_persist_fw_create_fn_t)(
    ib_engine_t      *ib,
    const ib_list_t  *params,
    void             *impl,
    void             *cbdata
);
typedef void (* ib_persist_fw_destroy_fn_t)(
    void *impl,
    void *cbdata
);
typedef ib_status_t (* ib_persist_fw_load_fn_t)(
    void *impl,
    ib_tx_t *tx,
    const char *key,
    size_t key_length,
    ib_list_t *list,
    void *cbdata
);
typedef ib_status_t (* ib_persist_fw_store_fn_t)(
    void *impl,
    ib_tx_t *tx,
    const char *key,
    size_t key_length,
    ib_time_t expiration,
    const ib_list_t *list,
    void *cbdata
);

/**
 * Create a new persistence framework.
 * @param[in,out] ib The IronBee engine this persistence engine will be
 *                created from and registered to.
 *                The main memory manager of @a ib is used.
 * @param[in] module The user's module.
 * @param[out] persist_fw The persistence framework object.
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On Allocation error.
 */
ib_status_t DLL_PUBLIC ib_persist_fw_create(
    ib_engine_t      *ib,
    ib_module_t      *module,
    ib_persist_fw_t **persist_fw
);

/**
 * Register a set of functions that handle a particular type.
 *
 * The callback functions and callback data values may all be NULL and that
 * function is skipped. For example, read-only persistence stores
 * may pass in a NULL @a store_fn.
 *
 * @param[in] persist_fw The persistence instance.
 * @param[in] ctx Configuration context the handler is defined in.
 * @param[in] type The name of the type this handles.
 *            Calls to ib_persist_fw_create_store() should pass in this
 *            type to create an store that uses this registration.
 * @param[in] create_fn Create callback.
 * @param[in] create_data Callback data.
 * @param[in] destroy_fn Destroy callback.
 * @param[in] destroy_data Callback data.
 * @param[in] load_fn Load callback.
 * @param[in] load_data Callback data.
 * @param[in] store_fn Store callback.
 * @param[in] store_data Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EEXIST If @a name is already defined.
 * - Other
 */
ib_status_t DLL_PUBLIC ib_persist_fw_register_type(
    ib_persist_fw_t            *persist_fw,
    ib_context_t                *ctx,
    const char                 *type,
    ib_persist_fw_create_fn_t   create_fn,
    void                       *create_data,
    ib_persist_fw_destroy_fn_t  destroy_fn,
    void                       *destroy_data,
    ib_persist_fw_load_fn_t     load_fn,
    void                       *load_data,
    ib_persist_fw_store_fn_t    store_fn,
    void                       *store_data
);

/**
 * Fetch a registered type handler and create and instance of that type.
 *
 * @param[in] persist_fw The persistence instance.
 * @param[in] ctx Configuration context the store is defined in.
 * @param[in] type The type.
 * @param[in] name The name to store the newly created type under.
 * @param[in] params A list of strings (null-terminated @c char @c *)
 *            passed to the @ref ib_persist_fw_create_fn_t for the
 *            given type.
 *
 * @sa ib_persist_fw_register_type()
 *
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT Store is not found.
 * - IB_EEXIST If @a name is already defined.
 * - Other
 */
ib_status_t DLL_PUBLIC ib_persist_fw_create_store(
    ib_persist_fw_t *persist_fw,
    ib_context_t    *ctx,
    const char      *type,
    const char      *name,
    const ib_list_t *params
);

/**
 * Map a collection to a named store created by ib_persist_fw_create_store().
 *
 * @param[in] persist_fw The persistence instance.
 * @param[in] ctx The configuration context this will be in.
 * @param[in] name The name of the collection to map.
 * @param[in] key The key to store the collection under in the store.
 *            This will be expanded against the transaction fields.
 * @param[in] expiration Expiration, in seconds, of the record at @a key.
 * @param[in] key_length Length of @a key.
 * @param[in] store The store name.
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If @a name is already defined.
 * - Other on failure.
 */
ib_status_t DLL_PUBLIC ib_persist_fw_map_collection(
    ib_persist_fw_t *persist_fw,
    ib_context_t    *ctx,
    const char      *name,
    const char      *key,
    size_t           key_length,
    ib_num_t         expiration,
    const char      *store
);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* _ENGINE__PERSISTENCE_FRAMEWORK_H_ */
