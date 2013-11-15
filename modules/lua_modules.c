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

#include "lua_modules_private.h"
#include "lua_private.h"
#include "lua_runtime_private.h"

#include <ironbee/context.h>
#include <ironbee/engine_state.h>

#include <assert.h>

/**
 * Lua Module callback structure.
 */
struct modlua_modules_cbdata_t {
    /**
     * The Lua module pointer, not the pointer to a Lua-implemented module.
     */
    ib_module_t *lua_module;
};
typedef struct modlua_modules_cbdata_t modlua_modules_cbdata_t;

/**
 * Callback data for the modlua_luamod_init function.
 *
 * This is used to initialize Lua Modules.
 */
typedef struct modlua_luamod_init_t {
    lua_State   *L;          /**< Lua execution stack. */
    const char  *file;       /**< Lua File to load. */
    /**
     * The Lua module. Not the user's module written in Lua.
     */
    ib_module_t *modlua;
    modlua_cfg_t *modlua_cfg; /**< Configuration for lua_module. */
} modlua_luamod_init_t;

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
    assert(ib != NULL);
    assert(module != NULL);
    assert(L != NULL);

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
    assert(ib != NULL);
    assert(module != NULL);
    assert(L != NULL);

    ib_status_t rc;

    rc = modlua_push_lua_handler(ib, module, event, L);

    /* Pop the lua handler off the stack. We're just checking for it. */
    lua_pop(L, 1);

    return rc;
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
    assert(ib != NULL);
    assert(module != NULL);
    assert(L != NULL);

    ib_status_t rc;
    int lua_rc;

    ib_log_debug(ib, "Calling handler for lua module: %s", module->name);

    /* Run dispatcher. */
    lua_rc = lua_pcall(L, 8, 1, 0);
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
    assert(ib != NULL);
    assert(conn != NULL);

    ib_status_t rc;
    ib_module_t *module = NULL;
    lua_State *L;
    modlua_runtime_t *lua = NULL;
    modlua_modules_cbdata_t *modlua_modules_cbdata =
        (modlua_modules_cbdata_t *)cbdata;

    assert(modlua_modules_cbdata != NULL);
    assert(modlua_modules_cbdata->lua_module != NULL);
    module = modlua_modules_cbdata->lua_module;

    rc = modlua_runtime_get(conn, module, &lua);
    if (rc != IB_OK) {
        return rc;
    }

    L = lua->L;

    return modlua_callback_dispatch_base(ib, module, L);
}

/**
 * Dispatch a null event into a Lua module.
 */
