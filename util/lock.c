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
 *****************************************************************************/

/**
 * @file
 * @brief IronBee - Lock Utilities
 * @author Sam Baskinger <sbaskinger@qualys.com>
 *
 * @internal
 */

#include <ironbee/lock.h>

#include <ironbee/debug.h>

ib_status_t ib_lock_init(ib_lock_t *lock)
{
    IB_FTRACE_INIT();

    int rc = pthread_mutex_init(lock, NULL);
    if (rc != 0) {
//        ib_util_log_error(3, "Failed to initialize mutex: %d", rc);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }


    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_lock_lock(ib_lock_t *lock)
{
    IB_FTRACE_INIT();

    int rc = pthread_mutex_lock(lock);
    if (rc != 0) {
//        ib_util_log_error(3, "Failed to lock mutex: %d", rc);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_lock_unlock(ib_lock_t *lock)
{
    IB_FTRACE_INIT();

    int rc = pthread_mutex_unlock(lock);
    if (rc != 0) {
//        ib_util_log_error(3, "Failed to unlock mutex: %d", rc);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_lock_destroy(ib_lock_t *lock)
{
    IB_FTRACE_INIT();

    int rc = pthread_mutex_destroy(lock);
    if (rc != 0) {
//        ib_util_log_error(3, "Failed to clean up mutex: %d", rc);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

