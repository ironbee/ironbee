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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

/**
 * A container to hold both ibmod_lua and a user-defined Lua module.
 *
 * This is used as callback data to event handlers that need to know
 * which user-defined module they were registered as, as well as,
 * which modules the ibmod_lua module was registered as.
 */
struct modlua_modules_t {
    /**
     * The Lua module pointer, not the pointer to a Lua-implemented module.
     *
     * This is used to retrieve shared runtimes and other global configuration.
     */
    ib_module_t *modlua;

    /**
     * Pointer to the Lua module created by the user. This represents Lua code.
     *
     * This is used when calling the Lua code to fetch configurations, etc.
     */
    ib_module_t *module;
};
typedef struct modlua_modules_t modlua_modules_t;

/**
 * Callback data for the modlua_luamod_init function.
 *
 * This is used to initialize Lua Modules.
 */
typedef struct modlua_luamod_init_t {
    const char  *file;       /**< Lua File to load. */
    /**
     * The Lua module. Not the user's module written in Lua.
     */
    ib_module_t  *modlua;
    modlua_cfg_t *modlua_cfg; /**< Configuration for modlua. */
} modlua_luamod_init_t;

/**
 * Push the specified handler for a lua module on top of the Lua stack L.
 *
 * @param[in] ib IronBee engine.
 * @param[in] modlua_modules Lua and lua-defined modules.
 * @param[in] event the even type.
 * @param[out] L the execution environment to modify.
 *
 * @returns
 *   - IB_OK on success. The stack is 1 element higher.
 *   - IB_EINVAL on a Lua runtime error.
 */
