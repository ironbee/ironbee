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

/**
 * @file
 * @brief IronBee Key-Value Store Implementation --- Key-Value Store Implmemtnation
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "ironbee/kvstore.h"
#include "ironbee/debug.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

/**
 * Default malloc implmemtation that wraps malloc.
 * @param[in] kvstore Key-value store.
 * @param[in] size Size in bytes.
 * @param[in] cbdata Callback data. Unused.
 * @returns
 *   - Pointer to the new memory segment.
 *   - Null on error.
 */
static void* kvstore_malloc(kvstore_t *kvstore, size_t size, ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();

    assert(kvstore);

    void *r = malloc(size);

    IB_FTRACE_RET_PTR((void*), r);
}

/**
 * Default malloc implmemtation that wraps free.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] ptr Pointer to free.
 * @param[in] cbdata Callback data. Unused.
 */
static void kvstore_free(kvstore_t *kvstore, void *ptr, ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();

    assert(kvstore);

    free(ptr);

    IB_FTRACE_RET_VOID();
}

/**
 * @param[in] kvstore The Key Value store.
 * @param[in] value The value that will be duplicated.
 * @returns Pointer to the duplicate value or NULL.
 */
static kvstore_value_t * kvstore_value_dup(
    kvstore_t *kvstore,
    kvstore_value_t *value)
{
    IB_FTRACE_INIT();

    assert(kvstore);
    assert(value);

    kvstore_value_t *new_value = kvstore->malloc(
        kvstore,
        sizeof(*new_value),
        kvstore->cbdata);

    if (!new_value) {
        IB_FTRACE_RET_PTR((kvstore_value_t*), NULL);
    }

    new_value->value = kvstore->malloc(
        kvstore,
        value->value_length,
        kvstore->cbdata);

    if (!new_value->value) {
        kvstore->free(kvstore, new_value, kvstore->cbdata);
        IB_FTRACE_RET_PTR((kvstore_value_t*), NULL);
    }

    new_value->type = kvstore->malloc(
        kvstore,
        value->type_length,
        kvstore->cbdata);

    if (!new_value->type) {
        kvstore->free(kvstore, new_value->value, kvstore->cbdata);
        kvstore->free(kvstore, new_value, kvstore->cbdata);
        IB_FTRACE_RET_PTR((kvstore_value_t*), NULL);
    }

    /* Copy in all data. */
    new_value->expiration = value->expiration;
    new_value->value_length = value->value_length;
    new_value->type_length = value->type_length;
    memcpy(new_value->value, value->value, value->value_length);
    memcpy(new_value->type, value->type, value->type_length);

    IB_FTRACE_RET_PTR((kvstore_value_t*), new_value);
}

/**
 * Trivial merge policy that returns the first value in the list 
 * if the list is size 1 or greater.
 *
 * If the list size is 0, this does nothing.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] values Array of @ref kvstore_value_t pointers.
 * @param[in] value_size The length of values.
 * @param[out] resultant_value Pointer to values[0] if value_size > 0.
 * @param[in,out] cbdata Context callback data.
 * @returns IB_OK
 */
static ib_status_t default_merge_policy(
    kvstore_t *kvstore,
    kvstore_value_t **values,
    size_t value_size,
    kvstore_value_t **resultant_value,
    ib_kvstore_cbdata_t *cbdata)
{
    IB_FTRACE_INIT();

    assert(kvstore);
    assert(values);

    if ( value_size > 0 ) {
        *resultant_value = values[0];
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t kvstore_init(kvstore_t *kvstore, ib_kvstore_cbdata_t *cbdata) {
    IB_FTRACE_INIT();

    assert(kvstore);

    kvstore->malloc = &kvstore_malloc;
    kvstore->free = &kvstore_free;
    kvstore->default_merge_policy = &default_merge_policy;
    kvstore->cbdata = cbdata;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t kvstore_connect(kvstore_t *kvstore) {
    IB_FTRACE_INIT();

    assert(kvstore);

    ib_status_t rc =  kvstore->connect(kvstore, kvstore->cbdata);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t kvstore_disconnect(kvstore_t *kvstore) {
    IB_FTRACE_INIT();

    assert(kvstore);

    ib_status_t rc = kvstore->disconnect(kvstore, kvstore->cbdata);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t kvstore_get(
    kvstore_t *kvstore,
    kvstore_merge_policy_fn_t merge_policy,
    const kvstore_key_t *key,
    kvstore_value_t **val)
{
    IB_FTRACE_INIT();

    assert(kvstore);
    assert(key);

    kvstore_value_t *merged_value = NULL;
    kvstore_value_t **values = NULL;
    size_t values_length;
    ib_status_t rc;
    size_t i;

    if ( merge_policy == NULL ) {
        merge_policy = kvstore->default_merge_policy;
    }

    rc = kvstore->get(kvstore, key, &values, &values_length, kvstore->cbdata);

    if (rc) {
        *val = NULL;
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Merge any values. */
    if (values_length > 1) {
        rc = merge_policy(
            kvstore,
            values,
            values_length,
            &merged_value,
            kvstore->cbdata);

        if (rc != IB_OK) {
            goto exit_get;
        }

        *val = kvstore_value_dup(kvstore, merged_value);
    }
    else if (values_length == 1 ) {
        *val = kvstore_value_dup(kvstore, values[0]);
    }
    else {
        *val = NULL;
    }

exit_get:
    for (i=0; i < values_length; ++i) {
        /* If the merge policy returns a pointer to a value array element,
         * null it to avoid a double free. */
        if ( merged_value == values[i] ) {
            merged_value = NULL;
        }
        kvstore_free_value(kvstore, values[i]);
    }

    if (values) {
        kvstore->free(kvstore, values, kvstore->cbdata);
    }

    /* Never free the user's value. Only free we allocated. */
    if (merged_value) {
        kvstore_free_value(kvstore, merged_value);
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t kvstore_set(
    kvstore_t *kvstore,
    kvstore_merge_policy_fn_t merge_policy,
    const kvstore_key_t *key,
    kvstore_value_t *val)
{
    IB_FTRACE_INIT();

    assert(kvstore);
    assert(key);
    assert(val);

    ib_status_t rc;

    if ( merge_policy == NULL ) {
        merge_policy = kvstore->default_merge_policy;
    }

    rc = kvstore->set(kvstore, merge_policy, key, val, kvstore->cbdata);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t kvstore_remove(kvstore_t *kvstore, const kvstore_key_t *key)
{
    IB_FTRACE_INIT();

    assert(kvstore);
    assert(key);

    ib_status_t rc = kvstore->remove(kvstore, key, kvstore->cbdata); 

    IB_FTRACE_RET_STATUS(rc);
}


void kvstore_free_value(kvstore_t *kvstore, kvstore_value_t *value) {
    IB_FTRACE_INIT();

    assert(kvstore);
    assert(value);

    if (value->value) {
        kvstore->free(kvstore, value->value, kvstore->cbdata);
    }

    if (value->type) {
        kvstore->free(kvstore, value->type, kvstore->cbdata);
    }

    kvstore->free(kvstore, value, kvstore->cbdata);

    IB_FTRACE_RET_VOID();
}

void kvstore_free_key(kvstore_t *kvstore, kvstore_key_t *key) {
    IB_FTRACE_INIT();

    assert(kvstore);
    assert(key);

    if (key->key) {
        kvstore->free(kvstore, key->key, kvstore->cbdata);
    }

    kvstore->free(kvstore, key, kvstore->cbdata);

    IB_FTRACE_RET_VOID();
}

