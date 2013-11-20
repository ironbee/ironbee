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
 * @brief IronBee --- Lua Runtime
 *
 * A structure that represents ibmod_lua's concept of a Lua runtime.
 *
 * A runtime includes a little meta information and a lua_State pointer.
 *
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */
#ifndef __MODULES__LUA_RUNTIME_H
#define __MODULES__LUA_RUNTIME_H

#include "lua/ironbee.h"
#include "lua_common_private.h"
#include "lua_private.h"

#include <ironbee/resource_pool.h>

/**
 * Per-connection module data containing a Lua runtime.
 *
 * Created for each connection and stored as the module's connection data.
 */
struct modlua_runtime_t {
    lua_State     *L;        /**< Lua stack */
    size_t         uses;     /**< Number of times this stack is used. */
    ib_mpool_t    *mp;       /**< Memory pool for this runtime. */
    ib_resource_t *resource; /**< Bookkeeping for modlua_releasestate(). */
};
typedef struct modlua_runtime_t modlua_runtime_t;

enum modlua_reload_type_t {
    MODLUA_RELOAD_RULE,
    MODLUA_RELOAD_MODULE
};
typedef enum modlua_reload_type_t modlua_reload_type_t;

/**
 * This represents a Lua item that must be reloaded.
 *
 * Reloading happens when a new Lua stack is created for the
 * resource pool (created by modlua_runtime_resource_pool_create())
 * and when a site-specific Lua file must be loaded.
 *
 * To maximize performance all Lua scripts should be put in in the main
 * context and as few as possible should be put in site contexts.
 */
struct modlua_reload_t {
    modlua_reload_type_t  type;    /**< Is this a module or a rule? */
    ib_module_t          *module;  /**< Lua module (not ibmod_lua.so). */
    const char           *file;    /**< File of the rule or module code. */
    const char           *rule_id; /**< Rule if this is a rule type. */
};
typedef struct modlua_reload_t modlua_reload_t;

/**
 * Get the lua runtime from the connection.
 *
 * @param[in] conn Connection
 * @param[in] module The IronBee module structure.
 * @param[out] lua Lua runtime struct. *lua must be NULL.
 *
 * @returns
 *   - IB_OK on success.
 *   - Result of ib_engine_module_get() on error.
 */
ib_status_t modlua_runtime_get(
    ib_conn_t         *conn,
    ib_module_t       *module,
    modlua_runtime_t **lua
);

/**
 * Set the lua runtime for the connection.
 *
 * @param[in] conn Connection
 * @param[in] module The IronBee module structure.
 * @param[in] lua Lua runtime struct.
 *
 * @returns
 *   - IB_OK on success.
 *   - Result of ib_engine_module_get() on error.
 */
ib_status_t modlua_runtime_set(
    ib_conn_t        *conn,
    ib_module_t      *module,
    modlua_runtime_t *lua
);

/**
 * Clear the runtime set by modlua_runtime_set().
 *
 * @param[in] conn Connection to clear the stored value from.
 * @param[in] module The IronBee module structure.
 *
 */
ib_status_t modlua_runtime_clear(
    ib_conn_t   *conn,
    ib_module_t *module
);

/**
 * Create a resource pool that manages @ref modlua_runtime_t instances.
 *
 *  @param[out] resource_pool Resource pool to create.
 *  @param[in] ib The IronBee engine made available to the Lua runtime.
 *  @param[in] module The IronBee module structure.
 *  @param[in] mp The memory pool the resource pool will use.
 *
 *  @returns
 *  - IB_OK On success
 *  - IB_EALLOC If callback data structure cannot be allocated out of @a mp.
 *  - Failures codes ib_resource_pool_create().
 */
ib_status_t modlua_runtime_resource_pool_create(
    ib_resource_pool_t **resource_pool,
    ib_engine_t         *ib,
    ib_module_t         *module,
    ib_mpool_t          *mp
);

/**
 * Reload @a ctx and all parent contexts except the main context.
 *
 * When a Lua stack is given from the resource pool to a connection,
 * it is assumed that the stack has all the files referenced in the main
 * context already loaded. All site-specific scripts must be reloaded.
 *
 * @param[in] ib The IronBee engine.
 * @param[in] module The module object for the Lua module.
 * @param[in] ctx The configuration context. If this is the main context,
 *            this function does nothing. If this is a context other than
 *            the main context it's parent is recursively passed
 *            to this function until @a ctx is the main context.
 * @param[in] L The Lua State to reload Lua scripts into.
 *
 * @returns
 * - IB_OK On success or if @a ctx is the main context.
 * - IB_EALLOC On an allocation error.
 * - IB_EINVAL If the Lua script fails to load.
 */
ib_status_t modlua_reload_ctx_except_main(
    ib_engine_t *ib,
    ib_module_t  *module,
    ib_context_t *ctx,
    lua_State    *L
);

/**
 * Reload the main context Lua files.
 *
 * @param[in] ib IronBee engine.
 * @param[in] module The Lua module structure.
 * @param[in] L The Lua state to reload the files into.
 *
 * @returns
 * - IB_OK
 * - Failures of modlua_reload_ctx().
 */
ib_status_t modlua_reload_ctx_main(
    ib_engine_t *ib,
    ib_module_t *module,
    lua_State   *L
);

/**
 * Push the file and the type into the reload list.
 *
 * This list is used to reload modules and rules into independent lua stacks
 * per transaction.
 *
 * @param[in] ib IronBee engine.
 * @param[in] cfg Configuration.
 * @param[in] type The type of the thing to reload.
 * @param[in] module For MODLUA_RELOAD_MODULE types this is a pointer
 *            to the Lua script's module structure.
 * @param[in] rule_id The rule id. This is copied.
 * @param[in] file Where is the Lua file to load. This is copied.
 */
ib_status_t modlua_record_reload(
    ib_engine_t          *ib,
    modlua_cfg_t         *cfg,
    modlua_reload_type_t  type,
    ib_module_t          *module,
    const char           *rule_id,
    const char           *file
);

ib_status_t modlua_releasestate(
    ib_engine_t *ib,
    modlua_cfg_t *cfg,
    modlua_runtime_t *runtime
);

ib_status_t modlua_acquirestate(
    ib_engine_t *ib,
    modlua_cfg_t *cfg,
    modlua_runtime_t **rt
);

#endif /* __MODULES__LUA_RUNTIME_H */
