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

#include "lua/ironbee.h"
#include "lua_common_private.h"

#include <ironbee/array.h>
#include <ironbee/cfgmap.h>
#include <ironbee/core.h>
#include <ironbee/engine.h>
#include <ironbee/escape.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>
#include <ironbee/lock.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/provider.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 *  * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* -- Module Setup -- */

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        lua
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Define the public module symbol. */
IB_MODULE_DECLARE();

typedef struct modlua_runtime_t modlua_runtime_t;
typedef struct modlua_cfg_t modlua_cfg_t;
typedef struct modlua_lua_cbdata_t modlua_lua_cbdata_t;

/**
 * Callback type for functions executed protected by global lock.
 *
 * This callback should take a @c ib_engine_t* which is used
 * for logging, a @c lua_State* which is used to create the
 * new thread, and a @c lua_State** which will be assigned a
 * new @c lua_State*.
 */
typedef ib_status_t(*critical_section_fn_t)(ib_engine_t *ib,
                                            lua_State *parent,
                                            lua_State **out_new);
/**
 * Per-connection module data containing a Lua runtime.
 *
 * Created for each connection and stored as the module's connection data.
 */
struct modlua_runtime_t {
    lua_State          *L;            /**< Lua stack */
};

/**
 * Callback data passed to Lua module C dispatch functions.
 *
 * All Lua module event handlers are called by a C function, which we here
 * refer to as the dispatcher. The dispatcher receives this struct
 * as its callback data. From it, and other sources, the dispatcher
 * will construct a single table that is passed to the Lua
 * event handler, and will then call that Lua event handler.
 */
struct modlua_lua_cbdata_t {
    ib_module_t *module; /**< The module object for this Lua module. */
};

/**
 * Global module configuration.
 */
struct modlua_cfg_t {
    char               *pkg_path;  /**< Package path Lua Configuration. */
    char               *pkg_cpath; /**< Cpath Lua Configuration. */
    lua_State          *L;         /**< Lua runtime stack. */
    ib_lock_t          *L_lck;     /**< Lua runtime stack lock. */
};

/* Instantiate a module global configuration. */
static modlua_cfg_t modlua_global_cfg = {
    NULL, /* pkg_path */
    NULL, /* pkg_cpath */
    NULL,
    NULL
};

ib_status_t modlua_rule_driver(
    ib_cfgparser_t *cp,
    ib_rule_t *rule,
    const char *tag,
    const char *location,
    void *cbdata
);

/* -- Lua Routines -- */

#define IB_FFI_MODULE  ironbee-ffi
#define IB_FFI_MODULE_STR IB_XSTRINGIFY(IB_FFI_MODULE)

#define IB_FFI_MODULE_WRAPPER     _IRONBEE_CALL_MODULE_HANDLER
#define IB_FFI_MODULE_WRAPPER_STR IB_XSTRINGIFY(IB_FFI_MODULE_WRAPPER)

#define IB_FFI_MODULE_CFG_WRAPPER     _IRONBEE_CALL_CONFIG_HANDLER
#define IB_FFI_MODULE_CFG_WRAPPER_STR IB_XSTRINGIFY(IB_FFI_MODULE_CFG_WRAPPER)

#define IB_FFI_MODULE_EVENT_WRAPPER     _IRONBEE_CALL_EVENT_HANDLER
#define IB_FFI_MODULE_EVENT_WRAPPER_STR IB_XSTRINGIFY(IB_FFI_MODULE_EVENT_WRAPPER)

/**
 * Get the lua runtime from the connection.
 *
 * @param[in] conn Connection
 * @param[out] lua Lua runtime struct.
 *
 * @returns
 *   - IB_OK on success.
 *   - Result of ib_engine_module_get() on error.
 */
static ib_status_t modlua_runtime_get(
    ib_conn_t *conn,
    modlua_runtime_t **lua)
{
    assert(conn);
    assert(conn->ib);
    assert(lua);

    ib_status_t rc;
    ib_module_t *module;

    rc = ib_engine_module_get(conn->ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        return rc;
    }

    ib_conn_get_module_data(conn, module, (void **)lua);

    return IB_OK;
}

/**
 * Set the lua runtime from the connection.
 *
 * @param[in] conn Connection
 * @param[in] lua Lua runtime struct.
 *
 * @returns
 *   - IB_OK on success.
 *   - Result of ib_engine_module_get() on error.
 */
static ib_status_t modlua_runtime_set(
    ib_conn_t *conn,
    modlua_runtime_t *lua)
{
    assert(conn);
    assert(conn->ib);
    assert(lua);

    ib_status_t rc;
    ib_module_t *module;

    rc = ib_engine_module_get(conn->ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        return rc;
    }

    ib_conn_set_module_data(conn, module, lua);

    return IB_OK;
}

/**
 * This will use module lock to atomically call @a fn.
 *
 * The argument @a fn will be either
 * ib_lua_new_thread() or ib_lua_join_thread() which will be called
 * only if module lock can be locked using @c semop.
 *
 * @param[in] ib IronBee context. Used for logging.
 * @param[in] fn The function to execute. This is passed @a ib and @a fn.
 * @param[in,out] L The Lua State to create or destroy. Passed to @a fn.
 * @returns If any error locking or unlocking the
 *          semaphore is encountered, the error code is returned.
 *          Otherwise the result of @a fn is returned.
 */
static ib_status_t call_in_critical_section(ib_engine_t *ib,
                                            critical_section_fn_t fn,
                                            lua_State **L)
{
    assert(ib);
    assert(fn);
    assert(L);

    /* Return code from IronBee calls. */
    ib_status_t ib_rc;
    /* Return code form critical call. */
    ib_status_t critical_rc;

    ib_rc  = ib_lock_lock(modlua_global_cfg.L_lck);
    /* Report semop error and return. */
    if (ib_rc != IB_OK) {
        ib_log_error(ib, "Failed to lock Lua context.");
        return ib_rc;
    }

    /* Execute lua call in critical section. */
    critical_rc = fn(ib, modlua_global_cfg.L, L);

    ib_rc = ib_lock_unlock(modlua_global_cfg.L_lck);

    if (critical_rc != IB_OK) {
        ib_log_error(ib, "Critical call failed: %s",
                     ib_status_to_string(critical_rc));
    }

    /* Report semop error and return. */
    if (ib_rc != IB_OK) {
        ib_log_error(ib, "Failed to unlock Lua context.");
        return ib_rc;
    }

    return critical_rc;
}

/**
 * Create a near-empty module structure.
 *
 * @param[in,out] ib Engine with which to initialize and register
 *                a module structure with no callbacks. Callbacks
 *                are dynamically assigned after the lua file is
 *                evaluated and loaded.
 * @param[in] file The file name. This is used as an identifier.
 * @param[out] module The module that will be allocated with
 *             ib_module_create.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC on a memory error.
 *   - Any error from the ib_engine registration process.
 */
static ib_status_t build_near_empty_module(
    ib_engine_t *ib,
    const char *file,
    ib_module_t **module)
{
    assert(ib);
    assert(file);
    assert(module);

    ib_status_t rc;
    ib_mpool_t *mp;

    mp = ib_engine_pool_main_get(ib);

    const char *module_name = ib_mpool_strdup(mp, file);

    /* Create the Lua module as if it was a normal module. */
    ib_log_debug3(ib, "Creating lua module structure");
    rc = ib_module_create(module, ib);
    if (rc != IB_OK) {
        return rc;
    }

    /* Initialize the loaded module. */
    ib_log_debug3(ib, "Init lua module structure");
    IB_MODULE_INIT_DYNAMIC(
        *module,                        /* Module */
        file,                           /* Module code filename */
        NULL,                           /* Module data */
        ib,                             /* Engine */
        module_name,                    /* Module name */
        NULL,                           /* Global config data */
        0,                              /* Global config data length */
        NULL,                           /* Config copier */
        NULL,                           /* Config copier data */
        NULL,                           /* Configuration field map */
        NULL,                           /* Config directive map */
        NULL,                           /* Initialize function */
        NULL,                           /* Callback data */
        NULL,                           /* Finish function */
        NULL,                           /* Callback data */
        NULL,                           /* Context open function */
        NULL,                           /* Callback data */
        NULL,                           /* Context close function */
        NULL,                           /* Callback data */
        NULL,                           /* Context destroy function */
        NULL                            /* Callback data */
    );

    /* Initialize and register the new lua module with the engine. */
    ib_log_debug3(ib, "Init lua module");
    rc = ib_module_init(*module, ib);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to initialize / register a lua module.");
        return rc;
    }

    ib_log_debug3(ib, "Empty lua module created.");

    return rc;
}

/**
 * Evaluate the Lua stack and report errors about directive processing.
 */
