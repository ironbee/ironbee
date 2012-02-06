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
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#ifndef _IB_LOCK_H_
#define _IB_LOCK_H_

#include "ironbee_config_auto.h"

#include "ironbee/build.h"
#include "ironbee/core.h"
#include "ironbee/release.h"
#include "ironbee/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The lock type for ironbee locks.
 */
typedef int ib_lock_t;

#define IB_LOCK_UNINITIALIZED -1

/**
 * param[in] ib The IronBee engine.
 * param[in,out] lock The lock.
 */
ib_status_t DLL_PUBLIC ib_lock_init(ib_lock_t *lock);

/**
 * param[in] ib The IronBee engine.
 * param[in,out] lock The lock.
 */
ib_status_t DLL_PUBLIC ib_lock_lock(ib_lock_t *lock);

/**
 * param[in] ib The IronBee engine.
 * param[in,out] lock The lock.
 */
ib_status_t DLL_PUBLIC ib_lock_unlock(ib_lock_t *lock);

/**
 * param[in] ib The IronBee engine.
 * param[in,out] lock The lock.
 */
ib_status_t DLL_PUBLIC ib_lock_destroy(ib_lock_t *lock);

#ifdef __cplusplus
}
#endif

#endif // _IB_LOCK_H_