static ib_status_t modlua_push_lua_handler(
    ib_engine_t *ib,
    modlua_modules_t *modlua_modules,
    ib_state_event_type_t event,
    lua_State *L)
{
    assert(ib);
    assert(modlua_modules);
    assert(modlua_modules->module);
    assert(L);

    /* Use the user-defined lua module. Do not use ibmod_lua.so. */
    ib_module_t *module = modlua_modules->module;
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
                "Failed to fetch error message during module load of %s",
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
 * @param[in] event The event to check for.
 * @param[out] L The Lua state to push the dispatcher onto.
 *
 * @returns
 *   - IB_OK if a handler exists.
 *   - IB_ENOENT if a handler does not exist.
 *   - IB_EINVAL on a Lua runtime error. See log file for details.
 */
static ib_status_t modlua_push_dispatcher(
    ib_engine_t *ib,
    ib_state_event_type_t event,
    lua_State *L)
{
    assert(ib != NULL);
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
 * @param[in] ibmod_modules Lua and lua-defined modules.
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
    modlua_modules_t *ibmod_modules,
    ib_state_event_type_t event,
    lua_State *L)
{
    assert(ib != NULL);
    assert(ibmod_modules != NULL);
    assert(L != NULL);

    ib_status_t rc;

    rc = modlua_push_lua_handler(ib, ibmod_modules, event, L);

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
 *
 * @param[in] ib IronBee engine.
 * @param[in] ibmod_modules The lua module and the user's lua-defined module.
 * @param[in] L Lua runtime environment.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t modlua_callback_dispatch_base(
    ib_engine_t      *ib,
    modlua_modules_t *ibmod_modules,
    lua_State        *L)
{
    assert(ib != NULL);
    assert(ibmod_modules->modlua != NULL);
    assert(ibmod_modules->module != NULL);
    assert(L != NULL);

    ib_status_t rc;
    int lua_rc;
    ib_module_t *module = ibmod_modules->module; /* Lua-defined module. */

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
                "Failed to fetch error message during callback of %s",
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
 * @param[in] modlua_runtime Lua runtime.
 * @param[in] modlua_modules_cbdata The lua modules and the user's
 *            lua-defined module.
 *
 * @returns
 *   - The result of the module dispatch call.
 *   - IB_E* values may be returned as a result of an internal
 *     module failure. The log file should always be examined
 *     to determine if a Lua module failure or an internal
 *     error is to blame.
 */
static ib_status_t modlua_callback_dispatch(
    ib_engine_t           *ib,
    ib_state_event_type_t  event,
    ib_tx_t               *tx,
    ib_conn_t             *conn,
    modlua_runtime_t      *modlua_runtime,
    modlua_modules_t      *modlua_modules_cbdata
)
{
    assert(ib                            != NULL);
    assert(conn                          != NULL);
    assert(modlua_runtime                != NULL);
    assert(modlua_runtime->L             != NULL);
    assert(modlua_modules_cbdata         != NULL);
    assert(modlua_modules_cbdata->modlua != NULL);

    return modlua_callback_dispatch_base(
        ib,
        modlua_modules_cbdata,
        modlua_runtime->L);
}

/**
 * Dispatch a null event into a Lua module.
 *
 * @param[in] ib IronBee engine.
 * @param[in] event The event type.
 * @param[in] cbdata A pointer to a modlua_modules_t with the lua module
 *            and the user's lua-defined module struct in it.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t modlua_null(
    ib_engine_t           *ib,
    ib_state_event_type_t  event,
    void                  *cbdata
)
{
    assert(ib != NULL);
    assert(cbdata != NULL);

    ib_status_t       rc;
    ib_status_t       rc2;
    lua_State        *L              = NULL;
    ib_context_t     *ctx            = ib_context_main(ib);
    modlua_cfg_t     *cfg            = NULL;
    modlua_runtime_t *runtime        = NULL;
    modlua_modules_t *modlua_modules = (modlua_modules_t *)cbdata;

    assert(modlua_modules->modlua != NULL);
    assert(modlua_modules->module != NULL);

    rc = ib_context_module_config(ctx, modlua_modules->modlua, &cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve module configuration.");
        return rc;
    }

    rc = modlua_acquirestate(ib, cfg, &runtime);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to get a Lua runtime resource.");
        return rc;
    }

    L = runtime->L;

    rc = modlua_reload_ctx_except_main(ib, modlua_modules->modlua, ctx, L);
    if (rc != IB_OK) {
        modlua_releasestate(ib, cfg, runtime);
        ib_log_error(ib, "Failed to configure Lua stack.");
        goto exit;
    }

    /* Push Lua dispatch method to stack. */
    rc = modlua_push_dispatcher(ib, event, L);
    if (rc != IB_OK) {
        modlua_releasestate(ib, cfg, runtime);
        ib_log_error(ib, "Cannot push modlua.dispatch_handler to stack.");
        goto exit;
    }

    /* Push Lua handler onto the table. */
    rc = modlua_push_lua_handler(ib, modlua_modules, event, L);
    if (rc != IB_OK) {
        modlua_releasestate(ib, cfg, runtime);
        ib_log_error(ib, "Cannot push modlua event handler to stack.");
        goto exit;
    }

    lua_pushlightuserdata(L, ib);
    lua_pushlightuserdata(L, modlua_modules->module);
    lua_pushinteger(L, event);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot push modlua.config_path to stack.");
        goto exit;
    }
    lua_pushnil(L);                /* Connection (conn) is nil. */
    lua_pushnil(L);                /* Transaction (tx) is nil. */
    lua_pushlightuserdata(L, ctx); /* Configuration context. */

    rc = modlua_callback_dispatch_base(ib, modlua_modules, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failure while executing callback handler.");
        goto exit;
    }

exit:
    rc2 = modlua_releasestate(ib, cfg, runtime);
    if (rc2 != IB_OK) {
        ib_log_error(ib, "Failure while returning Lua runtime.");
        if (rc == IB_OK) {
            return rc2;
        }
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
 * @param[in] modlua_runtime Lua runtime.
 * @param[in] modlua_modules Both the lua module and the user's
 *            lua-defined module.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EOTHER on a Lua runtime error.
 */
static ib_status_t modlua_callback_setup(
    ib_engine_t           *ib,
    ib_state_event_type_t  event,
    ib_tx_t               *tx,
    ib_conn_t             *conn,
    modlua_runtime_t      *modlua_runtime,
    modlua_modules_t      *modlua_modules
)
{
    assert(ib                     != NULL);
    assert(conn                   != NULL);
    assert(modlua_runtime         != NULL);
    assert(modlua_runtime->L      != NULL);
    assert(modlua_modules         != NULL);
    assert(modlua_modules->module != NULL);

    /* Pick the best context to use. */
    ib_context_t *ctx = ib_context_get_context(ib, conn, tx);
    ib_status_t   rc;
    lua_State    *L   = modlua_runtime->L;

    /* Push Lua dispatch method to stack. */
    rc = modlua_push_dispatcher(ib, event, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot push modlua.dispatch_handler to stack.");
        return rc;
    }

    /* Push Lua handler onto the table. */
    rc = modlua_push_lua_handler(ib, modlua_modules, event, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot push modlua event handler to stack.");
        return rc;
    }

    lua_pushlightuserdata(L, ib);
    lua_pushlightuserdata(L, modlua_modules->module);
    lua_pushinteger(L, event);
    rc = modlua_push_config_path(ib, ctx, L);
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
    lua_pushlightuserdata(L, ctx);

    return IB_OK;
}


/**
 * Dispatch a connection event into a Lua module.
 *
 * @param[in] ib IronBee engine.
 * @param[in] conn Connection.
 * @param[in] event The event type.
 * @param[in] cbdata A modlua_modules_t containing the lua module and the
 *            user's lua-defined  module.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t modlua_conn(
    ib_engine_t *ib,
    ib_conn_t *conn,
    ib_state_event_type_t event,
    void *cbdata)
{
    assert(ib != NULL);
    assert(conn != NULL);
    assert(cbdata != NULL);

    ib_status_t       rc;
    ib_status_t       rc2;
    modlua_runtime_t *runtime;
    modlua_cfg_t     *cfg;

    modlua_modules_t *mod_cbdata = (modlua_modules_t *)cbdata;

    rc = ib_context_module_config(conn->ctx, mod_cbdata->modlua, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    rc = modlua_acquirestate(ib, cfg, &runtime);
    if (rc != IB_OK) {
        return rc;
    }

    rc = modlua_callback_setup(ib, event, NULL, conn, runtime, mod_cbdata);
    if (rc != IB_OK) {
        goto exit;
    }

    /* Custom table setup */

    rc = modlua_callback_dispatch(ib, event, NULL, conn, runtime, mod_cbdata);
    if (rc != IB_OK) {
        goto exit;
    }

exit:
    rc2 = modlua_releasestate(ib, cfg, runtime);
    if (rc2 != IB_OK) {
        ib_log_error(ib, "Failed to release Lua stack back to resource pool.");
        if (rc == IB_OK) {
            return rc2;
        }
    }

    return rc;
}

/**
 * Dispatch a transaction event into a Lua module.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx Transaction.
 * @param[in] event The event type.
 * @param[in] cbdata A modlua_modules_t containing the lua module and the
 *            user's lua-defined  module.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t modlua_tx(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    void *cbdata)
{
    assert(ib       != NULL);
    assert(tx       != NULL);
    assert(tx->ctx  != NULL);
    assert(tx->conn != NULL);
    assert(cbdata   != NULL);

    ib_status_t       rc;
    ib_status_t       rc2;
    modlua_runtime_t *runtime;
    modlua_cfg_t     *cfg;

    modlua_modules_t *mod_cbdata = (modlua_modules_t *)cbdata;

    rc = ib_context_module_config(tx->ctx, mod_cbdata->modlua, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    rc = modlua_acquirestate(ib, cfg, &runtime);
    if (rc != IB_OK) {
        return rc;
    }

    rc = modlua_callback_setup(ib, event, tx, tx->conn, runtime, mod_cbdata);
    if (rc != IB_OK) {
        goto exit;
    }

    /* Custom table setup */

    rc = modlua_callback_dispatch(ib, event, tx, tx->conn, runtime, mod_cbdata);
    if (rc != IB_OK) {
        goto exit;
    }

exit:
    rc2 = modlua_releasestate(ib, cfg, runtime);
    if (rc2 != IB_OK) {
        ib_log_error(ib, "Failed to release Lua stack back to resource pool.");
        if (rc == IB_OK) {
            return rc2;
        }
    }

    return rc;
}

/**
 * Dispatch a transaction data event into a Lua module.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx Transaction.
 * @param[in] event Event type.
 * @param[in] data Transaction data.
 * @param[in] data_length Transaction data length.
 * @param[in] cbdata A modlua_modules_t containing the lua module and the
 *            user's lua-defined  module.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t modlua_txdata(
    ib_engine_t           *ib,
    ib_tx_t               *tx,
    ib_state_event_type_t  event,
    const char            *data,
    size_t                 data_length,
    void                  *cbdata
)
{
    assert(ib       != NULL);
    assert(tx       != NULL);
    assert(tx->ctx  != NULL);
    assert(tx->conn != NULL);
    assert(cbdata   != NULL);

    ib_status_t       rc;
    ib_status_t       rc2;
    modlua_runtime_t *runtime;
    modlua_cfg_t     *cfg;

    modlua_modules_t *mod_cbdata = (modlua_modules_t *)cbdata;

    rc = ib_context_module_config(tx->ctx, mod_cbdata->modlua, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    rc = modlua_acquirestate(ib, cfg, &runtime);
    if (rc != IB_OK) {
        return rc;
    }

    rc = modlua_callback_setup(ib, event, tx, tx->conn, runtime, mod_cbdata);
    if (rc != IB_OK) {
        goto exit;
    }

    /* Custom table setup */

    rc = modlua_callback_dispatch(ib, event, tx, tx->conn, runtime, mod_cbdata);
    if (rc != IB_OK) {
        goto exit;
    }

exit:
    rc2 = modlua_releasestate(ib, cfg, runtime);
    if (rc2 != IB_OK) {
        ib_log_error(ib, "Failed to releases Lua stack back to resource pool.");
        if (rc == IB_OK) {
            return rc2;
        }
    }

    return rc;
}

