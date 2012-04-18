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

#include "ironbee_config_auto.h"

#include <string.h>

#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/module.h>

#include "ironbee.h"

/**
 * @internal
 *
 * Register the module table with ironbee.
 *
 * Currently a module MUST call this to register itself with the
 * ironbee engine.
 *
 * @todo Really need a way to not have to do this.
 *
 * EX:
 *   local modname = "example"
 *   local ironbee = require("ironbee")
 *   ...
 *   module(modname)
 *   ironbee.register_module(_M)
 *
 * Lua parameter stack:
 *  1) module table
 *
 * @param L Lua state
 * @retval 1 On success
 */
static int register_module(lua_State *L)
{
    if ((lua_gettop(L) != 1) || !lua_istable(L, 1)) {
        fprintf(stderr,
                "The register_module function takes a single table arg.\n");
        fflush(stderr);
        return 0;
    }

    /// @todo Right now, just set a global var until I figure out a better way.

    lua_pushvalue(L, 1);
    lua_setglobal(L, "ironbee-module");

    return 1;
}

/**
 * @internal
 *
 * Log to debug log
 *
 * Lua parameter stack:
 *  1) engine handle
 *  3) format
 *  4) ...
 *
 * @param L Lua state
 */
static int log_debug(lua_State *L)
{
    ib_engine_t *ib = (ib_engine_t *)lua_topointer(L, 1);
    int nargs = lua_gettop(L);
    const char *msg;
    int ec;

    /*
     * Call string.format() to do the actual formatting.
     *
     * This is done as lua cannot bind a vararg C function.  Instead,
     * this reorganizes the stack, replacing the "level" arg with the
     * format function, then calls string.format with the remaining args.
     * This allows string.format to do the formatting so that a single
     * string arg can be passed to the underlying ironbee log function.
     */
    /// @todo Store the format function for faster access???
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "format"); /* string.format() */
    lua_pop(L, 1); /* cleanup the stack used to find string table */
    ec = lua_pcall(L, (nargs - 2), 1, 0); /* format(fmt, ...) */
    if (ec != 0) {
        ib_log_error(ib, "Failed to exec string.format - %s (%d)",
                     lua_tostring(L, -1), ec);
        return 0;
    }
    msg = lua_tostring(L, -1); /* formatted string */

    /* Call the ironbee API with the formatted message. */
    ib_log_debug(ib, "%s", msg);

    return 1;
}

/**
 * @internal
 *
 * Map Lua methods to C functions.
 */
static const struct luaL_Reg ironbee_lib[] = {
    { "register_module", register_module },
    { "log_debug", log_debug },
    { NULL, NULL }
};

int luaopen_ironbee(lua_State *L)
{
    /* Create the module environment table for encapsulating
     * private (module wide) data.
     */
    lua_newtable(L);
    lua_replace(L, LUA_ENVIRONINDEX);

    luaL_register(L, IRONBEE_NAME, ironbee_lib);

    return 1;
}