static ib_status_t modlua_null(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    void *cbdata)
{
    assert(ib);

    ib_status_t rc;
    lua_State *L = NULL;
    ib_module_t *module = NULL;
    ib_context_t *ctx;
    modlua_cfg_t *cfg = NULL;
    modlua_runtime_t *runtime = NULL;
    modlua_modules_cbdata_t *modlua_modules_cbdata =
        (modlua_modules_cbdata_t *)cbdata;

    assert(modlua_modules_cbdata != NULL);
    assert(modlua_modules_cbdata->lua_module != NULL);
    module = modlua_modules_cbdata->lua_module;

    ctx = ib_context_main(ib);

    rc = ib_context_module_config(ctx, module, &cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not retrieve module configuration.");
        return rc;
    }

    rc = modlua_acquirestate(ib, cfg, &runtime);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not get a Lua runtime resource.");
        return rc;
    }

    L = runtime->L;

    rc = modlua_reload_ctx_except_main(ib, module, ctx, L);
    if (rc != IB_OK) {
        modlua_releasestate(ib, cfg, runtime);
        ib_log_error(ib, "Could not configure Lua stack.");
        return rc;
    }

    /* Push Lua dispatch method to stack. */
    rc = modlua_push_dispatcher(ib, module, event, L);
    if (rc != IB_OK) {
        modlua_releasestate(ib, cfg, runtime);
        ib_log_error(ib, "Cannot push modlua.dispatch_handler to stack.");
        return rc;
    }

    /* Push Lua handler onto the table. */
    rc = modlua_push_lua_handler(ib, module, event, L);
    if (rc != IB_OK) {
        modlua_releasestate(ib, cfg, runtime);
        ib_log_error(ib, "Cannot push modlua event handler to stack.");
        return rc;
    }

    lua_pushlightuserdata(L, ib);
    lua_pushlightuserdata(L, module);
    lua_pushinteger(L, event);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        modlua_releasestate(ib, cfg, runtime);
        ib_log_error(ib, "Cannot push modlua.config_path to stack.");
        return rc;
    }
    lua_pushnil(L);                /* Connection (conn) is nil. */
    lua_pushnil(L);                /* Transaction (tx) is nil. */
    lua_pushlightuserdata(L, ctx); /* Configuration context. */

    rc = modlua_callback_dispatch_base(ib, module, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failure while executing callback handler.");
        /* Do not return. We must join the Lua thread. */
    }

    rc = modlua_releasestate(ib, cfg, runtime);
    if (rc != IB_OK) {
        return rc;
    }

    return rc;
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
    assert(ib != NULL);
    assert(conn != NULL);

    ib_status_t rc;
    ib_module_t *module;
    lua_State *L = NULL;
    modlua_runtime_t *lua = NULL;
    modlua_modules_cbdata_t *modlua_modules_cbdata =
        (modlua_modules_cbdata_t *)cbdata;

    assert(modlua_modules_cbdata != NULL);
    assert(modlua_modules_cbdata->lua_module != NULL);
    module = modlua_modules_cbdata->lua_module;

    rc = modlua_runtime_get(conn, module, &lua);
    if (rc != IB_OK) {
        return rc;
    }

    if (lua == NULL) {
        ib_log_error(
            ib,
            "No module configuration data found. Cannot retrieve Lua stack.");
        return IB_EOTHER;
    }
    if (lua->L == NULL) {
        ib_log_error(
            ib,
            "No Lua stack found in module data. Cannot retrieve Lua stack.");
        return IB_EOTHER;
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
    lua_pushinteger(L, event);
    rc = modlua_push_config_path(ib, conn->ctx, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to push configuration path onto Lua stack.");
        return rc;
    }
    /* Push connection. */
    lua_pushlightuserdata(L, conn);
    /* Push transaction. */
    if (tx) {
        lua_pushlightuserdata(L, tx);
    }
    else {
        lua_pushnil(L);
    }
    /* Push configuration context used in conn. */
    lua_pushlightuserdata(L, conn->ctx);

    return IB_OK;
}


/**
 * Dispatch a connection event into a Lua module.
 */