/**
 * Dispatch a header callback hook.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx Transaction.
 * @param[in] event Event type.
 * @param[in] header Parsed header data.
 * @param[in] cbdata A modlua_modules_t containing the lua module and the
 *            user's lua-defined  module.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t modlua_header(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_parsed_header_t *header,
    void *cbdata)
{
    assert(ib       != NULL);
    assert(tx       != NULL);
    assert(tx->ctx  != NULL);
    assert(tx->conn != NULL);
    assert(cbdata   != NULL);

    ib_status_t       rc;
    ib_status_t       rc2;
    modlua_runtime_t *runtime;
    modlua_cfg_t     *cfg;

    modlua_modules_t *mod_cbdata = (modlua_modules_t *)cbdata;

    rc = ib_context_module_config(tx->ctx, mod_cbdata->modlua, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    rc = modlua_acquirestate(ib, cfg, &runtime);
    if (rc != IB_OK) {
        return rc;
    }

    rc = modlua_callback_setup(ib, event, tx, tx->conn, runtime, mod_cbdata);
    if (rc != IB_OK) {
        goto exit;
    }

    /* Custom table setup */

    rc = modlua_callback_dispatch(ib, event, tx, tx->conn, runtime, mod_cbdata);
    if (rc != IB_OK) {
        goto exit;
    }

