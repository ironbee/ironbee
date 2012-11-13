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

#ifndef __IB_KVSTORE_H
#define __IB_KVSTORE_H

#include "ironbee_config_auto.h"

#include "ironbee/types.h"

#include <stdlib.h>

/**
 * @file
 * @brief IronBee --- Key-Value Store Interface
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

/**
 * @defgroup IronBeeKeyValueStore Key-Value Store
 * @ingroup IronBeeUtil
 *
 * This defines an API, and a few basic implementations thereof, for
 * storing values under keys with logic for reconciling key collisions.
 *
 * Its full realization is an interface to a NoSQL distributed data store.
 *
 * @{
 */

/**
 * The connection routine that will initialize the server object.
 *
 * The server object should contain all of the necessary inputs
 * to allow for a connection when passed to the kvstore_connect_t function.
 */
typedef void kvstore_server_t;

/**
 * Type of a callback object for implementations of a key-value store.
 */
typedef void ib_kvstore_cbdata_t;

typedef struct kvstore_t kvstore_t;
typedef struct kvstore_value_t kvstore_value_t;
typedef struct kvstore_key_t kvstore_key_t;


/**
 * Connect to the server defined in the kvstore_server_t.
 */
typedef ib_status_t (*kvstore_connect_fn_t)(
    kvstore_server_t *,
    ib_kvstore_cbdata_t *cbdata);

/**
 * Disconnect from the server defined in the kvstore_server_t.
 */
typedef ib_status_t (*kvstore_disconnect_fn_t)(
    kvstore_server_t *,
    ib_kvstore_cbdata_t *cbdata);

/**
 * Merges multiple values together.
 * Implementations may initialize temporary new values, but must free them all.
 *
 * @param[in] kvstore The key-value store object.
 * @param[in] user_value The value that is currently intended to be
 *            written.
 * @param[in] values The array of all values that are already stored at
 *            the given key. Optimally this will be 0 or 1, but
 *            depending on the storage engine guarantees, this might be 
 *            much higher.
 * @param[in] value_length The length of values.
 * @param[out] resultant_value This is a created value that may be 
 *             free'd with the implementation's free function.
 * @param[in,out] cbdata Callback data passed in during initialization.
 */
typedef ib_status_t (*kvstore_merge_policy_fn_t)(
    kvstore_t *kvstore,
    kvstore_value_t **values,
    size_t value_length,
    kvstore_value_t **resultant_value,
    ib_kvstore_cbdata_t *cbdata);

/**
 * Get a value from the data store. This is called by @ref kvstore_get
 * which will free all the allocated results and return the merged final
 * value.
 *
 * @param[in] kvstore The key-value store.
 * @param[in] key The key to get.
 * @param[out] values An array of values stored at the given key.
 *             This is allocated by @ref kvstore_malloc_fn_t and will be
 *             freed with @ref kvstore_free_fn_t.
 *             Further, each element in this array will be
 *             freed by a call to @ref kvstore_free_fn_t.
 *             These values will be merged before a single value is
 *             returned to the user.
 * @param[out] values_length The length of the array values.
 * @param[in,out] cbdata Callback data passed in during initialization.
 */
typedef ib_status_t (*kvstore_get_fn_t)(
    kvstore_t *kvstore,
    const kvstore_key_t *key,
    kvstore_value_t ***values,
    size_t *values_length,
    ib_kvstore_cbdata_t *cbdata);

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
typedef ib_status_t (*kvstore_set_fn_t)(
    kvstore_t *kvstore,
    kvstore_merge_policy_fn_t merge_policy,
    const kvstore_key_t *key,
    kvstore_value_t *value,
    ib_kvstore_cbdata_t *cbdata);

/**
 * Remove a value from the data store.
 *
 * @param[in] kvstore The key-value store.
 * @param[in] key The key of the object to remove.
 * @param[in,out] cbdata Callback data passed in during initialization.
 */
typedef ib_status_t (*kvstore_remove_fn_t)(
    kvstore_t *kvstore,
    const kvstore_key_t *key,
    ib_kvstore_cbdata_t *cbdata);

/**
 * Allocate memory, typically a kvstore_value_t.
 *
 * @param[in] kvstore The key-value store.
 * @param[in] size The size of memory to allocate in bytes.
 * @param[in,out] cbdata Callback data passed in during initialization.
 * @returns Pointer to the memory or NULL on an error.
 */
typedef void * (*kvstore_malloc_fn_t)(
    kvstore_t *kvstore,
    size_t size,
    ib_kvstore_cbdata_t *cbdata);

/**
 * Free anything allocated by kvstore_malloc_t.
 *
 * @param[in] kvstore The key-value store.
 * @param[in,out] ptr The pointer to free.
 * @param[in,out] cbdata Callback data passed in during initialization.
 */
typedef void (*kvstore_free_fn_t)(
    kvstore_t *kvstore,
    void *ptr,
    ib_kvstore_cbdata_t *cbdata);

/**
 * Value type.
 */
struct kvstore_value_t {
    void *value;         /**< The value pointer. */
    size_t value_length; /**< The length of value. */
    char *type;          /**< The name of the type. */
    size_t type_length;  /**< The type name length. */
    uint32_t expiration; /**< The expiration in seconds relative to now. */
};

/**
 * Key type.
 */