static ib_status_t modlua_conn(
    ib_engine_t *ib,
    ib_conn_t *conn,
    ib_state_event_type_t event,
    void *cbdata)
{
    assert(ib);
    assert(conn);

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
 * Dispatch a transaction event into a Lua module.
 */
static ib_status_t modlua_tx(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    void *cbdata)
{
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
 * Dispatch a transaction data event into a Lua module.
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
 * Dispatch a header callback hook.
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
 * Dispatch a request line callback hook.
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
 * Dispatch a response line callback hook.
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
 * Dispatch a context event into a Lua module.
 */
static ib_status_t modlua_ctx(
    ib_engine_t *ib,
    ib_context_t *ctx,
    ib_state_event_type_t event,
    void *cbdata)
{
    assert(ctx);
    assert(ib);

    ib_status_t rc;
    lua_State *L;
    modlua_cfg_t *cfg = NULL;
    ib_module_t *module = NULL;
    modlua_runtime_t *runtime;

    modlua_modules_cbdata_t *modlua_modules_cbdata =
        (modlua_modules_cbdata_t *)cbdata;

    assert(modlua_modules_cbdata != NULL);
    assert(modlua_modules_cbdata->lua_module != NULL);
    module = modlua_modules_cbdata->lua_module;

    rc = ib_context_module_config(ctx, module, &cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not retrieve module configuration.");
        return rc;
    }

    rc = modlua_acquirestate(ib, cfg, &runtime);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to acquire Lua runtime.");
        return rc;
    }
    L = runtime->L;

    rc = modlua_reload_ctx_except_main(ib, module, ctx, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not configure Lua stack.");
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

    /* Push handler arguments... */
    /* Push ib... */
    lua_pushlightuserdata(L, ib);             /* Push engine... */
    lua_pushlightuserdata(L, module);         /* Push module... */
    lua_pushinteger(L, event);                /* Push event type... */
    rc = modlua_push_config_path(ib, ctx, L); /* Push ctx path table... */
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot push modlua.config_path to stack.");
        return rc;
    }
    lua_pushnil(L);                /* Connection (conn) is nil... */
    lua_pushnil(L);                /* Transaction (tx) is nil... */
    lua_pushlightuserdata(L, ctx); /* Push configuration context. */

    rc = modlua_callback_dispatch_base(ib, module, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failure while executing callback handler.");
        /* Do not return. We must join the Lua thread. */
    }

    rc = modlua_releasestate(ib, cfg, runtime);

    return rc;
}

/**
 * Called by modlua_module_load to wire the callbacks in @a ib.
 *
 * @param[in] ib IronBee engine.
 * @param[in] lua_module The Lua module. Not the user's Lua-implemented module.
 * @param[in] file The file we are loading.
 * @param[in] module The Lua-implemented registered module structure.
 * @param[in,out] L The lua context that @a file will be loaded into as
 *                @a module.
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC on allocation errors.
 *   - IB_EOTHER on unexpected errors.
 */
static ib_status_t modlua_module_load_wire_callbacks(
    ib_engine_t *ib,
    ib_module_t *lua_module,
    const char *file,
    ib_module_t *module,
    lua_State *L)
{

    assert(ib != NULL);
    assert(lua_module != NULL);
    assert(file != NULL);
    assert(module != NULL);
    assert(L != NULL);

    ib_status_t rc;
    ib_mpool_t *mp;
    modlua_modules_cbdata_t *ibmod_modules_cbdata = NULL;

    mp = ib_engine_pool_main_get(ib);
    if (mp == NULL) {
        ib_log_error(
            ib,
            "Failed to fetch main engine memory pool for Lua module: %s",
            file);
        return IB_EOTHER;
    }

    ibmod_modules_cbdata =
        ib_mpool_calloc(mp, 1, sizeof(*ibmod_modules_cbdata));
    if (ibmod_modules_cbdata == NULL) {
        ib_log_error(ib, "Failed to allocate callback data.");
        return IB_EALLOC;
    }
    ibmod_modules_cbdata->lua_module = lua_module;

    for (ib_state_event_type_t event = 0; event < IB_STATE_EVENT_NUM; ++event) {

        rc = module_has_callback(ib, module, event, L);
        if (rc == IB_OK) {
            switch(ib_state_hook_type(event)) {
                case IB_STATE_HOOK_NULL:
                    rc = ib_hook_null_register(
                        ib,
                        event,
                        modlua_null,
                        ibmod_modules_cbdata);
                    break;
                case IB_STATE_HOOK_INVALID:
                    ib_log_error(ib, "Invalid hook: %d", event);
                    break;
                case IB_STATE_HOOK_CTX:
                    rc = ib_hook_context_register(
                        ib,
                        event,
                        modlua_ctx,
                        ibmod_modules_cbdata);
                    break;
                case IB_STATE_HOOK_CONN:
                    rc = ib_hook_conn_register(
                        ib,
                        event,
                        modlua_conn,
                        ibmod_modules_cbdata);
                    break;
                case IB_STATE_HOOK_TX:
                    rc = ib_hook_tx_register(
                        ib,
                        event,
                        modlua_tx,
                        ibmod_modules_cbdata);
                    break;
                case IB_STATE_HOOK_TXDATA:
                    rc = ib_hook_txdata_register(
                        ib,
                        event,
                        modlua_txdata,
                        ibmod_modules_cbdata);
                    break;
                case IB_STATE_HOOK_REQLINE:
                    rc = ib_hook_parsed_req_line_register(
                        ib,
                        event,
                        modlua_reqline,
                        ibmod_modules_cbdata);
                    break;
                case IB_STATE_HOOK_RESPLINE:
                    rc = ib_hook_parsed_resp_line_register(
                        ib,
                        event,
                        modlua_respline,
                        ibmod_modules_cbdata);
                    break;
                case IB_STATE_HOOK_HEADER:
                    rc = ib_hook_parsed_header_data_register(
                        ib,
                        event,
                        modlua_header,
                        ibmod_modules_cbdata);
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
 * Initialize a dynamically created Lua module.
 *
 * @param[in] ib IronBee engine.
 * @param[in] module The created module structure.
 * @param[in] cbdata Other variables required for proper initialization
 *            not provided for in the module init api.
 */
static ib_status_t modlua_luamod_init(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *cbdata
)
{
    modlua_luamod_init_t *cfg = (modlua_luamod_init_t *)cbdata;

    lua_State    *L          = cfg->L;
    ib_module_t  *modlua     = cfg->modlua;
    modlua_cfg_t *modlua_cfg = cfg->modlua_cfg;
    const char   *file       = cfg->file;
    ib_status_t   rc;

    /* Load the modules into the main Lua stack. Also register directives. */
    rc = modlua_module_load_lua(ib, true, file, module, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to load lua modules: %s", file);
        return rc;
    }

    /* If the previous succeeds, record that we should reload it on each tx. */
    rc = modlua_record_reload(ib, modlua_cfg, MODLUA_RELOAD_MODULE, NULL, file);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to record module file name to reload.");
        return rc;
    }

    /* Write up the callbacks. */
    rc = modlua_module_load_wire_callbacks(ib, modlua, file, module, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed register lua callbacks for module : %s", file);
        return rc;
    }


    return IB_OK;
}

ib_status_t modlua_module_load(
    ib_engine_t *ib,
    ib_module_t *lua_module,
    const char *file,
    modlua_cfg_t *cfg)
{
    assert(ib != NULL);
    assert(lua_module != NULL);
    assert(file != NULL);
    assert(cfg != NULL);
    assert(cfg->L != NULL);

    lua_State   *L = cfg->L;
    ib_module_t *module;
    ib_status_t  rc;
    ib_mpool_t  *mp = ib_engine_pool_main_get(ib);
    const char  *module_name = ib_mpool_strdup(mp, file);
    modlua_luamod_init_t *modlua_luamod_init_cbdata;

    /* Create the Lua module as if it was a normal module. */
    ib_log_debug3(ib, "Creating lua module structure");
    rc = ib_module_create(&module, ib);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot allocate module structure.");
        return rc;
    }

    modlua_luamod_init_cbdata =
        ib_mpool_alloc(mp, sizeof(*modlua_luamod_init_cbdata));

    modlua_luamod_init_cbdata->L    = L;
    modlua_luamod_init_cbdata->file = file;
    modlua_luamod_init_cbdata->modlua = lua_module;
    modlua_luamod_init_cbdata->modlua_cfg = cfg;

    /* Initialize the loaded module. */
    ib_log_debug3(ib, "Init lua module structure");
    IB_MODULE_INIT_DYNAMIC(
        module,                         /* Module */
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
        modlua_luamod_init,             /* Initialize function */
        modlua_luamod_init_cbdata,     /* Callback data */
        NULL,                           /* Finish function */
        NULL                            /* Callback data */
    );

    /* Initialize and register the new lua module with the engine. */
    ib_log_debug3(ib, "Init lua module");
    rc = ib_module_register(module, ib);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to initialize / register a lua module.");
        return rc;
    }

    ib_log_debug3(ib, "Lua module created.");

    return rc;
}