exit:
    rc2 = modlua_releasestate(ib, cfg, runtime);
    if (rc2 != IB_OK) {
        ib_log_error(ib, "Failed to release Lua stack back to resource pool.");
        if (rc == IB_OK) {
            return rc2;
        }
    }

    return rc;
}

/**
 * Dispatch a request line callback hook.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx Transaction.
 * @param[in] event Event type.
 * @param[in] line Parsed request line.
 * @param[in] cbdata A modlua_modules_t containing the lua module and the
 *            user's lua-defined  module.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t modlua_reqline(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_parsed_req_line_t *line,
    void *cbdata)
{
    assert(ib       != NULL);
    assert(tx       != NULL);
    assert(tx->ctx  != NULL);
    assert(tx->conn != NULL);
    assert(cbdata   != NULL);

    ib_status_t       rc;
    ib_status_t       rc2;
    modlua_runtime_t *runtime;
    modlua_cfg_t     *cfg;

    modlua_modules_t *mod_cbdata = (modlua_modules_t *)cbdata;

    rc = ib_context_module_config(tx->ctx, mod_cbdata->modlua, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    rc = modlua_acquirestate(ib, cfg, &runtime);
    if (rc != IB_OK) {
        return rc;
    }

    rc = modlua_callback_setup(ib, event, tx, tx->conn, runtime, mod_cbdata);
    if (rc != IB_OK) {
        goto exit;
    }

    /* Custom table setup */

    rc = modlua_callback_dispatch(ib, event, tx, tx->conn, runtime, mod_cbdata);
    if (rc != IB_OK) {
        goto exit;
    }

