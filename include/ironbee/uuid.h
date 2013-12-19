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

#ifndef _IB_UUID_H_
#define _IB_UUID_H_

/**
 * @file
 * @brief IronBee --- UUID Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup IronBeeUtilUUID UUID
 * @ingroup IronBeeUtil
 *
 * Code to generate, convert, and manipulate UUIDs.
 *
 * @{
 */

/*! Length of a UUID string with NUL. */
#define IB_UUID_LENGTH 37

/**
 * Initialize UUID library.
 *
 * ib_util_initialize() will call this.
 */
ib_status_t DLL_PUBLIC ib_uuid_initialize(void);

/**
 * Shutdown UUID library.
 *
 * ib_util_shutdown() will call this.
 */
ib_status_t DLL_PUBLIC ib_uuid_shutdown(void);

/**
 * Creates a new, random, v4 uuid (static buffer version).
 *
 * @param[in] uuid Where to write UUID.  Must be IB_UUID_LENGTH long.
 *
 * @returns
 * - IB_EALLOC on allocation failure.
 * - IB_EOTHER on other failure.
 */
ib_status_t DLL_PUBLIC ib_uuid_create_v4(char *uuid);

/** @} IronBeeUtilUUID */


#ifdef __cplusplus
}
#endif

#endif /* _IB_UUID_H_ */