static ib_status_t modlua_config_cb_eval(
    lua_State *L,
    ib_engine_t *ib,
    ib_module_t *module,
    const char *name,
    int args_in)
{
    int lua_rc = lua_pcall(L, args_in, 1, 0);
    ib_status_t rc;
    switch(lua_rc) {
        case 0:
            /* NOP */
            break;
        case LUA_ERRRUN:
            ib_log_error(
                ib,
                "Error processing directive for module %s: %s",
                module->name,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            return IB_EINVAL;
        case LUA_ERRMEM:
            ib_log_error(
                ib,
                "Failed to allocate memory processing directive for %s",
                module->name);
            return IB_EINVAL;
        case LUA_ERRERR:
            ib_log_error(
                ib,
                "Error fetching error message during directive for %s",
                module->name);
            return IB_EINVAL;
#if LUA_VERSION_NUM > 501
        /* If LUA_ERRGCMM is defined, include a custom error for it as well.
          This was introduced in Lua 5.2. */
        case LUA_ERRGCMM:
            ib_log_error(
                ib,
                "Garbage collection error during directive for %s.",
                module->name);
            return IB_EINVAL;
#endif
        default:
            ib_log_error(
                ib,
                "Unexpected error(%d) during directive %s for %s: %s",
                lua_rc,
                name,
                module->name,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            return IB_EINVAL;
    }

    if (!lua_isnumber(L, -1)) {
        ib_log_error(ib, "Directive handler did not return integer.");
        rc = IB_EINVAL;
    }
    else {
        rc = lua_tonumber(L, -1);
    }

    lua_pop(L, 1);

    return rc;
}

/**
 * Push a Lua table onto the stack that contains a path of configurations.
 *
 * IronBee supports nested configuration contexts. Configuration B may
 * occur in configuration A. This function will push
 * the Lua table { "A", "B" } such that t[1] = "A" and t[2] = "B".
 *
 * This allows the module to fetch or build the configuration table
 * required to store any user configurations to be done.
 *
 * Lazy configuration table creation is done to avoid a large, unused
 * memory footprint in situations of simple global Lua module configurations
 * but hundreds of sites, each of which has no unique configuration.
 *
 * This is also used when finding the closest not-null configuration to
 * pass to a directive handler.
 *
 * @param[in] ib IronBee Engine.
 * @param[in] ctx The current / starting context.
 * @param[in,out] L Lua stack onto which is pushed the table of configurations.
 */
static ib_status_t modlua_push_config_path(
    ib_engine_t *ib,
    ib_context_t *ctx,
    lua_State *L)
{
    assert(ib);
    assert(ctx);
    assert(L);

    int table;

    lua_createtable(L, 10, 0); /* Make a list to store our context names in. */
    table = lua_gettop(L);     /* Store where in the stack it is. */

    /* Until the main context is found, push the name of this ctx and fetch. */
    while (ctx != ib_context_main(ib)) {
        lua_pushstring(L, ib_context_name_get(ctx));
        ctx = ib_context_parent_get(ctx);
    }

    /* Push the main context's name. */
    lua_pushstring(L, ib_context_name_get(ctx));

    /* While there is a string on the stack, append it to the table. */
    for (int i = 1; lua_isstring(L, -1); ++i) {
        lua_pushinteger(L, i);  /* Insert k. */
        lua_insert(L, -2);      /* Make the stack [table, ..., k, v]. */
        lua_settable(L, table); /* Set t[k] = v. */
    }

    return IB_OK;
}

/**
 * @param[in] cp Configuration parser.
 * @param[in] name Directive name for the block that is being closed.
 * @param[in,out] cbdata Callback data.
 */
static ib_status_t modlua_config_cb_blkend(
    ib_cfgparser_t *cp,
    const char *name,
    void *cbdata)
{
    assert(cp);
    assert(name);
    assert(cbdata);

    ib_status_t rc;
    modlua_lua_cbdata_t *modlua_lua_cbdata = (modlua_lua_cbdata_t *)cbdata;
    lua_State *L = modlua_global_cfg.L;
    ib_module_t *module = modlua_lua_cbdata->module;
    ib_engine_t *ib = module->ib;
    ib_context_t *ctx;

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_blkend");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);

    rc = modlua_config_cb_eval(L, ib, module, name, 4);
    return rc;
}

/**
 * @param[in] cp Configuration parser.
 * @param[in] name Configuration directive name.
 * @param[in] onoff On or off setting.
 * @param[in] cbdata Callback data.
 */
static ib_status_t modlua_config_cb_onoff(
    ib_cfgparser_t *cp,
    const char *name,
    int onoff,
    void *cbdata)
{
    assert(cp);
    assert(name);
    assert(cbdata);

    ib_status_t rc;
    modlua_lua_cbdata_t *modlua_lua_cbdata = (modlua_lua_cbdata_t *)cbdata;
    lua_State *L = modlua_global_cfg.L;
    ib_module_t *module = modlua_lua_cbdata->module;
    ib_engine_t *ib = module->ib;
    ib_context_t *ctx;

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_onoff");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);
    lua_pushinteger(L, onoff);

    rc = modlua_config_cb_eval(L, ib, module, name, 5);
    return rc;
}
/**
 * @param[in] cp Configuration parser.
 * @param[in] name Configuration directive name.
 * @param[in] p1 The only parameter.
 * @param[in] cbdata Callback data.
 */
static ib_status_t modlua_config_cb_param1(
    ib_cfgparser_t *cp,
    const char *name,
    const char *p1,
    void *cbdata)
{
    assert(cp);
    assert(name);
    assert(p1);
    assert(cbdata);

    ib_status_t rc;
    modlua_lua_cbdata_t *modlua_lua_cbdata = (modlua_lua_cbdata_t *)cbdata;
    lua_State *L = modlua_global_cfg.L;
    ib_module_t *module = modlua_lua_cbdata->module;
    ib_engine_t *ib = module->ib;
    ib_context_t *ctx;

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_param1");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);
    lua_pushstring(L, p1);

    rc = modlua_config_cb_eval(L, ib, module, name, 5);
    return rc;
}
/**
 * @param[in] cp Configuration parser.
 * @param[in] name Configuration directive name.
 * @param[in] p1 The first parameter.
 * @param[in] p2 The second parameter.
 * @param[in] cbdata Callback data.
 */
static ib_status_t modlua_config_cb_param2(
    ib_cfgparser_t *cp,
    const char *name,
    const char *p1,
    const char *p2,
    void *cbdata)
{
    assert(cp);
    assert(name);
    assert(p1);
    assert(p2);
    assert(cbdata);

    ib_status_t rc;
    modlua_lua_cbdata_t *modlua_lua_cbdata = (modlua_lua_cbdata_t *)cbdata;
    lua_State *L = modlua_global_cfg.L;
    ib_module_t *module = modlua_lua_cbdata->module;
    ib_engine_t *ib = module->ib;
    ib_context_t *ctx;

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_param2");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);
    lua_pushstring(L, p1);
    lua_pushstring(L, p2);

    rc = modlua_config_cb_eval(L, ib, module, name, 6);
    return rc;
}
/**
 * @param[in] cp Configuration parser.
 * @param[in] name Configuration directive name.
 * @param[in] list List of values.
 * @param[in] cbdata Callback data.
 */
static ib_status_t modlua_config_cb_list(
    ib_cfgparser_t *cp,
    const char *name,
    const ib_list_t *list,
    void *cbdata)
{
    assert(cp);
    assert(name);
    assert(list);
    assert(cbdata);

    ib_status_t rc;
    modlua_lua_cbdata_t *modlua_lua_cbdata = (modlua_lua_cbdata_t *)cbdata;
    lua_State *L = modlua_global_cfg.L;
    ib_module_t *module = modlua_lua_cbdata->module;
    ib_engine_t *ib = module->ib;
    ib_context_t *ctx;

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_list");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);
    lua_pushlightuserdata(L, (void *)list);

    rc = modlua_config_cb_eval(L, ib, module, name, 5);
    return rc;
}
/**
 * @param[in] cp Configuration parser.
 * @param[in] name Configuration directive name.
 * @param[in] mask The flags we are setting.
 * @param[in] cbdata Callback data.
 */
static ib_status_t modlua_config_cb_opflags(
    ib_cfgparser_t *cp,
    const char *name,
    ib_flags_t mask,
    void *cbdata)
{
    assert(cp);
    assert(name);
    assert(cbdata);

    ib_status_t rc;
    modlua_lua_cbdata_t *modlua_lua_cbdata = (modlua_lua_cbdata_t *)cbdata;
    lua_State *L = modlua_global_cfg.L;
    ib_module_t *module = modlua_lua_cbdata->module;
    ib_engine_t *ib = module->ib;
    ib_context_t *ctx;

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_opflags");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);
    lua_pushinteger(L, mask);

    rc = modlua_config_cb_eval(L, ib, module, name, 5);
    return rc;
}
/**
 * @param[in] cp Configuration parser.
 * @param[in] name Configuration directive name.
 * @param[in] p1 The block name that we are exiting.
 * @param[in] cbdata Callback data.
 */
static ib_status_t modlua_config_cb_sblk1(
    ib_cfgparser_t *cp,
    const char *name,
    const char *p1,
    void *cbdata)
{
    assert(cp);
    assert(name);
    assert(p1);
    assert(cbdata);

    ib_status_t rc;
    modlua_lua_cbdata_t *modlua_lua_cbdata = (modlua_lua_cbdata_t *)cbdata;
    lua_State *L = modlua_global_cfg.L;
    ib_module_t *module = modlua_lua_cbdata->module;
    ib_engine_t *ib = module->ib;
    ib_context_t *ctx;

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_sblk1");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);
    lua_pushstring(L, p1);

    rc = modlua_config_cb_eval(L, ib, module, name, 5);
    return rc;
}


/**
 * Proxy function to ib_config_register_directive callable by Lua.
 *
 * @param[in] L Lua state. This stack should have on it 3 arguments
 *            to this function:
 *            - @c self - The Module API object.
 *            - @c name - The name of the directive to register.
 *            - @c type - The type of directive this is.
 *            - @c strvalmap - Optional string-value table map.
 */
static int modlua_config_register_directive(lua_State *L)
{
    assert(L);

    ib_status_t rc = IB_OK;
    const char *rcmsg = "Success.";

    /* Variables pulled from the Lua state. */
    ib_engine_t *ib;                /* Fetched from self on *L */
    const char *name;               /* Name of the directive. 2nd arg. */
    ib_dirtype_t type;              /* Type of directive. 3rd arg. */
    int args = lua_gettop(L);       /* Arguments passed to this function. */
    ib_strval_t *strvalmap;         /* Optional 4th arg. Must be converted. */
    ib_void_fn_t cfg_cb;            /* Configuration callback. */
    ib_mpool_t *mp;
    modlua_lua_cbdata_t *modlua_lua_cbdata;
    ib_module_t *module;

    /* We choose assert here because if this value is incorrect,
     * then the lua module code (lua.c and ironbee/module.lua) are
     * inconsistent with each other. */
    assert(args==3 || args==4);

    if (lua_istable(L, 0-args)) {

        /* Get IB Engine. */
        lua_getfield(L, 0-args, "ib_engine");
        if ( !lua_islightuserdata(L, -1) ) {
            lua_pop(L, 1);
            rc = IB_EINVAL;
            rcmsg = "ib_engine is not defined in module.";
            goto exit;
        }
        ib = (ib_engine_t *)lua_topointer(L, -1);
        assert(ib);
        lua_pop(L, 1); /* Remove IB. */

        /* Get Module. */
        lua_getfield(L, 0-args, "ib_module");
        if ( !lua_islightuserdata(L, -1) ) {
            lua_pop(L, 1);
            rc = IB_EINVAL;
            rcmsg = "ib_engine is not defined in module.";
            goto exit;
        }
        module = (ib_module_t *)lua_topointer(L, -1);
        assert(module);
        lua_pop(L, 1); /* Remove Module. */

    }
    else {
        rc = IB_EINVAL;
        rcmsg = "1st argument is not self table.";
        goto exit;
    }

    if (lua_isstring(L, 1-args)) {
        name = lua_tostring(L, 1-args);
    }
    else {
        rc = IB_EINVAL;
        rcmsg = "2nd argument is not a string.";
        goto exit;
    }

    if (lua_isnumber(L, 2-args)) {
        type = lua_tonumber(L, 2-args);
    }
    else {
        rc = IB_EINVAL;
        rcmsg = "3rd argument is not a number.";
        goto exit;
    }

    if (args==4) {
        if (lua_istable(L, 3-args)) {
            int varmapsz = 0;

            /* Iterator. */
            int i;

            /* Count the size of the table. table.maxn is not sufficient. */
            while (lua_next(L, 3-args)) { /* Push string, int onto stack. */
                ++varmapsz;
                lua_pop(L, 1); /* Pop off value. Leave key. */
            }

            if (varmapsz > 0) {
                /* Allocate space for strvalmap. */
                strvalmap = malloc(sizeof(*strvalmap) * (varmapsz + 1));
                if (strvalmap == NULL) {
                    rc = IB_EALLOC;
                    rcmsg = "Cannot allocate strval map.";
                    goto exit;
                }

                /* Build map. */
                i = 0;
                while (lua_next(L, 3-args)) { /* Push string, int onto stack. */
                    strvalmap[i].str = lua_tostring(L, -2);
                    strvalmap[i].val = lua_tointeger(L, -1);
                    lua_pop(L, 1); /* Pop off value. Leave key. */
                    ++i;
                }

                /* Null terminate the list. */
                strvalmap[varmapsz].str = NULL;
                strvalmap[varmapsz].val = 0;
            }
            else {
                strvalmap = NULL;
            }
        }
        else {
            rc = IB_EINVAL;
            rcmsg = "4th argument is not a table.";
            goto exit;
        }
    }
    else {
        strvalmap = NULL;
    }

    mp = ib_engine_pool_main_get(ib);
    modlua_lua_cbdata = ib_mpool_alloc(mp, sizeof(*modlua_lua_cbdata));
    if (modlua_lua_cbdata == NULL) {
        rc = IB_EALLOC;
        rcmsg = "Failed to allocate callback data structure for directive.";
        if (strvalmap != NULL) {
            free(strvalmap);
        }
        goto exit;
    }
    modlua_lua_cbdata->module = module;

    /* Assign the cfg_cb pointer to hand the callback. */
    switch (type) {
        case IB_DIRTYPE_ONOFF:
            cfg_cb = (ib_void_fn_t) &modlua_config_cb_onoff;
            break;
        case IB_DIRTYPE_PARAM1:
            cfg_cb = (ib_void_fn_t) &modlua_config_cb_param1;
            break;
        case IB_DIRTYPE_PARAM2:
            cfg_cb = (ib_void_fn_t) &modlua_config_cb_param2;
            break;
        case IB_DIRTYPE_LIST:
            cfg_cb = (ib_void_fn_t) &modlua_config_cb_list;
            break;
        case IB_DIRTYPE_OPFLAGS:
            cfg_cb = (ib_void_fn_t) &modlua_config_cb_opflags;
            break;
        case IB_DIRTYPE_SBLK1:
            cfg_cb = (ib_void_fn_t) &modlua_config_cb_sblk1;
            break;
        default:
            rc = IB_EINVAL;
            rcmsg = "Invalid configuration type.";
            goto exit;
    }

    rc = ib_config_register_directive(
        ib,
        name,
        type,
        cfg_cb,
        &modlua_config_cb_blkend,
        modlua_lua_cbdata,
        modlua_lua_cbdata,
        strvalmap);
    if (rc != IB_OK) {
        rcmsg = "Failed to register directive.";
        goto exit;
    }

exit:
    lua_pop(L, args);
    lua_pushinteger(L, rc);
    lua_pushstring(L, rcmsg);

    return lua_gettop(L);
}

/**
 * Push the specified handler for a lua module on top of the Lua stack L.
 *
 * @returns
 *   - IB_OK on success. The stack is 1 element higher.
 *   - IB_EINVAL on a Lua runtime error.
 */
static ib_status_t modlua_push_lua_handler(
    ib_engine_t *ib,
    ib_module_t *module,
    ib_state_event_type_t event,
    lua_State *L)
{
    assert(ib);
    assert(module);
    assert(L);

    int isfunction;
    int lua_rc;

    lua_getglobal(L, "modlua"); /* Get the package. */
    if (lua_isnil(L, -1)) {
        ib_log_error(ib, "Module modlua is undefined.");
        return IB_EINVAL;
    }
    if (! lua_istable(L, -1)) {
        ib_log_error(ib, "Module modlua is not a table/module.");
        lua_pop(L, 1); /* Pop modlua global off stack. */
        return IB_EINVAL;
    }

    lua_pushstring(L, "get_callback"); /* Push get_callback. */
    lua_gettable(L, -2);               /* Pop get_callback and get func. */
    if (lua_isnil(L, -1)) {
        ib_log_error(ib, "Module function get_callback is undefined.");
        lua_pop(L, 1); /* Pop modlua global off stack. */
        return IB_EINVAL;
    }
    if (! lua_isfunction(L, -1)) {
        ib_log_error(ib, "Module function get_callback is not a function.");
        lua_pop(L, 1); /* Pop modlua global off stack. */
        return IB_EINVAL;
    }

    lua_pushlightuserdata(L, ib);
    lua_pushinteger(L, module->idx);
    lua_pushinteger(L, event);
    lua_rc = lua_pcall(L, 3, 1, 0);
    switch(lua_rc) {
        case 0:
            /* NOP */
            break;
        case LUA_ERRRUN:
            ib_log_error(
                ib,
                "Error loading module %s: %s",
                module->name,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
        case LUA_ERRMEM:
            ib_log_error(
                ib,
                "Failed to allocate memory during module load of %s",
                module->name);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
        case LUA_ERRERR:
            ib_log_error(
                ib,
                "Error fetching error message during module load of %s",
                module->name);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
#if LUA_VERSION_NUM > 501
        /* If LUA_ERRGCMM is defined, include a custom error for it as well.
          This was introduced in Lua 5.2. */
        case LUA_ERRGCMM:
            ib_log_error(
                ib,
                "Garbage collection error during module load of %s.",
                module->name);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
#endif
        default:
            ib_log_error(
                ib,
                "Unexpected error(%d) during evaluation of %s: %s",
                lua_rc,
                module->name,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
    }

    isfunction = lua_isfunction(L, -1);

    /* Pop off modlua table by moving the function at -1 to -2 and popping. */
    lua_replace(L, -2);

    return isfunction ? IB_OK : IB_ENOENT;
}

/**
 * Check if a Lua module has a callback handler for a particular event.
 *
 * @param[in] ib IronBee engine.
 * @param[in] module The module to check.
 * @param[in] event The event to check for.
 * @param[in] L The Lua state that is checked. While it is an "in"
 *            parameter, it is manipulated and returned to its
 *            original state before this function returns.
 *
 * @returns
 *   - IB_OK if a handler exists.
 *   - IB_ENOENT if a handler does not exist.
 *   - IB_EINVAL on a Lua runtime error. See log file for details.
 */
static ib_status_t module_has_callback(
    ib_engine_t *ib,
    ib_module_t *module,
    ib_state_event_type_t event,
    lua_State *L)
{
    assert(ib);
    assert(module);
    assert(L);

    ib_status_t rc;

    rc = modlua_push_lua_handler(ib, module, event, L);

    /* Pop the lua handler off the stack. We're just checking for it. */
    lua_pop(L, 1);

    return rc;
}

/**
 * Push the lua callback dispatcher function to the stack.
 *
 * It takes a callback function handler and a table of arguments as
 * arguments. When run, it will pre-process any arguments
 * using the FFI and hand the user a final table.
 *
 * @param[in] ib IronBee engine.
 * @param[in] module The module to check.
 * @param[in] event The event to check for.
 * @param[in,out] L The Lua state to push the dispatcher onto.
 *
 * @returns
 *   - IB_OK if a handler exists.
 *   - IB_ENOENT if a handler does not exist.
 *   - IB_EINVAL on a Lua runtime error. See log file for details.
 */
static ib_status_t modlua_push_dispatcher(
    ib_engine_t *ib,
    ib_module_t *module,
    ib_state_event_type_t event,
    lua_State *L)
{
    assert(ib);
    assert(module);
    assert(L);

    lua_getglobal(L, "modlua"); /* Get the package. */
    if (lua_isnil(L, -1)) {
        ib_log_error(ib, "Module modlua is undefined.");
        return IB_EINVAL;
    }
    if (! lua_istable(L, -1)) {
        ib_log_error(ib, "Module modlua is not a table/module.");
        lua_pop(L, 1); /* Pop modlua global off stack. */
        return IB_EINVAL;
    }

    lua_pushstring(L, "dispatch_module"); /* Push dispatch_module */
    lua_gettable(L, -2);               /* Pop "dispatch_module" and get func. */
    if (lua_isnil(L, -1)) {
        ib_log_error(ib, "Module function dispatch_module is undefined.");
        lua_pop(L, 1); /* Pop modlua global off stack. */
        return IB_EINVAL;
    }
    if (! lua_isfunction(L, -1)) {
        ib_log_error(ib, "Module function dispatch_module is not a function.");
        lua_pop(L, 1); /* Pop modlua global off stack. */
        return IB_EINVAL;
    }

    /* Replace the modlua table by replacing it with dispatch_handler. */
    lua_replace(L, -2);

    return IB_OK;
}

/**
 * Push the callback dispatcher, callback handler, and argument table.
 *
 * Functions (callback hooks) that use this function should then
 * modify the table at the top of the stack to include custom
 * arguments and then call @ref modlua_callback_dispatch.
 *
 * The table at the top of the stack will have defined in it:
 *   - @c ib_engine
 *   - @c ib_tx (if @a tx is not null)
 *   - @c ib_conn
 *   - @c event as an integer
 *
 * @param[in] ib The IronBee engine. This may not be null.
 * @param[in] event The event type.
 * @param[in] tx The transaction. This may be null.
 * @param[in] conn The connection. This may not be null. We require it to,
 *            at a minimum, find the Lua runtime stack.
 * @param[in] cbdata The callback data. This may not be null.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EOTHER on a Lua runtime error.
 */
static ib_status_t modlua_callback_setup(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_tx_t *tx,
    ib_conn_t *conn,
    void *cbdata)
{
    assert(ib);
    assert(conn);
    assert(cbdata);

    ib_status_t rc;
    modlua_lua_cbdata_t *modlua_lua_cbdata;
    ib_module_t *module;
    lua_State *L;
    modlua_runtime_t *lua;

    modlua_lua_cbdata = (modlua_lua_cbdata_t *)cbdata;
    module = modlua_lua_cbdata->module;

    rc = modlua_runtime_get(conn, &lua);
    if (rc != IB_OK) {
        return rc;
    }

    L = lua->L;

    /* Push Lua dispatch method to stack. */
    rc = modlua_push_dispatcher(ib, module, event, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot push modlua.dispatch_handler to stack.");
        return rc;
    }

    /* Push Lua handler onto the table. */
    rc = modlua_push_lua_handler(ib, module, event, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot push modlua event handler to stack.");
        return rc;
    }

    lua_pushlightuserdata(L, ib);
    lua_pushinteger(L, module->idx);
    lua_pushinteger(L, event);
    rc = modlua_push_config_path(ib, conn->ctx, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to push configuration path onto Lua stack.");
        return rc;
    }
    lua_pushlightuserdata(L, conn);
    if (tx) {
        lua_pushlightuserdata(L, tx);
    }
    else {
        lua_pushnil(L);
    }

    return IB_OK;
}

/**
 * Common code to run the module handler.
 *
 * This is the basic function. This is almost always
 * called by a wrapper function that unwraps Lua values from
 * the connection or module for us, but in the case of a null event
 * callback, this is called directly
 */
static ib_status_t modlua_callback_dispatch_base(
    ib_engine_t *ib,
    ib_module_t *module,
    lua_State *L)
{
    assert(ib);
    assert(module);
    assert(L);

    ib_status_t rc;
    int lua_rc;

    ib_log_debug(ib, "Calling handler for lua module: %s", module->name);

    /* Run dispatcher. */
    lua_rc = lua_pcall(L, 7, 1, 0);
    switch(lua_rc) {
        case 0:
            /* NOP */
            break;
        case LUA_ERRRUN:
            ib_log_error(
                ib,
                "Error running callback %s: %s",
                module->name,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
        case LUA_ERRMEM:
            ib_log_error(
                ib,
                "Failed to allocate memory during callback of %s",
                module->name);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
        case LUA_ERRERR:
            ib_log_error(
                ib,
                "Error fetching error message during callback of %s",
                module->name);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
#if LUA_VERSION_NUM > 501
        /* If LUA_ERRGCMM is defined, include a custom error for it as well.
          This was introduced in Lua 5.2. */
        case LUA_ERRGCMM:
            ib_log_error(
                ib,
                "Garbage collection error during callback of %s.",
                module->name);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
#endif
        default:
            ib_log_error(
                ib,
                "Unexpected error(%d) during callback %s: %s",
                lua_rc,
                module->name,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
    }

    if (lua_isnumber(L, -1)) {
        rc = lua_tonumber(L, -1);
        lua_pop(L, 1);
        ib_log_debug(
            ib,
            "Exited with status %s(%d) for lua module with status: %s",
            ib_status_to_string(rc),
            rc,
            module->name);

    }
    else {
        ib_log_error(
            ib,
            "Lua handler did not return numeric status code. "
            "Returning IB_EOTHER");
        rc = IB_EOTHER;
    }

    return rc;
}

/**
 * Common code to run the module handler.
 *
 * This requires modlua_callback_setup to have been successfully run.
 *
 * @param[in] ib The IronBee engine. This may not be null.
 * @param[in] event The event type.
 * @param[in] tx The transaction. This may be null.
 * @param[in] conn The connection. This may not be null. We require it to,
 *            at a minimum, find the Lua runtime stack.
 * @param[in] cbdata The callback data. This may not be null.
 *
 * @returns
 *   - The result of the module dispatch call.
 *   - IB_E* values may be returned as a result of an internal
 *     module failure. The log file should always be examined
 *     to determine if a Lua module failure or an internal
 *     error is to blame.
 */
static ib_status_t modlua_callback_dispatch(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_tx_t *tx,
    ib_conn_t *conn,
    void *cbdata)
{
    assert(ib);
    assert(conn);
    assert(cbdata);

    ib_status_t rc;
    modlua_lua_cbdata_t *modlua_lua_cbdata;
    ib_module_t *module;
    lua_State *L;
    modlua_runtime_t *lua;

    modlua_lua_cbdata = (modlua_lua_cbdata_t *)cbdata;
    module = modlua_lua_cbdata->module;

    rc = modlua_runtime_get(conn, &lua);
    if (rc != IB_OK) {
        return rc;
    }

    L = lua->L;

    return modlua_callback_dispatch_base(ib, module, L);
}

/**
 * Null callback hook.
 */
static ib_status_t modlua_null(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    void *cbdata)
{
    assert(ib);
    assert(cbdata);

    ib_status_t rc;
    ib_status_t join_rc; /* We need a temporary rc value for thread joins. */
    lua_State *L;
    modlua_lua_cbdata_t *modlua_lua_cbdata;
    ib_module_t *module;

    modlua_lua_cbdata = (modlua_lua_cbdata_t *)cbdata;
    module = modlua_lua_cbdata->module;

    /* Since there is  no connection Lua stack, we make a new one. */
    rc = call_in_critical_section(ib, &ib_lua_new_thread, &L);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to allocate new Lua thread.");
        return rc;
    }

    /* Push Lua dispatch method to stack. */
    rc = modlua_push_dispatcher(ib, module, event, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot push modlua.dispatch_handler to stack.");
        return rc;
    }

    /* Push Lua handler onto the table. */
    rc = modlua_push_lua_handler(ib, module, event, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot push modlua event handler to stack.");
        return rc;
    }

    lua_pushlightuserdata(L, ib);
    lua_pushinteger(L, module->idx);
    lua_pushinteger(L, event);
    rc = modlua_push_config_path(ib, ib_context_main(ib), L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot push modlua.config_path to stack.");
        return rc;
    }
    lua_pushnil(L); /* Connection (conn) is nil. */
    lua_pushnil(L); /* Transaction (tx) is nil. */

    rc = modlua_callback_dispatch_base(ib, module, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failure while executing callback handler.");
        /* Do not return. We must join the Lua thread. */
    }

    join_rc = call_in_critical_section(ib, &ib_lua_join_thread, &L);
    if (join_rc != IB_OK) {
        ib_log_alert(ib, "Failed to join created Lua thread.");

        /* If there is no other error, return the join error. */
        if (rc == IB_OK) {
            rc = join_rc;
        }
    }

    return rc;
}

/**
 * Connection callback hook.
 */
static ib_status_t modlua_conn(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_conn_t *conn,
    void *cbdata)
{
    assert(ib);
    assert(conn);
    assert(cbdata);

    ib_status_t rc;

    rc = modlua_callback_setup(ib, event, NULL, conn, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    /* Custom table setup */

    rc = modlua_callback_dispatch(ib, event, NULL, conn, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    return rc;
}

/**
 * Transaction callback hook.
 */
static ib_status_t modlua_tx(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    void *cbdata)
{
    assert(cbdata);
    assert(tx);
    assert(tx->conn);
    assert(ib);

    ib_status_t rc;

    rc = modlua_callback_setup(ib, event, tx, tx->conn, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    /* Custom table setup */

    rc = modlua_callback_dispatch(ib, event, tx, tx->conn, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    return rc;
}

/**
 * Transaction data callback hook.
 */
static ib_status_t modlua_txdata(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_txdata_t *txdata,
    void *cbdata)
{
    assert(ib);
    assert(tx);
    assert(tx->conn);
    assert(cbdata);

    ib_status_t rc;

    rc = modlua_callback_setup(ib, event, tx, tx->conn, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    /* Custom table setup */

    rc = modlua_callback_dispatch(ib, event, tx, tx->conn, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    return rc;
}

/**
 * Header callback hook.
 */
static ib_status_t modlua_header(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_parsed_header_t *header,
    void *cbdata)
{
    assert(ib);
    assert(tx);
    assert(tx->conn);
    assert(cbdata);

    ib_status_t rc;

    rc = modlua_callback_setup(ib, event, tx, tx->conn, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    /* Custom table setup */

    rc = modlua_callback_dispatch(ib, event, tx, tx->conn, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    return rc;
}

/**
 * Request line callback hook.
 */
static ib_status_t modlua_reqline(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_parsed_req_line_t *line,
    void *cbdata)
{
    assert(ib);
    assert(tx);
    assert(tx->conn);
    assert(cbdata);

    ib_status_t rc;

    rc = modlua_callback_setup(ib, event, tx, tx->conn, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    /* Custom table setup */

    rc = modlua_callback_dispatch(ib, event, tx, tx->conn, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    return rc;
}

/**
 * Response line callback hook.
 */
static ib_status_t modlua_respline(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_parsed_resp_line_t *line,
    void *cbdata)
{
    assert(ib);
    assert(tx);
    assert(tx->conn);
    assert(cbdata);

    ib_status_t rc;

    rc = modlua_callback_setup(ib, event, tx, tx->conn, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    /* Custom table setup */

    rc = modlua_callback_dispatch(ib, event, tx, tx->conn, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    return rc;
}

/**
 * Called by modlua_module_load to load the lua script into the Lua runtime.
 *
 * @param[in] ib IronBee engine.
 * @param[in] file The file we are loading.
 * @param[in] module The registered module structure.
 * @param[in,out] L The lua context that @a file will be loaded into as
 *                @a module.
 * @returns
 *   - IB_OK on success.
 */
static ib_status_t modlua_module_load_lua(
    ib_engine_t *ib,
    const char *file,
    ib_module_t *module,
    lua_State *L)
{
    assert(ib);
    assert(file);
    assert(module);
    assert(L);

    int lua_rc;

    lua_getglobal(L, "modlua"); /* Get the package. */
    if (lua_isnil(L, -1)) {
        ib_log_error(ib, "Module modlua is undefined.");
        return IB_EINVAL;
    }
    if (! lua_istable(L, -1)) {
        ib_log_error(ib, "Module modlua is not a table/module.");
        lua_pop(L, 1); /* Pop modlua global off stack. */
        return IB_EINVAL;
    }

    lua_getfield(L, -1, "load_module"); /* Push load_module */
    if (lua_isnil(L, -1)) {
        ib_log_error(ib, "Module function load_module is undefined.");
        lua_pop(L, 1); /* Pop modlua global off stack. */
        return IB_EINVAL;
    }
    if (! lua_isfunction(L, -1)) {
        ib_log_error(ib, "Module function load_module is not a function.");
        lua_pop(L, 1); /* Pop modlua global off stack. */
        return IB_EINVAL;
    }

    lua_pushlightuserdata(L, ib); /* Push ib engine. */
    lua_pushlightuserdata(L, module); /* Push module engine. */
    lua_pushstring(L, file);
    lua_pushinteger(L, module->idx);
    lua_pushcfunction(L, &modlua_config_register_directive);
    lua_rc = luaL_loadfile(L, file);
    switch(lua_rc) {
        case 0:
            /* NOP */
            break;
        case LUA_ERRRUN:
            ib_log_error(
                ib,
                "Error evaluating %s: %s",
                file,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
        case LUA_ERRMEM:
            ib_log_error(
                ib,
                "Failed to allocate memory during evaluation of %s",
                file);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
        case LUA_ERRERR:
            ib_log_error(
                ib,
                "Error fetching error message during evaluation of %s",
                file);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
#if LUA_VERSION_NUM > 501
        /* If LUA_ERRGCMM is defined, include a custom error for it as well.
          This was introduced in Lua 5.2. */
        case LUA_ERRGCMM:
            ib_log_error(
                ib,
                "Garbage collection error during evaluation of %s.",
                file);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
#endif
        default:
            ib_log_error(
                ib,
                "Unexpected error(%d) during evaluation of %s: %s",
                lua_rc,
                file,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
    }

    /**
     * The stack now is...
     * +----------------------------------+
     * | load_module                      |
     * | ib                               |
     * | ib_module                        |
     * | module name (file name)          |
     * | module index                     |
     * | modlua_config_register_directive |
     * | module script                    |
     * +----------------------------------+
     *
     * Next step is to call load_module which will, in turn, execute
     * the module script.
     */
    lua_rc = lua_pcall(L, 6, 1, 0);
    switch(lua_rc) {
        case 0:
            /* NOP */
            break;
        case LUA_ERRRUN:
            ib_log_error(
                ib,
                "Error loading module %s: %s",
                file,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
        case LUA_ERRMEM:
            ib_log_error(
                ib,
                "Failed to allocate memory during module load of %s",
                file);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
        case LUA_ERRERR:
            ib_log_error(
                ib,
                "Error fetching error message during module load of %s",
                file);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
#if LUA_VERSION_NUM > 501
        /* If LUA_ERRGCMM is defined, include a custom error for it as well.
          This was introduced in Lua 5.2. */
        case LUA_ERRGCMM:
            ib_log_error(
                ib,
                "Garbage collection error during module load of %s.",
                file);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
#endif
        default:
            ib_log_error(
                ib,
                "Unexpected error(%d) during evaluation of %s: %s",
                lua_rc,
                file,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
    }

    lua_pop(L, 1); /* Pop modlua global off stack. */

    return IB_OK;
}

/**
 * Called by modlua_module_load to wire the callbacks in @a ib.
 *
 * @param[in] ib IronBee engine.
 * @param[in] file The file we are loading.
 * @param[in] module The registered module structure.
 * @param[in,out] L The lua context that @a file will be loaded into as
 *                @a module.
 * @returns
 *   - IB_OK on success.
 */
static ib_status_t modlua_module_load_wire_callbacks(
    ib_engine_t *ib,
    const char *file,
    ib_module_t *module,
    lua_State *L)
{
    ib_status_t rc;

    ib_mpool_t *mp;

    modlua_lua_cbdata_t *cbdata;

    mp = ib_engine_pool_main_get(ib);
    if (mp == NULL) {
        ib_log_error(
            ib,
            "Failed to fetch main engine memory pool for Lua module: %s",
            file);
        return IB_EOTHER;
    }

    cbdata = ib_mpool_alloc(mp, sizeof(*cbdata));
    if (!cbdata) {
        return IB_EALLOC;
    }

    cbdata->module = module;

    for (ib_state_event_type_t event = 0; event < IB_STATE_EVENT_NUM; ++event) {

        rc = module_has_callback(ib, module, event, L);
        if (rc == IB_OK) {
            switch(ib_state_hook_type(event)) {
                case IB_STATE_HOOK_NULL:
                    rc = ib_hook_null_register(ib, event, modlua_null, cbdata);
                    break;
                case IB_STATE_HOOK_INVALID:
                    ib_log_error(ib, "Invalid hook: %d", event);
                    break;
                case IB_STATE_HOOK_CONN:
                    rc = ib_hook_conn_register(ib, event, modlua_conn, cbdata);
                    break;
                case IB_STATE_HOOK_TX:
                    rc = ib_hook_tx_register(ib, event, modlua_tx, cbdata);
                    break;
                case IB_STATE_HOOK_TXDATA:
                    rc = ib_hook_txdata_register(
                        ib,
                        event,
                        modlua_txdata,
                        cbdata);
                    break;
                case IB_STATE_HOOK_REQLINE:
                    rc = ib_hook_parsed_req_line_register(
                        ib,
                        event,
                        modlua_reqline,
                        cbdata);
                    break;
                case IB_STATE_HOOK_RESPLINE:
                    rc = ib_hook_parsed_resp_line_register(
                        ib,
                        event,
                        modlua_respline,
                        cbdata);
                    break;
                case IB_STATE_HOOK_HEADER:
                    rc = ib_hook_parsed_header_data_register(
                        ib,
                        event,
                        modlua_header,
                        cbdata);
                    break;
            }
        }
        if ((rc != IB_OK) && (rc != IB_ENOENT)) {
            ib_log_error(ib,
                         "Failed to register hook: %s",
                         ib_status_to_string(rc));
            return rc;
        }
    }

    return IB_OK;
}

/**
 * Load a Lua module from a .lua file.
 *
 * This will dynamically create a Lua module that is managed by this
 * module.
 *
 * @param[in,out] ib IronBee Engine. Mostly used for logging, but will also
 *                receive the constructed module.
 * @param[in] file The file to read off of disk that contains the
 *            Lua module definition.
 *
 * @returns
 *   - IB_OK on success.
 */
static ib_status_t modlua_module_load(ib_engine_t *ib, const char *file) {
    lua_State *L;
    ib_module_t *module;
    ib_status_t rc;

    rc = build_near_empty_module(ib, file, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot initialize empty lua module structure.");
        return rc;
    }

    /* Uses the main configuration's Lua state to create a global,
     * read-only module object. */
    L = modlua_global_cfg.L;
    if (L == NULL) {
        ib_log_error(
            ib,
            "Cannot load lua module \"%s\": Lua support not available.",
            file);
        return IB_OK;
    }

    rc = modlua_module_load_lua(ib, file, module, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to load lua modules: %s", file);
        return rc;
    }

    rc = modlua_module_load_wire_callbacks(ib, file, module, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed register lua callbacks for module : %s", file);
        return rc;
    }

    return rc;
}

/**
 * Given a search prefix this will build a search path and add it to Lua.
 *
 * @param[in] ib IronBee engine.
 * @param[in,out] L Lua State that the search path will be set in.
 * @param[in] prefix The prefix. ?.lua will be appended.
 * @returns
 *   - IB_OK on success
 *   - IB_EALLOC on malloc failure.
 */
static ib_status_t modlua_append_searchprefix(
    ib_engine_t *ib,
    lua_State *L,
    const char *prefix)
{
    /* This is the search pattern that is appended to each element of
     * lua_search_paths and then added to the Lua runtime package.path
     * global variable. */
    const char *lua_file_pattern = "?.lua";

    char *path = NULL; /* Tmp string to build a search path. */
    ib_log_debug(ib, "Adding \"%s\" to lua search path.", prefix);

    /* Strlen + 2. One for \0 and 1 for the path separator. */
    path = malloc(strlen(prefix) + strlen(lua_file_pattern) + 2);
    if (path == NULL) {
        ib_log_error(ib, "Could allocate buffer for string append.");
        free(path);
        return IB_EALLOC;
    }

    strcpy(path, prefix);
    strcpy(path + strlen(path), "/");
    strcpy(path + strlen(path), lua_file_pattern);

    ib_lua_add_require_path(ib, L, path);

    ib_log_debug(ib, "Added \"%s\" to lua search path.", path);

    /* We are done with path. To be safe, we NULL it as there is more work
     * to be done in this function, and we do not want to touch path again. */
    free(path);

    return IB_OK;
}

/**
 * Set the search path in the lua state from the core config.
 */
static ib_status_t modlua_setup_searchpath(ib_engine_t *ib, lua_State *L)
{
    ib_status_t rc;

    ib_core_cfg_t *corecfg = NULL;
    /* Null terminated list of search paths. */
    const char *lua_search_paths[3];


    rc = ib_context_module_config(
        ib_context_main(ib),
        ib_core_module(),
        &corecfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not retrieve core module configuration.");
        return rc;
    }

    /* Initialize the search paths list. */
    lua_search_paths[0] = corecfg->module_base_path;
    lua_search_paths[1] = corecfg->rule_base_path;
    lua_search_paths[2] = NULL;

    for (int i = 0; lua_search_paths[i] != NULL; ++i)
    {
        rc = modlua_append_searchprefix(ib, L, lua_search_paths[i]);
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
 *   - waggle  = require("ironbee/waggle")
 *   - ffi     = require("ffi")
 *   - ironbee = require("ironbee-ffi")
 *   - ibapi   = require("ironbee/api")
 *   - modlua  = require("ironbee/module")
 *
 * @param[in] ib IronBee engine. Used to find load paths from the
 *            core module.
 * @param[out] L The Lua state that the modules will be "required" into.
 */
static ib_status_t modlua_preload(ib_engine_t *ib, lua_State *L) {

    ib_status_t rc;

    const char *lua_preloads[][2] = { { "waggle", "ironbee/waggle" },
                                      { "ibconfig", "ironbee/config" },
                                      { "ffi", "ffi" },
                                      { "ffi", "ffi" },
                                      { "ironbee", "ironbee-ffi" },
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
 * Commit any pending configuration items, such as rules.
 *
 * @param[in] ib IronBee engine.
 * @param[in] m The module object.
 *
 * @returns
 *   - IB_OK
 *   - IB_EOTHER on Rule adding errors. See log file.
 */
static ib_status_t modlua_commit_configuration(ib_engine_t *ib)
{
    ib_status_t rc;
    int lua_rc;
    lua_State *L = modlua_global_cfg.L;

    lua_getglobal(L, "ibconfig");
    if ( ! lua_istable(L, -1) ) {
        ib_log_error(ib, "ibconfig is not a module table.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_getfield(L, -1, "build_rules");
    if ( ! lua_isfunction(L, -1) ) {
        ib_log_error(ib, "ibconfig.include is not a function.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_pushlightuserdata(L, ib);
    lua_rc = lua_pcall(L, 1, 1, 0);
    if (lua_rc == LUA_ERRFILE) {
        ib_log_error(ib, "Configuration Error: %s", lua_tostring(L, -1));
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }
    else if (lua_rc) {
        ib_log_error(ib, "Configuration Error: %s", lua_tostring(L, -1));
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }
    else if (lua_tonumber(L, -1) != IB_OK) {
        rc = lua_tonumber(L, -1);
        lua_pop(L, lua_gettop(L));
        ib_log_error(
            ib,
            "Configuration error reported: %d:%s",
            rc,
            ib_status_to_string(rc));
        return IB_EOTHER;
    }

    /* Clear stack. */
    lua_pop(L, lua_gettop(L));

    return IB_OK;
}


/* -- Event Handlers -- */

/**
 * Callback to initialize the per-connection Lua stack.
 *
 * Every Lua Module shares the same Lua stack for execution. This stack is
 * "owned" not by the module written in Lua by the user, but by this
 * module, the Lua Module.
 *
 * @param ib Engine.
 * @param event Event type.
 * @param conn Connection.
 * @param cbdata Unused.
 *
 * @return
 *   - IB_OK
 *   - IB_EALLOC on memory allocation failure.
 *   - Other upon failure of callback registration.
 */
static ib_status_t modlua_conn_init_lua_runtime(ib_engine_t *ib,
                                                ib_state_event_type_t event,
                                                ib_conn_t *conn,
                                                void *cbdata)
{
    assert(event == conn_started_event);
    assert(ib);
    assert(conn);

    ib_status_t rc;
    modlua_runtime_t *modlua_runtime;

    modlua_runtime = ib_mpool_alloc(conn->mp, sizeof(modlua_runtime));
    if (!modlua_runtime) {
        return IB_EALLOC;
    }

    rc = call_in_critical_section(ib, &ib_lua_new_thread, &modlua_runtime->L);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to allocate new Lua thread for connection.");
        return rc;
    }

    rc = modlua_runtime_set(conn, modlua_runtime);
    if (rc != IB_OK) {
        ib_log_alert(
            ib,
            "Could not store connection Lua stack in connection.");
        return rc;
    }

    return IB_OK;
}

/**
 * Callback to destroy the per-connection Lua stack.
 *
 * Every Lua Module shares the same Lua stack for execution. This stack is
 * "owned" not by the module written in Lua by the user, but by this
 * module, the Lua Module.
 *
 * This is registered when the main context is closed to ensure that it
 * executes after all the Lua modules call backs (callbacks are executed
 * in the order of their registration).
 *
 * @param ib Engine.
 * @param event Event type.
 * @param conn Connection.
 * @param cbdata Unused.
 *
 * @return
 *   - IB_OK
 *   - IB_EALLOC on memory allocation failure.
 *   - IB_EOTHER if the stored Lua stack in the module's connection data
 *               is NULL.
 *   - Other upon failure of callback registration.
 */
static ib_status_t modlua_conn_fini_lua_runtime(ib_engine_t *ib,
                                                ib_state_event_type_t event,
                                                ib_conn_t *conn,
                                                void *cbdata)
{
    assert(event == conn_finished_event);
    assert(ib);
    assert(conn);

    ib_status_t rc;

    modlua_runtime_t *modlua_runtime;

    rc = modlua_runtime_get(conn, &modlua_runtime);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Could not fetch per-connection Lua execution stack.");
        return rc;
    }
    if (modlua_runtime == NULL) {
        ib_log_alert(ib, "Stored Lua execution stack was unexpectedly NULL.");
        return IB_EOTHER;
    }

    /* Atomically destroy the Lua stack */
    rc = call_in_critical_section(ib, &ib_lua_join_thread, &modlua_runtime->L);

    return rc;
}

/* -- External Rule Driver -- */

static ib_status_t rules_lua_init(ib_engine_t *ib, ib_module_t *m, void *cbdata)
{
    assert(ib != NULL);
    assert(m != NULL);
    ib_status_t rc;

    /* Register driver. */
    rc = ib_rule_register_external_driver(
        ib,
        "lua",
        modlua_rule_driver, NULL
    );
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register lua rule driver.");
        return rc;
    }

    return IB_OK;
}

/**
 * @brief Call the rule named @a func_name on a new Lua stack.
 * @details This will atomically create and destroy a lua_State*
 *          allowing for concurrent execution of @a func_name
 *          by a ib_lua_func_eval(ib_engine_t*, ib_txt_t*, const char*).
 *
 * @param[in,out] rule_exec Rule execution environment
 * @param[in] func_name The Lua function name to call.
 * @param[out] result The result integer value. This should be set to
 *             1 (true) or 0 (false).
 *
 * @returns IB_OK on success, IB_EUNKNOWN on semaphore locking error, and
 *          IB_EALLOC is returned if a new execution stack cannot be created.
 */
static ib_status_t ib_lua_func_eval_r(const ib_rule_exec_t *rule_exec,
                                      const char *func_name,
                                      ib_num_t *result)
{
    assert(rule_exec);

    ib_engine_t *ib = rule_exec->ib;
    ib_tx_t *tx = rule_exec->tx;
    int result_int;
    ib_status_t ib_rc;
    lua_State *L;

    /* Atomically create a new Lua stack */
    ib_rc = call_in_critical_section(ib, &ib_lua_new_thread, &L);

    if (ib_rc != IB_OK) {
        return ib_rc;
    }

    /* Call the rule in isolation. */
    ib_rc = ib_lua_func_eval_int(rule_exec, ib, tx, L, func_name, &result_int);

    /* Convert the passed in integer type to an ib_num_t. */
    *result = result_int;

    if (ib_rc != IB_OK) {
        return ib_rc;
    }

    /* Atomically destroy the Lua stack */
    ib_rc = call_in_critical_section(ib, &ib_lua_join_thread, &L);

    return ib_rc;
}

static ib_status_t lua_operator_create(ib_engine_t *ib,
                                       ib_context_t *ctx,
                                       const ib_rule_t *rule,
                                       ib_mpool_t *pool,
                                       const char *parameters,
                                       ib_operator_inst_t *op_inst)
{
    return IB_OK;
}

static ib_status_t lua_operator_execute(const ib_rule_exec_t *rule_exec,
                                        void *data,
                                        ib_flags_t flags,
                                        ib_field_t *field,
                                        ib_num_t *result)
{
    ib_status_t ib_rc;
    const char *func_name = (char *) data;

    ib_rule_log_trace(rule_exec, "Calling lua function %s.", func_name);

    ib_rc = ib_lua_func_eval_r(rule_exec, func_name, result);

    ib_rule_log_trace(rule_exec,
                      "Lua function %s=%"PRIu64".", func_name, *result);

    return ib_rc;
}

static ib_status_t lua_operator_destroy(ib_operator_inst_t *op_inst)
{
    return IB_OK;
}

/* -- Module Routines -- */


/**
 * Called by RuleExt lua:.
 *
 * @param[in] cp       Configuration parser.
 * @param[in] rule     Rule under construction.
 * @param[in] tag      Should be "lua".
 * @param[in] location What comes after "lua:".
 * @param[in] cbdata   Callback data; unused.
 * @return
 * - IB_OK on success.
 * - IB_EINVAL if Lua not available or not called for "lua" tag.
 * - Other error code if loading or registration fails.
 */
ib_status_t modlua_rule_driver(
    ib_cfgparser_t *cp,
    ib_rule_t *rule,
    const char *tag,
    const char *location,
    void *cbdata
)
{
    assert(cp != NULL);
    assert(tag != NULL);
    assert(location != NULL);

    ib_status_t rc;
    ib_operator_inst_t *op_inst;
    const char *slash;
    const char *name;

    if (strncmp(tag, "lua", 3) != 0) {
        ib_cfg_log_error(cp, "Lua rule driver called for non-lua tag.");
        return IB_EINVAL;
    }

    /* Check if lua is available. */
    if (modlua_global_cfg.L == NULL) {
        ib_cfg_log_error(cp, "Lua is not available");
        return IB_EINVAL;
    }

    rc = ib_lua_load_func(cp->ib,
                          modlua_global_cfg.L,
                          location,
                          ib_rule_id(rule));

    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to load lua file \"%s\"", location);
        return rc;
    }

    ib_cfg_log_debug3(cp, "Loaded lua file \"%s\"", location);
    slash = strrchr(location, '/');
    if (slash == NULL) {
        name = location;
    }
    else {
        name = slash + 1;
    }

    rc = ib_operator_register(cp->ib,
                              name,
                              IB_OP_FLAG_PHASE,
                              &lua_operator_create,
                              NULL,
                              &lua_operator_destroy,
                              NULL,
                              &lua_operator_execute,
                              NULL);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to register lua operator \"%s\": %s",
                         name, ib_status_to_string(rc));
        return rc;
    }

    rc = ib_operator_inst_create(cp->ib,
                                 cp->cur_ctx,
                                 rule,
                                 ib_rule_required_op_flags(rule),
                                 name,
                                 NULL,
                                 IB_OPINST_FLAG_NONE,
                                 &op_inst);

    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Failed to instantiate lua operator "
                         "for rule \"%s\": %s",
                         name, ib_status_to_string(rc));
        return rc;
    }

    /* The data is then name of the function. */
    op_inst->data = (void *)ib_rule_id(rule);

    rc = ib_rule_set_operator(cp->ib, rule, op_inst);

    if (rc != IB_OK) {
        ib_cfg_log_error(cp,
                         "Failed to associate lua operator \"%s\" "
                         "with rule \"%s\": %s",
                         name, ib_rule_id(rule), ib_status_to_string(rc));
        return rc;
    }

    ib_cfg_log_debug3(cp, "Set operator \"%s\" for rule \"%s\"",
                      name,
                      ib_rule_id(rule));

    return IB_OK;
}

/**
 * Initialize the ModLua Module.
 *
 * This will create a common "global" runtime into which various APIs
 * will be loaded.
 */
static ib_status_t modlua_init(ib_engine_t *ib,
                               ib_module_t *m,
                               void        *cbdata)
{
    ib_status_t rc;

    /* Set up defaults */
    modlua_global_cfg.L = NULL;

    modlua_global_cfg.L = luaL_newstate();
    if (modlua_global_cfg.L == NULL) {
        ib_log_error(ib, "Failed to initialize lua module.");
        return IB_EUNKNOWN;
    }

    luaL_openlibs(modlua_global_cfg.L);

    /* Setup search paths before ffi, api, etc loading. */
    rc = modlua_setup_searchpath(ib, modlua_global_cfg.L);
    if (rc != IB_OK) {
        return rc;
    }

    /* Load ffi, api, etc. */
    rc = modlua_preload(ib, modlua_global_cfg.L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to pre-load Lua files.");
        return rc;
    }

    /* Set package paths if configured. */
    if (modlua_global_cfg.pkg_path) {
        ib_log_debug(
            ib,
            "Using lua package.path=\"%s\"",
             modlua_global_cfg.pkg_path);
        lua_getfield(modlua_global_cfg.L, -1, "path");
        lua_pushstring(modlua_global_cfg.L, modlua_global_cfg.pkg_path);
        lua_setglobal(modlua_global_cfg.L, "path");
    }
    if (modlua_global_cfg.pkg_cpath) {
        ib_log_debug(
            ib,
            "Using lua package.cpath=\"%s\"",
            modlua_global_cfg.pkg_cpath);
        lua_getfield(modlua_global_cfg.L, -1, "cpath");
        lua_pushstring(modlua_global_cfg.L, modlua_global_cfg.pkg_cpath);
        lua_setglobal(modlua_global_cfg.L, "cpath");
    }

    /* Hook to initialize the lua runtime with the connection.
     * There is a modlua_conn_fini_lua_runtime which is only registered
     * when the main configuration context is being closed. This ensures
     * that it is the last hook to fire relative to the various
     * Lua-implemented modules this module may created and register. */
    rc = ib_hook_conn_register(
        ib,
        conn_started_event,
        modlua_conn_init_lua_runtime,
        NULL);
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Failed to register conn_started_event hook: %s",
            ib_status_to_string(rc));
        return rc;
    }

    /* Initialize lock to protect making new lua threads. */
    modlua_global_cfg.L_lck = malloc(sizeof(*modlua_global_cfg.L_lck));
    if (modlua_global_cfg.L_lck == NULL) {
        ib_log_error(ib, "Failed to initialize lua global lock.");
        return IB_EALLOC;
    }
    rc = ib_lock_init(modlua_global_cfg.L_lck);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to initialize lua global lock.");
        return rc;
    }

    /* Set up rule support. */
    rc = rules_lua_init(ib, m, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

static ib_status_t modlua_context_close(ib_engine_t  *ib,
                                        ib_module_t  *m,
                                        ib_context_t *ctx,
                                        void         *cbdata)
{
    ib_status_t rc;

    /* Close of the main context signifies configuration finished. */
    if (ib_context_type(ctx) == IB_CTYPE_MAIN) {

        /* Register this callback after the main context is closed.
         * This allows it to be executed LAST allowing all the Lua
         * modules created during configuration to be executed first. */
        rc = ib_hook_conn_register(
            ib,
            conn_finished_event,
            modlua_conn_fini_lua_runtime,
            NULL);
        if (rc != IB_OK) {
            ib_log_error(
                ib,
                "Failed to register conn_finished_event hook: %s",
                ib_status_to_string(rc));
        }

        /* Commit any pending configuration items. */
        rc = modlua_commit_configuration(ib);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

static ib_status_t modlua_dir_commit_rules(
    ib_cfgparser_t *cp,
    const char *name,
    const ib_list_t *list,
    void *cbdata)
{
    return modlua_commit_configuration(cp->ib);
}

/* -- Module Configuration -- */

static IB_CFGMAP_INIT_STRUCTURE(modlua_config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".pkg_path",
        IB_FTYPE_NULSTR,
        modlua_cfg_t,
        pkg_path
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".pkg_cpath",
        IB_FTYPE_NULSTR,
        modlua_cfg_t,
        pkg_cpath
    ),

    IB_CFGMAP_INIT_LAST
};




/* -- Configuration Directives -- */

/**
 * Implements the LuaInclude directive.
 *
 * Use the common Lua Configuration stack to configure IronBee using Lua.
 *
 * @param[in] cp Configuration parser and state.
 * @param[in] name The directive.
 * @param[in] p1 The file to include.
 * @param[in] cbdata The callback data. NULL. None is needed.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC if an allocation cannot be performed, such as a Lua Stack.
 *   - IB_EOTHER if any other error is encountered.
 *   - IB_EINVAL if there is a Lua interpretation problem. This
 *               will almost always indicate a problem with the user's code
 *               and the user should examine their script.
 */
static ib_status_t modlua_dir_lua_include(ib_cfgparser_t *cp,
                                          const char *name,
                                          const char *p1,
                                          void *cbdata)
{
    ib_engine_t *ib = cp->ib;
    ib_status_t rc;
    int lua_rc;
    ib_core_cfg_t *corecfg = NULL;
    lua_State *L = modlua_global_cfg.L;

    rc = ib_context_module_config(ib_context_main(ib),
                                  ib_core_module(),
                                  (void *)&corecfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve core configuration.");
        lua_pop(L, lua_gettop(L));
        return rc;
    }

    lua_getglobal(L, "ibconfig");
    if ( ! lua_istable(L, -1) ) {
        ib_log_error(ib, "ibconfig is not a module table.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_getfield(L, -1, "include");
    if ( ! lua_isfunction(L, -1) ) {
        ib_log_error(ib, "ibconfig.include is not a function.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_pushlightuserdata(L, cp);
    lua_pushstring(L, p1);
    lua_rc = lua_pcall(L, 2, 1, 0);
    if (lua_rc == LUA_ERRFILE) {
        ib_log_error(ib, "Could not access file %s.", p1);
        ib_log_error(ib, "Configuration Error: %s", lua_tostring(L, -1));
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }
    else if (lua_rc) {
        ib_log_error(ib, "Configuration Error: %s", lua_tostring(L, -1));
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }
    else if (lua_tonumber(L, -1) != IB_OK) {
        rc = lua_tonumber(L, -1);
        lua_pop(L, lua_gettop(L));
        ib_log_error(
            ib,
            "Configuration error reported: %d:%s",
            rc,
            ib_status_to_string(rc));
        return IB_EOTHER;
    }

    lua_pop(L, lua_gettop(L));
    return IB_OK;
}

/**
 * @param[in] cp Configuration parser.
 * @param[in] name The name of the directive.
 * @param[in] p1 The argument to the directive parameter.
 * @param[in,out] cbdata Unused callback data.
 *
 * @returns
 *   - IB_OK
 *   - IB_EALLOC on memory error.
 *   - Others if engine registration or util functions fail.
 */
static ib_status_t modlua_dir_param1(ib_cfgparser_t *cp,
                                     const char *name,
                                     const char *p1,
                                     void *cbdata)
{
    ib_engine_t *ib = cp->ib;
    ib_status_t rc;
    ib_core_cfg_t *corecfg = NULL;
    size_t p1_len = strlen(p1);
    size_t p1_unescaped_len;
    char *p1_unescaped = malloc(p1_len+1);

    if ( p1_unescaped == NULL ) {
        return IB_EALLOC;
    }

    rc = ib_util_unescape_string(
        p1_unescaped,
        &p1_unescaped_len,
        p1,
        p1_len,
        IB_UTIL_UNESCAPE_NULTERMINATE |
        IB_UTIL_UNESCAPE_NONULL);
    if (rc != IB_OK) {
        const char *msg = (rc == IB_EBADVAL)?
            "Value for parameter \"%s\" may not contain NULL bytes: %s":
            "Value for parameter \"%s\" could not be unescaped: %s";
        ib_log_debug(ib, msg, name, p1);
        free(p1_unescaped);
        return rc;
    }

    rc = ib_context_module_config(ib_context_main(ib),
                                  ib_core_module(),
                                  (void *)&corecfg);

    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve core configuration.");
        return rc;
    }

    if (strcasecmp("LuaLoadModule", name) == 0) {
        /* Absolute path. */
        if (p1_unescaped[0] == '/') {
            rc = modlua_module_load(ib, p1_unescaped);
            if (rc != IB_OK) {
                ib_log_error(
                    ib,
                    "Failed to load Lua module with error %s: %s",
                    ib_status_to_string(rc),
                    p1_unescaped);
                return rc;
            }
        }
        else {
            char *path;

            /* Length of prefix, file name, and +1 for the joining '/'. */
            size_t path_len =
                strlen(corecfg->module_base_path) +
                strlen(p1_unescaped) +
                1;

            path = malloc(path_len+1);
            if (!path) {
                ib_log_error(ib, "Cannot allocate memory for module path.");
                free(p1_unescaped);
                return IB_EALLOC;
            }

            /* Build the full path. */
            snprintf(
                path,
                path_len+1,
                "%s/%s",
                corecfg->module_base_path,
                p1_unescaped);

            rc = modlua_module_load(ib, path);
            if (rc != IB_OK) {
                ib_log_error(
                    ib,
                    "Failed to load Lua module with error %s: %s",
                    ib_status_to_string(rc),
                    path);
                free(path);
                return rc;
            }
            free(path);
        }
    }
    else if (strcasecmp("LuaPackagePath", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);
        rc = ib_context_set_string(ctx, MODULE_NAME_STR ".pkg_path", p1_unescaped);
        free(p1_unescaped);
        return rc;
    }
    else if (strcasecmp("LuaPackageCPath", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);
        rc = ib_context_set_string(ctx, MODULE_NAME_STR ".pkg_cpath", p1_unescaped);
        free(p1_unescaped);
        return rc;
    }
    else {
        ib_log_error(ib, "Unhandled directive: %s %s", name, p1_unescaped);
        free(p1_unescaped);
        return IB_EINVAL;
    }

    free(p1_unescaped);
    return IB_OK;
}

static IB_DIRMAP_INIT_STRUCTURE(modlua_directive_map) = {
    IB_DIRMAP_INIT_PARAM1(
        "LuaLoadModule",
        modlua_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "LuaPackagePath",
        modlua_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "LuaPackageCPath",
        modlua_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "LuaInclude",
        modlua_dir_lua_include,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "LuaCommitRules",
        modlua_dir_commit_rules,
        NULL
    ),

    /* End */
    IB_DIRMAP_INIT_LAST
};

/**
 * Destroy global lock and Lua state.
 */
static ib_status_t modlua_fini(ib_engine_t *ib, ib_module_t *m, void *cbdata) {

    ib_lock_destroy(modlua_global_cfg.L_lck);
    free(modlua_global_cfg.L_lck);
    modlua_global_cfg.L_lck = NULL;

    lua_close(modlua_global_cfg.L);
    modlua_global_cfg.L = NULL;

    return IB_OK;
}

/* -- Module Definition -- */

/**
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /**< Default metadata */
    MODULE_NAME_STR,                     /**< Module name */
    IB_MODULE_CONFIG(&modlua_global_cfg),/**< Global config data */
    modlua_config_map,                   /**< Configuration field map */
    modlua_directive_map,                /**< Config directive map */
    modlua_init,                         /**< Initialize function */
    NULL,                                /**< Callback data */
    modlua_fini,                         /**< Finish function */
    NULL,                                /**< Callback data */
    NULL,                                /**< Context open function */
    NULL,                                /**< Callback data */
    modlua_context_close,                /**< Context close function */
    NULL,                                /**< Callback data */
    NULL,                                /**< Context destroy function */
    NULL                                 /**< Callback data */
);
