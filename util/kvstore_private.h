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

#ifndef __IB_KVSTORE_PRIVATE_H
#define __IB_KVSTORE_PRIVATE_H

#include "ironbee_config_auto.h"

#include <ironbee/clock.h>
#include <ironbee/kvstore.h>
#include <ironbee/types.h>

#include <stdlib.h>

/**
 * @file
 * @brief IronBee --- Key-Value Store Interface
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 * @author Nick LeRoy <nleroy@qualys.com>
 */

/**
 * The connection routine that will initialize the server object.
 *
 * The server object should contain all of the necessary inputs
 * to allow for a connection when passed to the kvstore_connect_t function.
 */
typedef void ib_kvstore_server_t;

/**
 * Connect to the server defined in the kvstore_server_t.
 */
typedef ib_status_t (*ib_kvstore_connect_fn_t)(
    ib_kvstore_t *kvstore,
    ib_kvstore_cbdata_t *cbdata);

/**
 * Disconnect from the server defined in the kvstore_server_t.
 */
typedef ib_status_t (*ib_kvstore_disconnect_fn_t)(
    ib_kvstore_t *kvstore,
    ib_kvstore_cbdata_t *cbdata);

/**
 * Get a value from the data store. This is called by @ref ib_kvstore_get
 * which will free all the allocated results and return the merged final
 * value.
 *
 * @param[in] kvstore The key-value store.
 * @param[in] mm Memory manager that 2a values will be allocated out of.
 * @param[in] key The key to get.
 * @param[out] values An array of values stored at the given key.
 *             This is allocated by @ref ib_kvstore_malloc_fn_t and will be
 *             freed with @ref ib_kvstore_free_fn_t.
 *             Further, each element in this array will be
 *             freed by a call to @ref ib_kvstore_free_fn_t.
 *             These values will be merged before a single value is
 *             returned to the user.
 * @param[out] values_length The length of the array values.
 * @param[in,out] cbdata Callback data passed in during initialization.
 */
typedef ib_status_t (*ib_kvstore_get_fn_t)(
    ib_kvstore_t             *kvstore,
    ib_mm_t                   mm,
    const ib_kvstore_key_t   *key,
    ib_kvstore_value_t     ***values,
    size_t                   *values_length,
    ib_kvstore_cbdata_t      *cbdata
);

/**
 * Set a value in the data store.
 *
 * @param[in] kvstore The key-value store.
 * @param[in] merge_policy If the implementation supports reporting key
 *            conflicts-on-write, the implementation may use this merge
 *            policy to write a single, new value.
 * @param[in] key The key to set.
 * @param[in] value The value to set.
 * @param[in,out] cbdata Callback data passed in during initialization.
 */
typedef ib_status_t (*ib_kvstore_set_fn_t)(
    ib_kvstore_t                 *kvstore,
    ib_kvstore_merge_policy_fn_t  merge_policy,
    const ib_kvstore_key_t       *key,
    ib_kvstore_value_t           *value,
    ib_kvstore_cbdata_t          *cbdata
);

/**
 * Remove a value from the data store.
 *
 * @param[in] kvstore The key-value store.
 * @param[in] key The key of the object to remove.
 * @param[in,out] cbdata Callback data passed in during initialization.
 */
typedef ib_status_t (*ib_kvstore_remove_fn_t)(
    ib_kvstore_t           *kvstore,
    const ib_kvstore_key_t *key,
    ib_kvstore_cbdata_t    *cbdata
);

/**
 * Allocate memory, typically a kvstore_value_t.
 *
 * @param[in] kvstore The key-value store.
 * @param[in] size The size of memory to allocate in bytes.
 * @param[in,out] cbdata Callback data passed in during initialization.
 * @returns Pointer to the memory or NULL on an error.
 */
typedef void * (*ib_kvstore_malloc_fn_t)(
    ib_kvstore_t *kvstore,
    size_t size,
    ib_kvstore_cbdata_t *cbdata);

/**
 * Free anything allocated by kvstore_malloc_t.
 *
 * @param[in] kvstore The key-value store.
 * @param[in,out] ptr The pointer to free.
 * @param[in,out] cbdata Callback data passed in during initialization.
 */
typedef void (*ib_kvstore_free_fn_t)(
    ib_kvstore_t *kvstore,
    void *ptr,
    ib_kvstore_cbdata_t *cbdata);

/**
 * Destruction method.
 * @param[in,out] kvstore The KV store to destroy.
 * @param[in,out] cbdata The callback data for the user.
 */
typedef void (*ib_kvstore_destroy_fn_t)(
    ib_kvstore_t *kvstore,
    ib_kvstore_cbdata_t *cbdata);

/**
 * The key-value store object that contains server information defined
 * by a particular implementation, as well as any requisite metadata.
 *
 * Function pointer in this structure should never be called directly.
 */
struct ib_kvstore_t {
    ib_kvstore_server_t *server; /**< Implementation dependent server data. */

    ib_kvstore_malloc_fn_t malloc; /**< Malloc memory for keys and values. */
    ib_kvstore_cbdata_t *malloc_cbdata; /**< Malloc cbdata. */

    ib_kvstore_free_fn_t free; /**< Free memory for keys and values. */
    ib_kvstore_cbdata_t *free_cbdata; /**< Free cbdata. */

    ib_kvstore_connect_fn_t connect; /**< Method to connect to the server. */
    ib_kvstore_cbdata_t *connect_cbdata; /**< Connect cbdata. */

    ib_kvstore_disconnect_fn_t disconnect; /**< Disconnect from server. */
    ib_kvstore_cbdata_t *disconnect_cbdata; /**< Disconnect cbdata. */

    ib_kvstore_get_fn_t get; /**< Get a value from the key-value store. */
    ib_kvstore_cbdata_t *get_cbdata; /**< Get cbdata. */

    ib_kvstore_set_fn_t set; /**< Set a value in the key-value store. */
    ib_kvstore_cbdata_t *set_cbdata; /**< Set cbdata. */

    ib_kvstore_remove_fn_t remove; /**< Remove a value from the kv store. */
    ib_kvstore_cbdata_t *remove_cbdata; /**< Remove cbdata. */

    ib_kvstore_merge_policy_fn_t default_merge_policy; /**< Default policy. */
    ib_kvstore_cbdata_t *merge_policy_cbdata; /**< Merge cbdata. */

    ib_kvstore_destroy_fn_t destroy; /**< Destroy this ib_kvstore_t. */
    ib_kvstore_cbdata_t *destroy_cbdata; /**< Destroyed cbdata. */
};

#endif
