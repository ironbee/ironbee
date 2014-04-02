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
 * @brief IronBee Key-Value Store Implementation --- Key-Value Store Implementation
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/bytestr.h>
#include <ironbee/kvstore.h>

#include "kvstore_private.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/**
 * Key type.
 */
struct ib_kvstore_key_t {
    const uint8_t *data;     /**< Data. */
    size_t         data_len; /**< Length of ib_kvstore_key_t::data. */
};

/**
 * Default malloc implementation that wraps malloc.
 * @param[in] kvstore Key-value store.
 * @param[in] size Size in bytes.
 * @param[in] cbdata Callback data. Unused.
 * @returns
 *   - Pointer to the new memory segment.
 *   - Null on error.
 */
static void* kvstore_malloc(
    ib_kvstore_t *kvstore,
    size_t size,
    ib_kvstore_cbdata_t *cbdata)
{
    assert(kvstore != NULL);

    void *r = malloc(size);

    return r;
}

/**
 * Default malloc implementation that wraps free.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] ptr Pointer to free.
 * @param[in] cbdata Callback data. Unused.
 */
static void kvstore_free(
    ib_kvstore_t *kvstore,
    void *ptr,
    ib_kvstore_cbdata_t *cbdata)
{
    assert(kvstore != NULL);

    free(ptr);

    return;
}

/**
 * Trivial merge policy that returns the first value in the list
 * if the list is size 1 or greater.
 *
 * If the list size is 0, this does nothing.
 *
 * @param[in] kvstore Key-value store.
 * @param[in] key The key being considered.
 * @param[in] values Array of @ref ib_kvstore_value_t pointers.
 * @param[in] value_size The length of values.
 * @param[out] resultant_value Pointer to values[0] if value_size > 0.
 * @param[in,out] cbdata Context callback data.
 * @returns IB_OK
 */
static ib_status_t default_merge_policy(
    ib_kvstore_t *kvstore,
    const ib_kvstore_key_t *key,
    ib_kvstore_value_t **values,
    size_t value_size,
    ib_kvstore_value_t **resultant_value,
    ib_kvstore_cbdata_t *cbdata)
{
    assert(kvstore != NULL);
    assert(values != NULL);

    if ( value_size > 0 ) {
        *resultant_value = values[0];
    }

    return IB_OK;
}

size_t ib_kvstore_size(void)
{
    return sizeof(ib_kvstore_t);
}

ib_status_t ib_kvstore_init(ib_kvstore_t *kvstore)
{
    assert(kvstore != NULL);

    kvstore->malloc = &kvstore_malloc;
    kvstore->free = &kvstore_free;
    kvstore->default_merge_policy = &default_merge_policy;

    return IB_OK;
}

ib_status_t ib_kvstore_connect(ib_kvstore_t *kvstore) {
    assert(kvstore != NULL);

    ib_status_t rc =  kvstore->connect(kvstore, kvstore->connect_cbdata);

    return rc;
}

ib_status_t ib_kvstore_disconnect(ib_kvstore_t *kvstore) {
    assert(kvstore != NULL);

    ib_status_t rc = kvstore->disconnect(kvstore, kvstore->disconnect_cbdata);

    return rc;
}

