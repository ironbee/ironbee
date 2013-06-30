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
 * @brief IronBee --- LUA Module
 *
 * This module integrates with luajit, allowing lua modules to be loaded.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#ifndef __MODULES__LUA_H
#define __MODULES__LUA_H

#include "lua/ironbee.h"

#include <ironbee/core.h>
#include <ironbee/lock.h>
#include <ironbee/module.h>
#include <ironbee/release.h>
#include <ironbee/resource_pool.h>

/**
 * Module configuration.
 */
struct modlua_cfg_t {
    char               *pkg_path;      /**< Package path Lua Configuration. */
    char               *pkg_cpath;     /**< Cpath Lua Configuration. */
    ib_list_t          *reloads;       /**< modlua_reload_t list. */
    ib_resource_pool_t *lua_pool;      /**< Pool of Lua stacks. */
    ib_lock_t           lua_pool_lock; /**< Pool lock. */
    ib_resource_t      *lua_resource;  /**< Resource modlua_cfg_t::L. */
    lua_State          *L;             /**< Lua stack used for config. */
};
typedef struct modlua_cfg_t modlua_cfg_t;

ib_status_t modlua_cfg_get(
    ib_engine_t *ib,
    ib_context_t *ctx,
    modlua_cfg_t **cfg);

/**
 * Called by modlua_module_load to load the lua script into the Lua runtime.
 *
 * If @a is_config_time is true, then this will also register
 * configuration directives and handle them.
 *
 * @param[in] ib IronBee engine.
 * @param[in] is_config_time Is this executing at configuration time.
 *            If yes, then effects on the ironbee engine may be performed,
 *            such as rules and directives.
 * @param[in] file The file we are loading.
 * @param[in] module The registered module structure.
 * @param[in,out] L The lua context that @a file will be loaded into as
 *                @a module.
 * @returns
 *   - IB_OK on success.
 */
ib_status_t modlua_module_load_lua(
    ib_engine_t *ib,
    bool is_config_time,
    const char *file,
    ib_module_t *module,
    lua_State *L
);

#endif /* __MODULES__LUA_H */