struct kvstore_key_t {
    size_t length;
    void *key;
};

/**
 * The key-value store object that contains server information defined
 * by a particular implementation, as well as any requisite metadata.
 *
 * Function pointer in this structure should never be called directly.
 */
struct kvstore_t {
    kvstore_server_t *server; /**< Implementation dependent server data. */
    kvstore_malloc_fn_t malloc; /**< Malloc memory for keys and values. */
    kvstore_free_fn_t free; /**< Malloc memory for keys and values. */
    kvstore_connect_fn_t connect; /**< Method to connect to the server. */
    kvstore_disconnect_fn_t disconnect; /**< Disconnect from the server. */
    kvstore_get_fn_t get; /**< Get a value from the key-value store. */
    kvstore_set_fn_t set; /**< Set a value in the key-value store. */
    kvstore_remove_fn_t remove; /**< Remove a value from the key-value store. */
    kvstore_merge_policy_fn_t default_merge_policy; /**< Def. merge policy. */
    ib_kvstore_cbdata_t *cbdata; /**< Callback Data unrelated to server. */
};

/**
 * Initialize a kvstore.
 *
 * This will zero the structure and then 
 * set server to the server parameter, malloc is set to the system malloc
 * definition, free is set to the system free implementation,
 * and default_merge_policy is set to a function that returns the original
 * value.
 *
 * This function is not enough to fully initialize a kvstore_t.
 * You must also define the other function pointers according to 
 * their respective contracts.
 *
 * See:
 *  - @ref kvstore_connect_fn_t connect
 *  - @ref kvstore_disconnect_fn_t disconnect
 *  - @ref kvstore_get_fn_t get
 *  - @ref kvstore_set_fn_t set
 *  - @ref kvstore_remove_fn_t remove
 *
 * @param[out] kvstore The server object which is initialized.
 * @param[in] cbdata Data provided to all callback calls. Implementations
 *            of a key-value store should hide this parameter from the user
 *            as they do the server parameter. Both are opaque
 *            data pointers that only the implementation uses.
 *
 * @returns IB_OK
 */
ib_status_t kvstore_init(kvstore_t *kvstore, ib_kvstore_cbdata_t *cbdata);

/**
 * Connect to the server by calling @ref kvstore_connect_fn_t in kvstore.
 *
 * @param [in,out] kvstore The kvstore that is connected to the data store.
 * @return
 *   - IB_OK on success
 *   - Implementation-defined other value.
 */
ib_status_t kvstore_connect(kvstore_t *kvstore);

/**
 * Disconnect from the server by calling @ref kvstore_disconnect_fn_t in
 * kvstore.
 *
 * @return
 *   - IB_OK on success
 *   - Implementation-defined other value.
 */
ib_status_t kvstore_disconnect(kvstore_t *kvstore);

/**
 * Get the named value.
 * @param[in] kvstore The key-value store object.
 * @param[in] merge_policy The function pointer that merges colliding keys.
 *            If null then the @c default_merge_policy in kvstore is used.
 * @param[in] key The key that will be written to.
 * @param[out] val The stored value. If multiple values are fetched,
 *                 they are merged. This value is populated with
 *                 a value allocated by kvstore->malloc
 *                 and should be freed with kvstore_value_destroy.
 * @return
 *   - IB_OK on success
 *   - IB_EALLOC on memory allocation error.
 *   - Implementation-defined other value.
 */
ib_status_t kvstore_get(
    kvstore_t *kvstore,
    kvstore_merge_policy_fn_t merge_policy,
    const kvstore_key_t *key,
    kvstore_value_t **val);

/**
 * Set a value. If a key-conflict is detected on write, then the
 * merge method is used to combine the values and re-write them.
 *
 * @param[in] kvstore The key-value store object.
 * @param[in] merge_policy The function pointer that merges colliding keys.
 *            If null then the @c default_merge_policy in kvstore is used.
 * @param[in] key The key that will be written to.
 * @param[in,out] val The value that will be written. If the @c merge_policy
 *                is employed val is updated to reflect them merged value.
 * @return
 *   - IB_OK on success
 *   - IB_EALLOC on memory allocation error.
 *   - Implementation-defined other value.
 */
ib_status_t kvstore_set(
    kvstore_t *kvstore,
    kvstore_merge_policy_fn_t merge_policy,
    const kvstore_key_t *key,
    kvstore_value_t *val);

/**
 * Remove all stored values under the given key.
 *
 * @param[in] kvstore The key-value store.
 * @param[in] key The key to remove.
 * @return
 *   - IB_OK on success
 *   - Implementation-specific error code.
 */
ib_status_t kvstore_remove(kvstore_t *kvstore, const kvstore_key_t *key);

/**
 * Free the value pointer and all member elements.
 *
 * @param[in] kvstore The key value store.
 * @param[in,out] value The value to be freed using @ref kvstore_free_fn_t.
 */
void kvstore_free_value(kvstore_t *kvstore, kvstore_value_t *value);

/**
 * Free the key pointer and all member elements.
 *
 * @param[in] kvstore The key value store.
 * @param[in,out] key The key to be freed using @ref kvstore_free_fn_t.
 */
void kvstore_free_key(kvstore_t *kvstore, kvstore_key_t *key);

/**
 * @} Key Value Store
 */
#endif