ib_status_t ib_kvstore_get(
    ib_kvstore_t                  *kvstore,
    ib_kvstore_merge_policy_fn_t   merge_policy,
    ib_mm_t                        mm,
    const ib_kvstore_key_t        *key,
    ib_kvstore_value_t           **val
)
{
    assert(kvstore != NULL);
    assert(key != NULL);

    ib_kvstore_value_t  *merged_value = NULL;
    ib_kvstore_value_t **values       = NULL;
    size_t               values_length;
    ib_status_t          rc;
    ib_mm_t              mm_tmp;
    ib_mpool_lite_t     *mp_tmp;

    rc = ib_mpool_lite_create(&mp_tmp);
    if (rc != IB_OK) {
        return rc;
    }

    mm_tmp = ib_mm_mpool_lite(mp_tmp);

    if ( merge_policy == NULL ) {
        merge_policy = kvstore->default_merge_policy;
    }

    rc = kvstore->get(
        kvstore,
        mm_tmp,
        key,
        &values,
        &values_length,
        kvstore->get_cbdata);

    if (rc != IB_OK) {
        *val = NULL;
        goto exit_get;
    }

    /* Merge any values. */
    if (values_length > 1) {
        rc = merge_policy(
            kvstore,
            key,
            values,
            values_length,
            &merged_value,
            kvstore->merge_policy_cbdata);

        if (rc != IB_OK) {
            goto exit_get;
        }

        rc = ib_kvstore_value_dup(mm, merged_value, val);
    }
    else if (values_length == 1 ) {
        rc = ib_kvstore_value_dup(mm, values[0], val);
    }
    else {
        *val = NULL;
        rc = IB_ENOENT;
    }

exit_get:

    ib_mpool_lite_destroy(mp_tmp);

    return rc;
}

ib_status_t ib_kvstore_set(
    ib_kvstore_t *kvstore,
    ib_kvstore_merge_policy_fn_t merge_policy,
    const ib_kvstore_key_t *key,
    ib_kvstore_value_t *val)
{
    assert(kvstore != NULL);
    assert(key != NULL);
    assert(val != NULL);

    ib_status_t rc;

    if ( merge_policy == NULL ) {
        merge_policy = kvstore->default_merge_policy;
    }

    rc = kvstore->set(kvstore, merge_policy, key, val, kvstore->set_cbdata);

    return rc;
}

ib_status_t ib_kvstore_remove(
    ib_kvstore_t *kvstore,
    const ib_kvstore_key_t *key)
{
    assert(kvstore != NULL);
    assert(key != NULL);

    ib_status_t rc = kvstore->remove(kvstore, key, kvstore->remove_cbdata);

    return rc;
}


void ib_kvstore_destroy(ib_kvstore_t *kvstore) {
    assert(kvstore != NULL);
    assert(kvstore->destroy != NULL);

    kvstore->destroy(kvstore, kvstore->destroy_cbdata);
}


ib_status_t DLL_PUBLIC ib_kvstore_key_create(
    ib_kvstore_key_t **key,
    ib_mm_t            mm,
    const uint8_t     *data,
    size_t             data_len
)
{
    assert(key != NULL);

    ib_kvstore_key_t *k;

    k = ib_mm_alloc(mm, sizeof(*k));
    if (k == NULL) {
        return IB_EALLOC;
    }

    k->data = data;
    k->data_len = data_len;

    *key = k;
    return IB_OK;
}

void ib_kvstore_key_set(
    ib_kvstore_key_t *key,
    const uint8_t    *data,
    size_t            data_len
)
{
    assert(key != NULL);

    key->data = data;
    key->data_len = data_len;
}

void ib_kvstore_key_get(
    const ib_kvstore_key_t  *key,
    const uint8_t          **data,
    size_t                  *data_len
)
{
    assert(key != NULL);
    assert(data != NULL);
    assert(data_len != NULL);

    *data = key->data;
    *data_len = key->data_len;
}

/**
 * Value type.
 */
struct ib_kvstore_value_t {
    ib_mpool_lite_t *mp;
    const uint8_t   *value;        /**< The value pointer. A byte array. */
    size_t           value_length; /**< The length of value. */
    const char      *type;         /**< A \0 terminated name of the type. */
    size_t           type_length;  /**< The type name length. */
    ib_time_t        expiration;   /**< Expiration in usec relative to now. */
    ib_time_t        creation;     /**< Creation time in usec */
};

ib_status_t ib_kvstore_value_create(
    ib_kvstore_value_t **kvstore_value,
    ib_mm_t              mm
)
{
    assert(kvstore_value != NULL);

    ib_kvstore_value_t *val;

    val = ib_mm_alloc(mm, sizeof(*val));
    if (val == NULL) {
        return IB_EALLOC;
    }

    val->value        = (uint8_t *)"";
    val->value_length = 0;
    val->type         = "";
    val->type_length  = 0;
    val->expiration   = 0;
    val->creation     = 0;

    *kvstore_value = val;

    return IB_OK;
}

