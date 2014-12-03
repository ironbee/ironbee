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

#ifndef _IB_MM_MPOOL_FREEABLE_H_
#define _IB_MM_MPOOL_FREEABLE_H_

/**
 * @file
 * @brief IronBee --- Memory Manager: Mpool Freeable Adapter
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/mm.h>
#include <ironbee/mpool_freeable.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup IronBeeUtilMM
 * @{
 */

/**
 * Create an @ref ib_mm_t from an @ref ib_mpool_freeable_t.
 *
 * @param[in] mpf Lite memory pool to provide interface to.
 * @return Memory manager interface to @a mp.
 **/
ib_mm_t DLL_PUBLIC ib_mm_mpool_freeable(ib_mpool_freeable_t *mpf)
NONNULL_ATTRIBUTE(1);

/** @} IronBeeUtilMM */

#ifdef __cplusplus
}
#endif

#endif /* _IB_MM_MPOOL_FREEABLE_H_ */
