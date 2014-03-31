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
 * @brief IronBee --- LUA Runtime
 *
 * Lua runtime management code used throughout `ibmod_lua`.
 *
 * A runtime includes a little meta information and a lua_State pointer.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */
#include "ironbee_config_auto.h"
#include "lua_runtime_private.h"

#include "lua_modules_private.h"
#include "lua_private.h"

#include <ironbee/context.h>
#include <ironbee/mm_mpool_lite.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <assert.h>
#include <stdlib.h>

/* If LUA_BASE_PATH was not set as part of autoconf, define a default. */
#ifndef LUA_BASE_PATH
#define LUA_BASE_PATH /usr/local/ironbee/lib/lua
#endif

#define LUA_BASE_PATH_STR IB_XSTRINGIFY(LUA_BASE_PATH)

//! Maximum number of times a resource pool Lua stack should be used.
static size_t MAX_LUA_STACK_USES = 1000;

/**
 * Data provided to the resource pool to use in creating Lua stacks.
 */
struct modlua_runtime_cbdata_t {
    ib_engine_t *ib;     /**< The engine. */
    ib_module_t *module; /**< `ibmod_lua` structure. */
};
typedef struct modlua_runtime_cbdata_t modlua_runtime_cbdata_t;

/**
 * Given a search prefix this will build a search path and add it to Lua.
 *
 * @param[in] ib IronBee engine.
 * @param[in,out] L Lua State that the search path will be set in.
 * @param[in] prefix The prefix. `?.lua` will be appended.
 *
 * @returns
 *   - IB_OK on success
 *   - IB_EALLOC on malloc failure.
 */
static ib_status_t modlua_append_searchprefix(
    ib_engine_t *ib,
    lua_State   *L,
    const char  *prefix
)
{
    assert(ib != NULL);
    assert(L != NULL);
    assert(prefix != NULL);

    /* This is the search pattern that is appended to each element of
     * lua_search_paths and then added to the Lua runtime package.path
     * global variable. */
    static const char *lua_file_pattern = "?.lua";
    static const char *lua_cfile_pattern = "?.so";

    ib_mpool_lite_t *mp;
    ib_mm_t          mm;
    ib_status_t      rc;

    char            *path = NULL; /* Tmp string to build a search path. */
    char            *cpath = NULL; /* Tmp string to build a search path. */

    rc = ib_mpool_lite_create(&mp);
    if (rc != IB_OK) {
        return rc;
    }

    mm = ib_mm_mpool_lite(mp);

    /* Strlen + 2. One for \0 and 1 for the path separator. */
    path = ib_mm_alloc(mm, strlen(prefix) + strlen(lua_file_pattern) + 2);
    if (path == NULL) {
        rc = IB_EALLOC;
        goto cleanup;
    }

    cpath = ib_mm_alloc(mm, strlen(prefix) + strlen(lua_cfile_pattern) + 2);
    if (path == NULL) {
        rc = IB_EALLOC;
        goto cleanup;
    }

    strcpy(path, prefix);
    strcpy(path + strlen(path), "/");
    strcpy(path + strlen(path), lua_file_pattern);
    ib_lua_add_require_path(ib, L, path);

    strcpy(cpath, prefix);
    strcpy(cpath + strlen(cpath), "/");
    strcpy(cpath + strlen(cpath), lua_cfile_pattern);
    ib_lua_add_require_cpath(ib, L, cpath);

cleanup:
    ib_mpool_lite_destroy(mp);
    return rc;
}

/**
 * Set the search path in the lua state from the core config.
 *
 * @param[in] ib IronBee engine.
 * @param[in] L The Lua State to setup.
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure to retrieve configuration context or Lua eval error.
 */
