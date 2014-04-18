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

#ifndef _IB_STRING_LOWER_H_
#define _IB_STRING_LOWER_H_

/**
 * @file
 * @brief IronBee --- String Lowercase.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/mm.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup IronBeeUtilString
 *
 * @{
 */

/**
 * Simple ASCII lowercase function.
 *
 * @param[in] mm Memory manager to allocate @a out from.
 * @param[in] in Input to convert to lowercase.
 * @param[in] in_len Length of @a in.
 * @param[out] out Lower case version of @a in.
 *
 * @returns Status code.
 */
ib_status_t ib_strlower(
    ib_mm_t         mm,
    const uint8_t  *in,
    size_t          in_len,
    uint8_t       **out
)
NONNULL_ATTRIBUTE(2, 4);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_STRING_LOWER_H_ */
