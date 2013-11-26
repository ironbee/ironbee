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
 * @brief IronBee --- Lua Modules
 *
 * IronBee Modules as Lua scripts.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */
#ifndef __MODULES__LUA_MODULES_PRIVATE_H
#define __MODULES__LUA_MODULES_PRIVATE_H

#include "lua/ironbee.h"
#include "lua_common_private.h"
#include "lua_private.h"

#include <ironbee/engine.h>
#include <ironbee/types.h>

/**
 * Load a Lua module from a .lua file.
 *
 * @param[in] ib IronBee engine.
 * @param[in] modlua The Lua module. Not a Lua-implemented module.
 * @param[in] file The Lua file to load.
 * @param[in] cfg The configuration of the lua module.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL On a Lua language error.
 * - IB_EALLOC On a memory error.
 * - Other when interacting with registering the modules with @a ib.
 */
ib_status_t modlua_module_load(
    ib_engine_t  *ib,
    ib_module_t  *modlua,
    const char   *file,
    modlua_cfg_t *cfg
);

#endif /* __MODULES__LUA_MODULES_PRIVATE_H */
