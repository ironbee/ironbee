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
 * @brief IronBee --- Lock Utilities
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/lock.h>

static void lock_destroy(void *cbdata)
{
    ib_lock_t *lock = (ib_lock_t *)cbdata;
    int        rc;

    if (lock == NULL) {
        return;
    }

    rc = pthread_mutex_destroy(lock);
    if (rc != 0) {
        return;
    }

    return;
}

ib_status_t ib_lock_create(ib_lock_t **lock, ib_mm_t mm)
{
    ib_lock_t *l;
    int        rc;

    l = ib_mm_alloc(mm, sizeof(*l));
    if (l == NULL) {
        return IB_EALLOC;
    }

    rc = pthread_mutex_init(l, NULL);
    if (rc != 0) {
        return IB_EALLOC;
    }

    rc = ib_mm_register_cleanup(mm, &lock_destroy, l);
    if (rc != 0) {
        return IB_EOTHER;
    }

    *lock = l;

    return IB_OK;
}

ib_status_t ib_lock_create_malloc(ib_lock_t **lock)
{
    ib_lock_t *l;
    int        rc;

    l = malloc(sizeof(*l));
    if (l == NULL) {
        return IB_EALLOC;
    }

    rc = pthread_mutex_init(l, NULL);
    if (rc != 0) {
        free(l);
        return IB_EALLOC;
    }

    *lock = l;

    return IB_OK;
}

void ib_lock_destroy_malloc(ib_lock_t *lock)
{
    if (lock != NULL) {
        lock_destroy(lock);

        free(lock);
    }
}

ib_status_t ib_lock_lock(ib_lock_t *lock)
{
    int rc = pthread_mutex_lock(lock);
    if (rc != 0) {
        return IB_EUNKNOWN;
    }

    return IB_OK;
}

ib_status_t ib_lock_unlock(ib_lock_t *lock)
{
    int rc = pthread_mutex_unlock(lock);
    if (rc != 0) {
        return IB_EUNKNOWN;
    }

    return IB_OK;
}
