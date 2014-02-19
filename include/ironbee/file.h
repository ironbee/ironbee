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

#ifndef _UTIL__FILE_H_
#define _UTIL__FILE_H_

/**
 * @file
 * @brief IronBee --- Utility Functions
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/mm.h>
#include <ironbee/types.h>

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeFile File Utilities
 * @ingroup IronBeeUtil
 *
 * Functions to manipulate files.
 *
 * @{
 */

/**
 * Read an entire file.
 *
 * @param[in] mm Memory manager.
 * @param[in] file Path to the file.
 * @param[out] out The resultant buffer of bytes.
 * @param[out] sz The size of the buffer.
 *
 * @return
 * - IB_OK On success.
 * - IB_EINVAL Error stating the file. Errno is set.
 * - IB_EOTHER Read error. Errno is set.
 * - IB_EALLOC On allocation error.
 */
ib_status_t ib_file_readall(
    ib_mm_t         mm,
    const char     *file,
    const uint8_t **out,
    size_t         *sz
);

/**
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif /* _UTIL__FILE_H_ */
