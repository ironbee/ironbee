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
 *
 * @defgroup LuaIbCUtil C Utilities
 * @ingroup Lua
 *
 * C Utilities for IronBee.
 *
 * @{
 */

#include <ironbee/field.h>

/* Lua module requirement. */
#define LUA_LIB
/* Lua module requirement. */
#include "lua.h"
/* Lua module requirement. */
#include "lauxlib.h"

#include <assert.h>

//! Module Name
static const char LUA_IBCUTIL_NAME[] = "ibcutil";

//! Module Version
static const char LUA_IBCUTIL_VERSION[] = "1.0";

//! Module Copyright
static const char LUA_IBCUTIL_COPYRIGHT[] =
    "Copyright (C) 2010-2014 Qualys, Inc.";

//! Module Description
static const char LUA_IBCUTIL_DESCRIPTION[] = "IronBee C Utilitie module.";


/**
 * Takes an `ib_float_t *` and a number; Converts number to ib_float_t.
 *
 * @note We choose not to type-check the first argument as we're not
 * sure what the final C99 type might be.
 *
 * @param[in] L Lua stack.
 * @returns the number of elements returned from the Lua call.
 */
LUALIB_API int to_ib_float(lua_State *L)
{
    lua_Number in;
    const void *out;

    if (!lua_isnumber(L, -1)) {
        lua_pushstring(L, "Second argument to to_ib_float() is not a number.");
        return lua_error(L);
    }

    in = lua_tonumber(L, -1);
    out = lua_topointer(L, -2);
    if (out == NULL) {
        lua_pushstring(L, "Out pointer in NULL.");
        return lua_error(L);
    }

    *(ib_float_t*)out = (ib_float_t)in;

    return 0;
}

/**
 * Take a @ref ib_float_t* on the stack and push a Lua number value.
 */
LUALIB_API int from_ib_float(lua_State *L)
{
    const void *in;
    lua_Number out;

    in = lua_topointer(L, -1);
    if (in == NULL) {
        lua_pushstring(L, "Input value is null.");
        return lua_error(L);
    }

    out = (lua_Number)*(ib_float_t*)in;

    lua_pushnumber(L, out);

    return 1;
}

/**
 * The table of mappings from Lua function names to C implementations.
 */
static const luaL_reg cutillib[] = {
    {"to_ib_float", to_ib_float },
    {"from_ib_float", from_ib_float },
    {NULL, NULL}
};

/**
 * Register the Lua bindings.
 *
 * This is called on load by Lua and, in turn, calls luaL_register().
 *
 * @code{.lua}
 * package.cpath = "my/path/?.so"
 * require "ibjson"
 * local t = ibjson.parse_string("{}")
 * @endcode
 *
 * @param[in] L The Lua Stack to load the module into.
 *
 * @returns 1 for success.
 */
LUALIB_API int luaopen_ibcutil(lua_State *L) {

    luaL_register(L, LUA_IBCUTIL_NAME, cutillib);
    assert(lua_istable(L, -1));

    lua_pushstring(L, LUA_IBCUTIL_VERSION);
    lua_setfield(L, -2, "_VERSION");

    lua_pushstring(L, LUA_IBCUTIL_COPYRIGHT);
    lua_setfield(L, -2, "_COPYRIGHT");

    lua_pushstring(L, LUA_IBCUTIL_DESCRIPTION);
    lua_setfield(L, -2, "_DESCRIPTION");

    return 1;
}

/**
 * @}
 */