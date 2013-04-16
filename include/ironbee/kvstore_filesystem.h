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

#ifndef __IRONBEE__KVSTORE_FILESYSTEM_H
#define __IRONBEE__KVSTORE_FILESYSTEM_H

#include <ironbee/kvstore.h>
#include <ironbee/types.h>

#include <sys/stat.h>
#include <sys/types.h>

/**
 * @file
 * @brief IronBee --- Key-Value Filesystem Store Interface
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

/**
 * @addtogroup IronBeeKeyValueStore
 * @ingroup IronBeeUtil
 * @{
 */

/**
 * The filesystem server object.
 */
struct ib_kvstore_filesystem_server_t {
    const char *directory; /**< The directory in which files are written. */
    size_t directory_length; /**< Cache the string length of the directory. */
    mode_t fmode; /**< The mode of created files. */
    mode_t dmode; /**< The mode of created directories. */
};
typedef struct ib_kvstore_filesystem_server_t ib_kvstore_filesystem_server_t;

/**
 * Initializes kvstore that writes to a filesystem.
 *
 * @param[out] kvstore Initialized with kvserver and some defaults.
 * @param[in] directory The directory we will store this data in.
 * @returns
 *   - IB_OK on success
 *   - IB_EALLOC on memory allocation failure using malloc.
 */
ib_status_t ib_kvstore_filesystem_init(
    ib_kvstore_t *kvstore,
    const char *directory);

 /**
  * @}
  */
#endif /* __IRONBEE__KVSTORE_FILESYSTEM_H */