static ib_status_t modlua_setup_searchpath(ib_engine_t *ib, lua_State *L)
{
    /* Null terminated list of search paths. */
    const char    *lua_search_paths[4];
    ib_status_t    rc;
    ib_core_cfg_t *corecfg = NULL;

    rc = ib_core_context_config(ib_context_main(ib), &corecfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve core module configuration.");
        return rc;
    }

    /* Initialize the search paths list. */
    lua_search_paths[0] = LUA_BASE_PATH_STR;
    lua_search_paths[1] = corecfg->module_base_path;
    lua_search_paths[2] = corecfg->rule_base_path;
    lua_search_paths[3] = NULL;

    for (const char **path = lua_search_paths; *path != NULL; ++path)
    {
        rc = modlua_append_searchprefix(ib, L, *path);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

/**
 * Pre-load files into the given lua stack.
 *
 * This will attempt to run...
 *   - waggle   = require("ironbee/waggle")
 *   - ibconfig = require("ironbee/config")
 *   - ffi      = require("ffi")
 *   - ibapi    = require("ironbee/api")
 *   - modlua   = require("ironbee/module")
 *
 * @param[in] ib IronBee engine. Used to find load paths from the
 *            core module.
 * @param[out] L The Lua state that the modules will be "required" into.
 *
 * @returns
 * - IB_OK On success.
 * - Other on Lua evaluation error.
 */
static ib_status_t modlua_preload(ib_engine_t *ib, lua_State *L)
{

    ib_status_t rc;

    const char *lua_preloads[][2] = { { "waggle", "ironbee/waggle" },
                                      { "ibconfig", "ironbee/config" },
                                      { "ffi", "ffi" },
                                      { "ibapi", "ironbee/api" },
                                      { "modlua", "ironbee/module" },
                                      { NULL, NULL } };

    for (int i = 0; lua_preloads[i][0] != NULL; ++i)
    {
        rc = ib_lua_require(ib, L, lua_preloads[i][0], lua_preloads[i][1]);
        if (rc != IB_OK)
        {
            ib_log_error(ib,
                "Failed to load mode \"%s\" into \"%s\".",
                lua_preloads[i][1],
                lua_preloads[i][0]);
            return rc;
        }
    }

    return IB_OK;
}

/**
 * Create a new Lua state.
 *
 * @param[in] ib IronBee engine.
 * @param[in] cfg Configuration of the module.
 * @param[in] Lout The Lua stack to create.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EUNKNOWN If luaL_newstate() fails.
 */
static ib_status_t modlua_newstate(
    ib_engine_t   *ib,
    modlua_cfg_t  *cfg,
    lua_State    **Lout
)
{
    lua_State   *L;
    ib_status_t  rc;

    L = luaL_newstate();
    if (L == NULL) {
        ib_log_error(ib, "Failed to initialize lua module.");
        return IB_EUNKNOWN;
    }

    luaL_openlibs(L);

    /* Inject some constants so we know we are in the IronBee Lua Module. */
    lua_pushboolean(L, 1);
    lua_setglobal(L, "IRONBEE_MODLUA");
    lua_pushstring(L, VERSION);
    lua_setglobal(L, "IRONBEE_VERSION");

    /* Setup search paths before ffi, api, etc loading. */
    rc = modlua_setup_searchpath(ib, L);
    if (rc != IB_OK) {
        return rc;
    }

    /* Load ffi, api, etc. */
    rc = modlua_preload(ib, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to pre-load Lua files.");
        return rc;
    }

    /* Set package paths if configured. */
    if (cfg->pkg_path) {
        ib_log_debug(
            ib,
            "Using lua package.path=\"%s\"",
             cfg->pkg_path);
        lua_getfield(L, -1, "path");
        lua_pushstring(L, cfg->pkg_path);
        lua_setglobal(L, "path");
    }
    if (cfg->pkg_cpath) {
        ib_log_debug(
            ib,
            "Using lua package.cpath=\"%s\"",
            cfg->pkg_cpath);
        lua_getfield(L, -1, "cpath");
        lua_pushstring(L, cfg->pkg_cpath);
        lua_setglobal(L, "cpath");
    }

    *Lout = L;

    return IB_OK;
}

/**
 * A helper function that reloads Lua rules and modules in a context.
 *
 * @param[in] ib IronBee engine.
 * @param[in] module Used to retrieve the configuration.
 * @param[in] ctx The context whose configuration for @a module to retrieve.
 * @param[in] L The stack the configuration will be loaded into.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error. A log message is generated.
 */
static ib_status_t modlua_reload_ctx(
    ib_engine_t  *ib,
    ib_module_t  *module,
    ib_context_t *ctx,
    lua_State    *L
)
{
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(L != NULL);
    assert(module != NULL);

    ib_status_t           rc;
    ib_status_t           tmp_rc = IB_OK;
    modlua_cfg_t         *cfg;
    const ib_list_node_t *node;

    rc = ib_context_module_config(ctx, module, &cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve modlua configuration.");
        return rc;
    }

    IB_LIST_LOOP_CONST(cfg->reloads, node) {
        const modlua_reload_t *reload =
            (const modlua_reload_t *)ib_list_node_data_const(node);

        switch(reload->type) {
            case MODLUA_RELOAD_MODULE:
                tmp_rc = modlua_module_load_lua(
                    ib,
                    reload->file,
                    reload->module,
                    L);
                break;
            case MODLUA_RELOAD_RULE:
                tmp_rc = ib_lua_load_func(
                    ib,
                    L,
                    reload->file,
                    reload->rule_id);
                break;
        }

        if (rc == IB_OK && tmp_rc != IB_OK) {
            ib_log_error(
                ib,
                "Failed to reload Lua rule or module \"%s\".",
                reload->file);
            rc = tmp_rc;
        }
    }

    return rc;
}

ib_status_t modlua_record_reload(
    ib_engine_t          *ib,
    modlua_cfg_t         *cfg,
    modlua_reload_type_t  type,
    ib_module_t          *module,
    const char           *rule_id,
    const char           *file
)
{
    assert(ib           != NULL);
    assert(cfg          != NULL);
    assert(cfg->reloads != NULL);
    assert(file         != NULL);

    ib_mm_t          mm;
    ib_status_t      rc;
    modlua_reload_t *data;

    mm = ib_engine_mm_config_get(ib);

    data = ib_mm_alloc(mm, sizeof(*data));
    if (data == NULL) {
        return IB_EALLOC;
    }

    /* Record type. */
    data->type = type;

    /* Record what the user gave us for a module structure. */
    data->module = module;

    /* Copy file name. */
    data->file = ib_mm_strdup(mm, file);
    if (data->file == NULL) {
        ib_log_error(ib, "Failed to copy file name \"%s\".", file);
        return IB_EALLOC;
    }

    /* Copy rule_id. */
    if (rule_id != NULL) {
        data->rule_id = ib_mm_strdup(mm, rule_id);
        if (data->rule_id == NULL) {
            ib_log_error(ib, "Failed to copy rule_id \"%s\".", rule_id);
            return IB_EALLOC;
        }
    }

    rc = ib_list_push(cfg->reloads, data);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t modlua_reload_ctx_main(
    ib_engine_t *ib,
    ib_module_t *module,
    lua_State   *L
)
{
    assert(ib     != NULL);
    assert(module != NULL);
    assert(L      != NULL);

    ib_status_t   rc;
    ib_context_t *ctx = ib_context_main(ib);

    /* Reload this context. */
    rc = modlua_reload_ctx(ib, module, ctx, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to load main context into Lua stack.");
        return rc;
    }

    return IB_OK;
}

/**
 * Reload all the contexts except the main context.
 */
ib_status_t modlua_reload_ctx_except_main(
    ib_engine_t *ib,
    ib_module_t  *module,
    ib_context_t *ctx,
    lua_State    *L
)
{
    assert(ib     != NULL);
    assert(module != NULL);
    assert(ctx    != NULL);
    assert(L      != NULL);

    ib_status_t rc;

    /* Do not reload the main context. */
    if (ctx == ib_context_main(ib)) {
        return IB_OK;
    }

    /* Reload the parent context first. */
    rc = modlua_reload_ctx_except_main(
        ib,
        module,
        ib_context_parent_get(ctx),
        L);
    if (rc != IB_OK) {
        return rc;
    }

    /* Reload this context. */
    rc = modlua_reload_ctx(ib, module, ctx, L);
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Failed to load context \"%s\" into Lua stack.",
            ib_context_name_get(ctx));
        return rc;
    }

    return IB_OK;
}

/**
 * Resource pool callback to create a new Lua runtime.
 *
 * @param[out] resource The created @ref modlua_runtime_t.
 * @param[in] cbdata Callback data. @ref modlua_runtime_cbdata_t.
 *
 * @returns
 * - IB_OK On Success.
 * - Other on failure.
 */
static ib_status_t lua_pool_create_fn(void *resource, void *cbdata)
{
    assert(resource != NULL);
    assert(cbdata != NULL);

    modlua_runtime_t *modlua_runtime;
    ib_mpool_lite_t  *mp;
    ib_status_t       rc;
    modlua_cfg_t     *cfg;
    ib_context_t     *ctx;
    ib_engine_t      *ib;
    ib_module_t      *module;
    modlua_runtime_cbdata_t *modlua_runtime_cbdata =
        (modlua_runtime_cbdata_t *)cbdata;

    assert(modlua_runtime_cbdata->ib     != NULL);
    assert(modlua_runtime_cbdata->module != NULL);

    /* Alias some callback values. */
    ib     = modlua_runtime_cbdata->ib;
    module = modlua_runtime_cbdata->module;

    rc = ib_mpool_lite_create(&mp);
    if (rc != IB_OK) {
        return rc;
    }

    modlua_runtime =
        ib_mm_calloc(ib_mm_mpool_lite(mp), 1, sizeof(*modlua_runtime));
    if (modlua_runtime == NULL) {
        return IB_EALLOC;
    }

    ctx = ib_context_main(ib);

    rc = modlua_cfg_get(ib, ctx, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    modlua_runtime->use_count = 0;
    modlua_runtime->mp        = mp;

    /* Create a new Lua State. */
    rc = modlua_newstate(ib, cfg, &(modlua_runtime->L));
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create Lua stack.");
        return rc;
    }

    /* Preload the user's main context. */
    rc = modlua_reload_ctx_main(ib, module, modlua_runtime->L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to configure Lua stack.");
        return rc;
    }

    *(void **)resource = modlua_runtime;

    return IB_OK;
}

/**
 * A resource pool callback to destroy a @ref modlua_runtime_t.
 *
 * @param[out] resource The @ref modlua_runtime_t to destroy.
 * @param[in] cbdata Unused.
 */
static void lua_pool_destroy_fn(void *resource, void *cbdata)
{
    assert(resource != NULL);

    modlua_runtime_t *modlua_runtime = (modlua_runtime_t *)resource;

    lua_close(modlua_runtime->L);

    ib_mpool_lite_destroy(modlua_runtime->mp);
}

/**
 * Increment modlua_runtime_t::uses when fetched from a resource pool.
 *
 * @param[out] resource The @ref modlua_runtime_t being fetched.
 * @param[in] cbdata Unused.
 */
static void lua_pool_preuse_fn(void *resource, void *cbdata)
{
    assert(resource != NULL);

    modlua_runtime_t *modlua_runtime = (modlua_runtime_t *)resource;

    ++(modlua_runtime->use_count);
}

/**
 * Returns @ref IB_EINVAL when modlua_runtime_t should be destroyed.
 *
 * Currently the only condition that triggers destruction is
 * modlua_runtime_t::uses exceeding @ref MAX_LUA_STACK_USES.
 *
 * @param[in] resource The @ref modlua_runtime_t to check.
 * @param[in] cbdata Unused.
 *
 * @returns
 * - IB_OK When no action is required. The @a resource is still valid.
 * - IB_EINVAL When @a resource should be destroyed.
 */
static ib_status_t lua_pool_postuse_fn(void *resource, void *cbdata)
{
    assert(resource != NULL);

    modlua_runtime_t *modlua_runtime = (modlua_runtime_t *)resource;

    /* Signal stack destruction if it was used some number of times. */
    return (modlua_runtime->use_count > MAX_LUA_STACK_USES)? IB_EINVAL : IB_OK;
}


ib_status_t modlua_runtime_resource_pool_create(
    ib_resource_pool_t **resource_pool,
    ib_engine_t         *ib,
    ib_module_t         *module,
    ib_mm_t              mm
)
{

    assert(resource_pool != NULL);
    assert(ib != NULL);
    assert(module != NULL);

    modlua_runtime_cbdata_t *modlua_runtime_cbdata;

    modlua_runtime_cbdata =
        ib_mm_calloc(mm, 1, sizeof(*modlua_runtime_cbdata));

    if (modlua_runtime_cbdata == NULL) {
        return IB_EALLOC;
    }

    modlua_runtime_cbdata->ib = ib;
    modlua_runtime_cbdata->module = module;

    return ib_resource_pool_create(
        resource_pool,         /* Out variable. */
        mm,                    /* Memory manager. */
        10,                    /* Minimum 10 Lua stacks in reserve. */
        0,                     /* No maximum limit. */
        lua_pool_create_fn,    /* Create function. */
        modlua_runtime_cbdata, /* Callback data is just the active engine. */
        lua_pool_destroy_fn,   /* Destroy function. Calls lua_close(). */
        modlua_runtime_cbdata, /* Callback data is just the active engine. */
        lua_pool_preuse_fn,    /* Pre use function. Increments use count. */
        modlua_runtime_cbdata, /* Callback data is just the active engine. */
        lua_pool_postuse_fn,   /* Post use function. Signals delete. */
        modlua_runtime_cbdata  /* Callback data is just the active engine. */
    );
}

ib_status_t modlua_releasestate(
    ib_engine_t      *ib,
    modlua_cfg_t     *cfg,
    modlua_runtime_t *modlua_runtime
)
{
    assert(ib != NULL);
    assert(cfg != NULL);

    ib_status_t rc;

    rc = ib_lock_lock(&(cfg->lua_pool_lock));
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_resource_release(modlua_runtime->resource);
    if (rc != IB_OK) {
        ib_lock_unlock(&(cfg->lua_pool_lock));
        return rc;
    }

    rc = ib_lock_unlock(&(cfg->lua_pool_lock));
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t modlua_acquirestate(
    ib_engine_t       *ib,
    modlua_cfg_t      *cfg,
    modlua_runtime_t **modlua_runtime
)
{
    assert(ib != NULL);
    assert(cfg != NULL);

    ib_status_t    rc;
    ib_resource_t *resource;

    rc = ib_lock_lock(&(cfg->lua_pool_lock));
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_resource_acquire(cfg->lua_pool, &resource);
    if (rc != IB_OK) {
        ib_lock_unlock(&(cfg->lua_pool_lock));
        return rc;
    }

    rc = ib_lock_unlock(&(cfg->lua_pool_lock));
    if (rc != IB_OK) {
        return rc;
    }

    *modlua_runtime = (modlua_runtime_t *)ib_resource_get(resource);

    (*modlua_runtime)->resource = resource;

    return IB_OK;
}