void ib_kvstore_value_value_set(
    ib_kvstore_value_t *kvstore_value,
    const uint8_t      *value,
    size_t              value_length
)
{
    assert(kvstore_value != NULL);
    assert(value != NULL);

    kvstore_value->value = value;
    kvstore_value->value_length = value_length;
}

void ib_kvstore_value_value_get(
    ib_kvstore_value_t  *kvstore_value,
    const uint8_t      **value,
    size_t              *value_length
)
{
    assert(kvstore_value != NULL);
    assert(value != NULL);
    assert(value_length != NULL);

    *value = kvstore_value->value;
    *value_length = kvstore_value->value_length;
}

void ib_kvstore_value_type_get(
    ib_kvstore_value_t  *kvstore_value,
    const char         **type,
    size_t              *type_length
)
{
    assert(kvstore_value != NULL);
    assert(type != NULL);
    assert(type_length != NULL);

    *type = kvstore_value->type;
    *type_length = kvstore_value->type_length;
}

void ib_kvstore_value_type_set(
    ib_kvstore_value_t *kvstore_value,
    const char         *type,
    size_t              type_length
)
{
    assert(kvstore_value != NULL);
    assert(type != NULL);

    kvstore_value->type = type;
    kvstore_value->type_length = type_length;
}

void ib_kvstore_value_expiration_set(
    ib_kvstore_value_t *kvstore_value,
    ib_time_t           expiration
)
{
    assert(kvstore_value != NULL);

    kvstore_value->expiration = expiration;
}

ib_time_t ib_kvstore_value_expiration_get(
    ib_kvstore_value_t *kvstore_value
)
{
    assert(kvstore_value != NULL);

    return kvstore_value->expiration;
}

void ib_kvstore_value_creation_set(
    ib_kvstore_value_t *kvstore_value,
    ib_time_t           creation
)
{
    assert(kvstore_value != NULL);

    kvstore_value->creation = creation;
}

ib_time_t ib_kvstore_value_creation_get(
    ib_kvstore_value_t *kvstore_value
)
{
    assert(kvstore_value != NULL);

    return kvstore_value->creation;
}

ib_status_t ib_kvstore_value_dup(
    ib_mm_t                    mm,
    const ib_kvstore_value_t  *value,
    ib_kvstore_value_t       **pnew_value
)
{
    assert(value != NULL);
    assert(pnew_value != NULL);

    ib_kvstore_value_t *new_value;
    ib_status_t         rc;

    rc = ib_kvstore_value_create(&new_value, mm);
    if (rc != IB_OK) {
        return IB_EALLOC;
    }

    new_value->value = ib_mm_memdup(mm, value->value, value->value_length);
    if (new_value->value == NULL) {
        return IB_EALLOC;
    }

    new_value->type = ib_mm_memdup(mm, value->type, value->type_length);
    if (new_value->type == NULL) {
        return IB_EALLOC;
    }

    new_value->value_length = value->value_length;
    new_value->type_length  = value->type_length;
    new_value->expiration   = value->expiration;
    new_value->creation     = value->creation;

    /* On success, commit back the value.*/
    *pnew_value = new_value;

    return IB_OK;
}

ib_status_t DLL_PUBLIC ib_kvstore_key_dup(
    ib_mm_t                  mm,
    const ib_kvstore_key_t  *in,
    ib_kvstore_key_t       **out
)
{
    assert(in != NULL);
    assert(out != NULL);

    ib_kvstore_key_t *o;

    o = ib_mm_alloc(mm, sizeof(*o));
    if (o == NULL) {
        return IB_EALLOC;
    }

    o->data = ib_mm_memdup(mm, in->data, in->data_len);
    if (o == NULL) {
        return IB_EALLOC;
    }

    o->data_len = in->data_len;

    *out = o;
    return IB_OK;
}
