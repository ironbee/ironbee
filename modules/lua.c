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
 * Callback type for functions executed protected by g_lua_lock.
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

    ib_conn_get_data(conn, module, (void **)lua);

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

    ib_conn_set_data(conn, module, lua);

    return IB_OK;
}

/**
 * This will use g_lua_lock to atomically call @a fn.
 *
 * The argument @a fn will be either
 * ib_lua_new_thread() or ib_lua_join_thread() which will be called
 * only if g_lua_lock can be locked using @c semop.
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

    lua_pushstring(L, "get_callback"); /* Push load_module */
    lua_gettable(L, -2);               /* Pop "load_module" and get func. */
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

    lua_pushlightuserdata(L, ib);
    lua_pushnumber(L, module->idx);
    lua_pushnumber(L, event);
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

    lua_pushstring(L, "dispatch_module"); /* Push load_module */
    lua_gettable(L, -2);               /* Pop "load_module" and get func. */
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
    lua_pushlightuserdata(L, module);
    lua_pushnumber(L, event);
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
    int lua_rc;

    modlua_lua_cbdata = (modlua_lua_cbdata_t *)cbdata;
    module = modlua_lua_cbdata->module;

    rc = modlua_runtime_get(conn, &lua);
    if (rc != IB_OK) {
        return rc;
    }

    L = lua->L;

    ib_log_debug(ib, "Calling handler for lua module: %s", module->name);

    /* Run dispatcher. */
    lua_rc = lua_pcall(L, 6, 1, 0);
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
 * Null callback hook.
 */
static ib_status_t modlua_null(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    void *cbdata)
{
    /* Cannot implement this until we find a way to get a Lua stack. */

    // FIXME - allocate *L

    ib_log_warning(ib, "NULL callback not implemented for Lua modules yet.");

    // FIXME - deallocate *L
    return IB_OK;
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
 * Connection data callback hook.
 */
static ib_status_t modlua_conndata(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    ib_conndata_t *conndata,
    void *cbdata)
{
    assert(ib);
    assert(conndata);
    assert(conndata->conn);
    assert(cbdata);

    ib_status_t rc;

    rc = modlua_callback_setup(ib, event, NULL, conndata->conn, cbdata);
    if (rc != IB_OK) {
        return rc;
    }

    /* Custom table setup */

    rc = modlua_callback_dispatch(ib, event, NULL, conndata->conn, cbdata);
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

    lua_pushstring(L, "load_module"); /* Push load_module */
    lua_gettable(L, -2);              /* Pop "load_module" and get func. */
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
    lua_pushinteger(L, module->idx);
    lua_pushstring(L, file);
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
     * +------------------------+
     * | load_module            |
     * | ib                     |
     * | module index           |
     * | module name (file name |
     * | module script          |
     * +------------------------+
     *
     * Next step is to call load_module which will, in turn, execute
     * the module script.
     */
    lua_rc = lua_pcall(L, 4, 1, 0);
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
                case IB_STATE_HOOK_CONNDATA:
                    rc = ib_hook_conndata_register(
                        ib,
                        event,
                        modlua_conndata,
                        cbdata);
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
 * Pre-load files into the given lua stack.
 *
 * This will attempt to run...
 *   - ffi     = require("ffi")
 *   - ironbee = require("ironbee-ffi")
 *   - ibapi   = require("ironbee-api")
 *   - modlua  = require("ironbee-modlua")
 *
 * @param[in] ib IronBee engine. Used to find load paths from the
 *            core module.
 * @param[out] L The Lua state that the modules will be "required" into.
 */
static ib_status_t modlua_preload(ib_engine_t *ib, lua_State *L) {

    ib_status_t rc;

    ib_core_cfg_t *corecfg = NULL;

    /* This is the search pattern that is appended to each element of
     * lua_search_paths and then added to the Lua runtime package.path
     * global variable. */
    const char *lua_file_pattern = "?.lua";

    /* Null terminated list of search paths. */
    const char *lua_search_paths[3];

    const char *lua_preloads[][2] = { { "ffi", "ffi" },
                                      { "ironbee", "ironbee-ffi" },
                                      { "ibapi", "ironbee-api" },
                                      { "modlua", "ironbee-modlua" },
                                      { NULL, NULL } };
    char *path = NULL; /* Tmp string to build a search path. */
    int i = 0; /* An iterator. */

    rc = ib_context_module_config(ib_context_main(ib),
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

    for (i = 0; lua_search_paths[i] != NULL; ++i)
    {
        char *tmp;
        ib_log_debug(ib,
            "Adding \"%s\" to lua search path.", lua_search_paths[i]);

        /* Strlen + 2. One for \0 and 1 for the path separator. */
        tmp = realloc(path,
                      strlen(lua_search_paths[i]) +
                      strlen(lua_file_pattern) + 2);

        if (tmp == NULL) {
            ib_log_error(ib, "Could allocate buffer for string append.");
            free(path);
            return IB_EALLOC;
        }
        path = tmp;

        strcpy(path, lua_search_paths[i]);
        strcpy(path + strlen(path), "/");
        strcpy(path + strlen(path), lua_file_pattern);

        ib_lua_add_require_path(ib, L, path);

        ib_log_debug(ib, "Added \"%s\" to lua search path.", path);
    }

    /* We are done with path. To be safe, we NULL it as there is more work
     * to be done in this function, and we do not want to touch path again. */
    free(path);
    path = NULL;

    for (i = 0; lua_preloads[i][0] != NULL; ++i)
    {
        rc = ib_lua_require(ib,
                            L,
                            lua_preloads[i][0],
                            lua_preloads[i][1]);
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

/* -- Module Routines -- */

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
    }

    /* Initialize lock to protect making new lua threads. */
    /* NOTE: To avoid any confusion as to whether the lock
     *       is thread-safe or all copies of the lock structure
     *       are thread safe we use a pointer to a lock structure
     *       so that all copies of the module configuration use
     *       the same lock structure in memory. */
    modlua_global_cfg.L_lck = malloc(sizeof(*modlua_global_cfg.L_lck));
    if (modlua_global_cfg.L_lck == NULL) {
        ib_log_error(ib, "Failed to initialize lua global lock.");
        return IB_EALLOC;
    }
    rc = ib_lock_init(modlua_global_cfg.L_lck);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to initialize lua global lock.");
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
    }

    return IB_OK;
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
