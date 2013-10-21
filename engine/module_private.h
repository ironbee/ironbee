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

#ifndef _IB_MODULE_PRIVATE_H_
#define _IB_MODULE_PRIVATE_H_

/**
 * @file
 * @brief IronBee --- Module Private Routines
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <ironbee/module.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeModulePrivate Module Private
 * @ingroup IronBeeModule
 *
 * Private module routines.
 *
 * @{
 */

/**
 * Unload an engine module.
 *
 * Ensures module finalizer, if defined, is called.  Otherwise, does nothing.
 *
 * @param[in] m Module
 */
void DLL_PUBLIC ib_module_unload(
     ib_module_t *m
);

/**
 * Register a module with a configuration context.
 *
 * @param[in] m Module
 * @param[in] ctx Configuration context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_module_register_context(
     ib_module_t  *m,
     ib_context_t *ctx
);
/**
 * @} IronBeeModulePrivate
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_MODULE_PRIVATE_H_ */