exit:
    rc2 = modlua_releasestate(ib, cfg, runtime);
    if (rc2 != IB_OK) {
        ib_log_error(ib, "Failed to release Lua stack back to resource pool.");
        if (rc == IB_OK) {
            return rc2;
        }
    }

    return rc;
}

/**
 * Dispatch a response line callback hook.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx Transaction.
 * @param[in] event Event type.
 * @param[in] line The parsed response line.
 * @param[in] cbdata A modlua_modules_t containing the lua module and the
 *            user's lua-defined  module.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
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
    assert(cbdata != NULL);

    ib_status_t       rc;
    ib_status_t       rc2;
    modlua_runtime_t *runtime;
    modlua_cfg_t     *cfg;

    modlua_modules_t *mod_cbdata = (modlua_modules_t *)cbdata;

    rc = ib_context_module_config(tx->ctx, mod_cbdata->modlua, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    rc = modlua_acquirestate(ib, cfg, &runtime);
    if (rc != IB_OK) {
        return rc;
    }

    rc = modlua_callback_setup(ib, event, tx, tx->conn, runtime, mod_cbdata);
    if (rc != IB_OK) {
        goto exit;
    }

    /* Custom table setup */

    rc = modlua_callback_dispatch(ib, event, tx, tx->conn, runtime, mod_cbdata);
    if (rc != IB_OK) {
        goto exit;
    }

exit:
    rc2 = modlua_releasestate(ib, cfg, runtime);
    if (rc2 != IB_OK) {
        ib_log_error(ib, "Failed to release Lua stack back to resource pool.");
        if (rc == IB_OK) {
            return rc2;
        }
    }

    return rc;
}

/**
 * Dispatch a context event into a Lua module.
 *
 * @param[in] ib IronBee engine.
 * @param[in] ctx Context.
 * @param[in] event Event type.
 * @param[in] cbdata A modlua_modules_t containing the lua module and the
 *            user's lua-defined  module.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t modlua_ctx(
    ib_engine_t *ib,
    ib_context_t *ctx,
    ib_state_event_type_t event,
    void *cbdata)
{
    assert(ib);
    assert(ctx);
    assert(cbdata != NULL);

    ib_status_t       rc;
    ib_status_t       rc2;
    lua_State        *L;
    modlua_cfg_t     *cfg = NULL;
    modlua_runtime_t *runtime;

    modlua_modules_t *modlua_modules = (modlua_modules_t *)cbdata;
    assert(modlua_modules != NULL);
    assert(modlua_modules->modlua != NULL);
    assert(modlua_modules->module != NULL);

    rc = ib_context_module_config(ctx, modlua_modules->modlua, &cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve module configuration.");
        return rc;
    }

    rc = modlua_acquirestate(ib, cfg, &runtime);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to acquire Lua runtime.");
        return rc;
    }
    L = runtime->L;

    rc = modlua_reload_ctx_except_main(ib, modlua_modules->modlua, ctx, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to configure Lua stack.");
        goto exit;
    }

    /* Push Lua dispatch method to stack. */
    rc = modlua_push_dispatcher(ib, event, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot push modlua.dispatch_handler to stack.");
        goto exit;
    }

    /* Push Lua handler onto the table. */
    rc = modlua_push_lua_handler(ib, modlua_modules, event, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot push modlua event handler to stack.");
        goto exit;
    }

    /* Push handler arguments... */
    /* Push ib... */
    lua_pushlightuserdata(L, ib);             /* Push engine... */
    lua_pushlightuserdata(L, modlua_modules->module);
    lua_pushinteger(L, event);                /* Push event type... */
    rc = modlua_push_config_path(ib, ctx, L); /* Push ctx path table... */
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot push modlua.config_path to stack.");
        goto exit;
    }
    lua_pushnil(L);                /* Connection (conn) is nil... */
    lua_pushnil(L);                /* Transaction (tx) is nil... */
    lua_pushlightuserdata(L, ctx); /* Push configuration context. */

    rc = modlua_callback_dispatch_base(ib, modlua_modules, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failure while executing callback handler.");
        goto exit;
    }

exit:
    rc2 = modlua_releasestate(ib, cfg, runtime);
    if (rc2 != IB_OK) {
        ib_log_error(ib, "Failed to release Lua stack back to resource pool.");
        if (rc == IB_OK) {
            return rc2;
        }
    }

    return rc;
}

