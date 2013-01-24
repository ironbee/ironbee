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

#include <ironbee/clock.h>
#include <ironbee/types.h>

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
 * Type of a callback object for implementations of a key-value store.
 */
typedef void ib_kvstore_cbdata_t;

typedef struct ib_kvstore_t ib_kvstore_t;
typedef struct ib_kvstore_value_t ib_kvstore_value_t;
typedef struct ib_kvstore_key_t ib_kvstore_key_t;


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
typedef ib_status_t (*ib_kvstore_merge_policy_fn_t)(
    ib_kvstore_t *kvstore,
    ib_kvstore_value_t **values,
    size_t value_length,
    ib_kvstore_value_t **resultant_value,
    ib_kvstore_cbdata_t *cbdata);

/**
 * Value type.
 */
struct ib_kvstore_value_t {
    void *value;             /**< The value pointer. A byte array. */
    size_t value_length;     /**< The length of value. */
    char *type;              /**< A \0 terminated name of the type. */
    size_t type_length;      /**< The type name length. */
    ib_time_t expiration;    /**< The expiration in useconds relative to now. */
    ib_time_t creation;      /**< The value's creation time in useconds */
};

/**
 * Key type.
 */
struct ib_kvstore_key_t {
    const void *key;
    size_t length;
};

/**
 * Get the size of an @sa ib_kvstore_t object for use in allocations
 *
 * @returns Size in bytes
 */
size_t ib_kvstore_size(void);

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
 * @sa ib_kvstore_connect_fn_t connect
 * @sa ib_kvstore_disconnect_fn_t disconnect
 * @sa ib_kvstore_get_fn_t get
 * @sa ib_kvstore_set_fn_t set
 * @sa ib_kvstore_remove_fn_t remove
 *
 * @param[out] kvstore The server object which is initialized.
 *
 * @returns IB_OK
 */
ib_status_t ib_kvstore_init(ib_kvstore_t *kvstore);

/**
 * Connect to the server by calling the connect function in @a kvstore.
 *
 * @param [in,out] kvstore The kvstore that is connected to the data store.
 * @return
 *   - IB_OK on success
 *   - Implementation-defined other value.
 */
ib_status_t ib_kvstore_connect(ib_kvstore_t *kvstore);

/**
 * Disconnect from the server by calling the disconnect in @a kvstore.
 *
 * @return
 *   - IB_OK on success
 *   - Implementation-defined other value.
 */
ib_status_t ib_kvstore_disconnect(ib_kvstore_t *kvstore);

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
ib_status_t ib_kvstore_get(
    ib_kvstore_t *kvstore,
    ib_kvstore_merge_policy_fn_t merge_policy,
    const ib_kvstore_key_t *key,
    ib_kvstore_value_t **val);

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
ib_status_t ib_kvstore_set(
    ib_kvstore_t *kvstore,
    ib_kvstore_merge_policy_fn_t merge_policy,
    const ib_kvstore_key_t *key,
    ib_kvstore_value_t *val);

/**
 * Remove all stored values under the given key.
 *
 * @param[in] kvstore The key-value store.
 * @param[in] key The key to remove.
 * @return
 *   - IB_OK on success
 *   - Implementation-specific error code.
 */
ib_status_t ib_kvstore_remove(
    ib_kvstore_t *kvstore,
    const ib_kvstore_key_t *key);

/**
 * Free the value pointer and all member elements.
 *
 * @param[in] kvstore The key value store.
 * @param[in,out] value The value to be freed using the
 *                @a kvstore's free function.
 */
void ib_kvstore_free_value(ib_kvstore_t *kvstore, ib_kvstore_value_t *value);

/**
 * Free the key pointer and all member elements.
 *
 * @param[in] kvstore The key value store.
 * @param[in,out] key The key to be freed using @a kvstore's free.
 */
void ib_kvstore_free_key(ib_kvstore_t *kvstore, ib_kvstore_key_t *key);

/**
 * Destroy this kvstore.
 * @param[in,out] kvstore The Key-value store.
 */

void ib_kvstore_destroy(ib_kvstore_t *kvstore);

/**
 * @} Key Value Store
 */
#endif
