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
 * @brief IronBee --- Utility Functions
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "ironbee/kvstore.h"
#include "ironbee/debug.h"

#include <string.h>
#include <stdlib.h>

static void* kvstore_malloc(kvstore_t *kvstore, size_t size)
{
    IB_FTRACE_INIT();

    void *r = malloc(size);

    IB_FTRACE_RET_PTR((void*), r);
}

static void kvstore_free(kvstore_t *kvstore, void *ptr)
{
    IB_FTRACE_INIT();

    free(ptr);

    IB_FTRACE_RET_VOID();
}

static ib_status_t default_merge_policy(
    kvstore_t *kvstore,
    kvstore_value_t *current_value,
    kvstore_value_t **values,
    size_t value_size,
    kvstore_value_t **resultant_value)
{
    IB_FTRACE_INIT();

    *resultant_value = current_value;
    return IB_OK;
}

ib_status_t kvstore_init(kvstore_t *kvstore) {
    IB_FTRACE_INIT();

    memset(kvstore, 1, sizeof(*kvstore));
    kvstore->malloc = &kvstore_malloc;
    kvstore->free = &kvstore_free;
    kvstore->default_merge_policy = &default_merge_policy;
    return IB_OK;
}

ib_status_t kvstore_connect(kvstore_t *kvstore) {
    IB_FTRACE_INIT();

    ib_status_t rc =  kvstore->connect(kvstore);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t kvstore_disconnect(kvstore_t *kvstore) {
    IB_FTRACE_INIT();

    ib_status_t rc = kvstore->disconnect(kvstore);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t kvstore_get(
    kvstore_t *kvstore,
    kvstore_merge_policy_t merge_policy,
    const kvstore_key_t *key,
    kvstore_value_t *val)
{
    IB_FTRACE_INIT();

    kvstore_value_t *merged_value;
    kvstore_value_t **values;
    size_t values_length;
    ib_status_t rc;
    size_t i;

    if ( merge_policy == NULL ) {
        merge_policy = kvstore->default_merge_policy;
    }

    rc = kvstore->get(kvstore, key, &values, &values_length);

    if (rc != IB_OK) {
        return rc;
    }

    if (values_length > 0) {
        rc = merge_policy(kvstore, val, values, values_length, &merged_value);

        if (rc != IB_OK) {
            goto exit_get;
        }

        if ( merged_value != val ) {
            /* Shallow-copy merged values into user-provided value. */
            *val = *merged_value;
            
            /* Free the value container, not the member pointers. */
            kvstore->free(kvstore, merged_value);

            /* NULL merged_value so it is not double-freed later. */
            merged_value = NULL;
        }
    }

exit_get:

    /* Never free the user's value. Only free we allocated. */
    if (merged_value != val && merged_value != NULL) {
        kvstore_free_value(kvstore, merged_value);
    }

    for (i=0; i < values_length; ++i) {
        kvstore_free_value(kvstore, values[i]);
    }

    kvstore->free(kvstore, values);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t kvstore_set(
    kvstore_t *kvstore,
    kvstore_merge_policy_t merge_policy,
    const kvstore_key_t *key,
    kvstore_value_t *val)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    if ( merge_policy == NULL ) {
        merge_policy = kvstore->default_merge_policy;
    }

    rc = kvstore->set(kvstore, merge_policy, key, val);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t kvstore_remove(kvstore_t *kvstore, const kvstore_key_t *key)
{
    IB_FTRACE_INIT();

    ib_status_t rc = kvstore->remove(kvstore, key); 

    IB_FTRACE_RET_STATUS(rc);
}


void kvstore_free_value(kvstore_t *kvstore, kvstore_value_t *value) {
    IB_FTRACE_INIT();

    if (value->value) {
        kvstore->free(kvstore, value->value);
    }

    if (value->type) {
        kvstore->free(kvstore, value->type);
    }

    kvstore->free(kvstore, value);

    IB_FTRACE_RET_VOID();
}

void kvstore_free_key(kvstore_t *kvstore, kvstore_key_t *key) {
    IB_FTRACE_INIT();

    if (key->key) {
        kvstore->free(kvstore, key->key);
    }

    kvstore->free(kvstore, key);

    IB_FTRACE_RET_VOID();
}