/**
 * Called by modlua_module_load to wire the callbacks in @a ib.
 *
 * @param[in] ib IronBee engine.
 * @param[in] modlua The Lua module. Not the user's Lua-implemented module.
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
    ib_module_t *modlua,
    const char  *file,
    ib_module_t *module,
    lua_State   *L
)
{

    assert(ib != NULL);
    assert(modlua != NULL);
    assert(file != NULL);
    assert(module != NULL);
    assert(L != NULL);

    ib_status_t rc;
    ib_mpool_t *mp;
    modlua_modules_t *ibmod_modules_cbdata = NULL;

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
    ibmod_modules_cbdata->modlua = modlua;
    ibmod_modules_cbdata->module = module;

    for (ib_state_event_type_t event = 0; event < IB_STATE_EVENT_NUM; ++event) {

        rc = module_has_callback(ib, ibmod_modules_cbdata, event, L);
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
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t modlua_luamod_init(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *cbdata
)
{
    assert(ib != NULL);
    assert(module != NULL);
    assert(cbdata != NULL);

    modlua_luamod_init_t *cfg = (modlua_luamod_init_t *)cbdata;

    /* Validate the passed along configuration. */
    assert(cfg->modlua != NULL);
    assert(cfg->modlua_cfg != NULL);
    assert(cfg->modlua_cfg->L != NULL);
    assert(cfg->file != NULL);

    ib_module_t          *modlua     = cfg->modlua;
    modlua_cfg_t         *modlua_cfg = cfg->modlua_cfg;
    lua_State            *L          = cfg->modlua_cfg->L;
    const char           *file       = cfg->file;
    ib_status_t           rc;

    /* Load the modules into the main Lua stack. Also register directives. */
    rc = modlua_module_config_lua(ib, file, module, L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to load lua modules: %s", file);
        return rc;
    }

    /* If the previous succeeds, record that we should reload it on each tx. */
    rc = modlua_record_reload(
        ib,
        modlua_cfg,
        MODLUA_RELOAD_MODULE,
        module,
        NULL,
        file);
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

/**
 * Load a Lua-defined module.
 *
 * @param[in] ib IronBee engine.
 * @param[in] modlua The ibmod_lua module structure.
 * @param[in] file The file that the user's lua-defined module resides in.
 * @param[in] cfg The module configuration for @a modlua.
 *
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If the file cannot be stat'ed.
 * - Other on error.
 */
ib_status_t modlua_module_load(
    ib_engine_t  *ib,
    ib_module_t  *modlua,
    const char   *module_name,
    const char   *file,
    modlua_cfg_t *cfg
)
{
    assert(ib != NULL);
    assert(modlua != NULL);
    assert(file != NULL);
    assert(cfg != NULL);
    assert(cfg->L != NULL);

    ib_module_t *module;
    ib_status_t  rc;
    ib_mpool_t  *mp          = ib_engine_pool_main_get(ib);
    modlua_luamod_init_t *modlua_luamod_init_cbdata =
        ib_mpool_alloc(mp, sizeof(*modlua_luamod_init_cbdata));
    int          sys_rc;
    struct stat  file_stat;

    /* Stat the file to avoid touching files that don't even exist. */
    sys_rc = stat(file, &file_stat);
    if (sys_rc == -1) {
        return IB_ENOENT;
    }

    if (modlua_luamod_init_cbdata == NULL) {
        return IB_EALLOC;
    }

    module_name = ib_mpool_strdup(mp, module_name);
    if (module_name == NULL) {
        return IB_EALLOC;
    }

    /* Create the Lua module as if it was a normal module. */
    rc = ib_module_create(&module, ib);
    if (rc != IB_OK) {
        ib_log_error(ib, "Cannot allocate module structure.");
        return rc;
    }

    modlua_luamod_init_cbdata->file       = file;
    modlua_luamod_init_cbdata->modlua     = modlua;
    modlua_luamod_init_cbdata->modlua_cfg = cfg;

    /* Initialize the loaded module. */
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
        modlua_luamod_init_cbdata,      /* Callback data */
        NULL,                           /* Finish function */
        NULL                            /* Callback data */
    );

    /* Initialize and register the new lua module with the engine. */
    rc = ib_module_register(module, ib);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to initialize / register a lua module.");
        return rc;
    }

    return rc;
}
