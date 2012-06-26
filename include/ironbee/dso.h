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

#ifndef _IB_DSO_H_
#define _IB_DSO_H_

/**
 * @file
 * @brief IronBee &mdash; DSO Utility Functions
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/types.h>
#include <ironbee/mpool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeUtilDso Dynamic Shared Object (DSO)
 * @ingroup IronBeeUtil
 *
 * Code to load and interact with DSOs.  Used for module loading.
 *
 * @{
 */


/**
 * Open a dynamic shared object (DSO) from a file.
 *
 * @param pdso DSO handle is stored in *dso
 * @param file DSO filename
 * @param pool Memory pool to use
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_dso_open(ib_dso_t **pdso,
                                   const char *file,
                                   ib_mpool_t *pool);


/**
 * Close a dynamic shared object (DSO).
 *
 * @param dso DSO handle is stored in *dso
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_dso_close(ib_dso_t *dso);


/**
 * Find a given symbol in a dynamic shared object (DSO).
 *
 * @param dso DSO handle
 * @param name DSO symbol name
 * @param psym DSO symbol handle is stored in *sym
 *
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_dso_sym_find(ib_dso_t *dso,
                                       const char *name,
                                       ib_dso_sym_t **psym);

/** @} IronBeeUtilDso */

#ifdef __cplusplus
}
#endif

#endif /* _IB_DSO_H_ */
