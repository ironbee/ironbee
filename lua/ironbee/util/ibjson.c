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
 * @defgroup LuaIbJson JSON Bindings
 * @ingroup Lua
 *
 * Bindings of IronBee's YAJL library to a Lua module.
 *
 * @{
 */

/**
 * @file
 *
 * Bindings to IronBee's JSON services.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

/* Lua module requirement. */
#define LUA_LIB
/* Lua module requirement. */
#include "lua.h"
/* Lua module requirement. */
#include "lauxlib.h"

#include <ironbee/mpool.h>

#include <assert.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif
#include <yajl/yajl_tree.h>
#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

//! Module Name
static const char LUA_IBJSONLIB_NAME[] = "ibjson";

//! Module Version
static const char LUA_IBJSONLIB_VERSION[] = "1.0";

//! Module Copyright
static const char LUA_IBJSON_COPYRIGHT[] =
    "Copyright (C) 2010-2014 Qualys, Inc.";

//! Module Description
static const char LUA_IBJSON_DESCRIPTION[] = "IronBee JSON Interface.";

/**
 * Forward declaration of callback data for parsing.
 * This is used as the Yajl @c ctx value.
 */
typedef struct ibjson_cbdata_t ibjson_cbdata_t;

/**
 * Callback that pushes (in a parsing sense, not a Lua stack sense) values.
 *
 * When parsing JSON different values may be assigned differently.
 * For example, a number parsed after a map key should be assigned to a
 * table.
 *
 * @param[in] cbdata Callback data.
 */
typedef void (*ibjson_push_fn_t)(ibjson_cbdata_t *cbdata);

/**
 * Callback data passed around a Yajl parse.
 */
struct ibjson_cbdata_t
{
    ib_mpool_t *mp;           /**< Memory pool. */
    lua_State *L;             /**< Lua stack. */
    ibjson_push_fn_t push_fn; /**< Current function for pushing value. */
};

/**
 * Nop.
 *
 * @param[in] cbdata Callback data.
 */
static void ibjson_pushval(ibjson_cbdata_t *cbdata)
{
    /* Nop. The value on the stack is final. */
}

/**
 * Append a value to the Lua list at index -2 of the Lua stack.
 *
 * @param[in] cbdata Callback data.
 */
static void ibjson_pushlist(ibjson_cbdata_t *cbdata)
{
    int i = lua_objlen(cbdata->L, -2) + 1;

    /* Copy the value to the top of the stack. */
    lua_pushvalue(cbdata->L, -1);

    /* Push the index to assign it to. */
    lua_pushinteger(cbdata->L, i);

    lua_replace(cbdata->L, -3);

    lua_settable(cbdata->L, -3);
}

/**
 * Assign value at -1 of the Lua stack to the key at -2 into the table at -3.
 *
 * @param[in] cbdata Callback data.
 */
static void ibjson_pushmap(ibjson_cbdata_t *cbdata)
{
    lua_settable(cbdata->L, -3);
}


/*****************************************************************************
 * YAJL callback definitions.
 ****************************************************************************/

static int yajl_boolean(void *ctx, int boolVal)
{
    ibjson_cbdata_t *cbdata = (ibjson_cbdata_t *)ctx;

    lua_pushboolean(cbdata->L, boolVal);

    (cbdata->push_fn)(cbdata);

    return 1;
}

static int yajl_double(void *ctx, double doubleVal)
{
    ibjson_cbdata_t *cbdata = (ibjson_cbdata_t *)ctx;

    lua_pushnumber(cbdata->L, doubleVal);

    (cbdata->push_fn)(cbdata);

    return 1;
}

static int yajl_end_array(void *ctx)
{
    ibjson_cbdata_t *cbdata = (ibjson_cbdata_t *)ctx;

    /* Restore the previous C function. */
    cbdata->push_fn = (ibjson_push_fn_t)lua_tocfunction(cbdata->L, -2);

    /* Remove it the C function from the stack. */
    lua_remove(cbdata->L, -2);

    /* Handle final assignment. */
    (cbdata->push_fn)(cbdata);

    return 1;
}

static int yajl_end_map(void *ctx)
{
    ibjson_cbdata_t *cbdata = (ibjson_cbdata_t *)ctx;

    /* Restore the previous C function. */
    cbdata->push_fn = (ibjson_push_fn_t)lua_tocfunction(cbdata->L, -2);

    /* Remove it the C function from the stack. */
    lua_remove(cbdata->L, -2);

    /* Handle final assignment. */
    (cbdata->push_fn)(cbdata);

    return 1;
}

static int yajl_integer(void *ctx, long long integerVal)
{
    ibjson_cbdata_t *cbdata = (ibjson_cbdata_t *)ctx;

    lua_pushinteger(cbdata->L, integerVal);

    (cbdata->push_fn)(cbdata);

    return 1;
}

static int yajl_map_key(void *ctx, const unsigned char *key, size_t stringLen)
{
    ibjson_cbdata_t *cbdata = (ibjson_cbdata_t *)ctx;

    lua_pushlstring(cbdata->L, (const char *)key, stringLen);

    return 1;
}

static int yajl_null(void *ctx)
{
    ibjson_cbdata_t *cbdata = (ibjson_cbdata_t *)ctx;

    lua_pushnil(cbdata->L);

    (cbdata->push_fn)(cbdata);

    return 1;
}

static int yajl_number(void *ctx, const char *numberVal, size_t numberLen)
{
    ibjson_cbdata_t *cbdata = (ibjson_cbdata_t *)ctx;

    lua_pushlstring(cbdata->L, (const char *)numberVal, numberLen);

    (cbdata->push_fn)(cbdata);

    return 1;
}

static int yajl_start_array(void *ctx)
{
    ibjson_cbdata_t *cbdata = (ibjson_cbdata_t *)ctx;

    lua_pushcfunction(cbdata->L, (lua_CFunction)cbdata->push_fn);

    lua_newtable(cbdata->L);

    cbdata->push_fn = ibjson_pushlist;

    return 1;
}

static int yajl_start_map(void *ctx)
{
    ibjson_cbdata_t *cbdata = (ibjson_cbdata_t *)ctx;

    lua_pushcfunction(cbdata->L, (lua_CFunction)cbdata->push_fn);

    lua_newtable(cbdata->L);

    cbdata->push_fn = ibjson_pushmap;

    return 1;
}

static int yajl_string(
    void *ctx,
    const unsigned char *stringVal,
    size_t stringLen
)
{
    ibjson_cbdata_t *cbdata = (ibjson_cbdata_t *)ctx;

    lua_pushlstring(cbdata->L, (const char *)stringVal, stringLen);

    (cbdata->push_fn)(cbdata);

    return 1;
}

static const yajl_callbacks g_yajl_callbacks = {
    .yajl_boolean = yajl_boolean,
    .yajl_double = yajl_double,
    .yajl_end_array = yajl_end_array,
    .yajl_end_map = yajl_end_map,
    .yajl_integer = yajl_integer,
    .yajl_map_key = yajl_map_key,
    .yajl_null = yajl_null,
    .yajl_number = yajl_number,
    .yajl_start_map = yajl_start_map,
    .yajl_string = yajl_string,
    .yajl_start_array = yajl_start_array,
};

/*****************************************************************************
 * End YAJL callback definitions.
 ****************************************************************************/

/**
 * Parse a string and push the results onto the Lua stack.
 *
 * This parses using YAJL, a callback-based parser. As such, the parsing
 * logic is distributed across all the callbacks. This documentation
 * unites that documentation.
 *
 * When a value that is not a map or a list is parsed, it is
 * pushed onto the Lua stack (@c lua_pushX(L, x)), but then
 * a second "push" function is called which is of type @ref ibjson_push_fn_t.
 *
 * The second push function takes the value at the top of the Lua stack
 * and, if a JSON map is being parsed, pushes the new value into the JSON map,
 * if a JSON list is being parsed, pushes the new value into the JSON list.
 *
 * When a map or list is parsed first, the previous @ref ibjson_push_fn_t
 * is pushed to the Lua stack and ibjson_pushmap() or ibjson_pushlist() is
 * set as the current push function.  Then the empty map or list is pushed
 * and parsing continues as described above.
 *
 * When a map or list is signaled as complete, the old @ref ibjson_push_fn_t
 * is pulled off the stack from below the newly constructed map or list
 * and made the current push function. The new map or list remains at the
 * top of the stack and is pushed as normal by the now-current
 * @ref ibjson_push_fn_t.
 *
 * @param[in] L Lua stack.
 * @returns the number of elements returned from the Lua call.
 */
LUALIB_API int ibjson_parse_string(lua_State *L) {

    ibjson_cbdata_t cbdata;
    ib_mpool_t *mp;
    ib_status_t rc; /* Return code. */
    yajl_status yc; /* Yajl return code. */
    const unsigned char *json_text;
    size_t json_text_sz;
    unsigned char* errmsg;

    /* Fetch our single argument. */
    if (!lua_isstring(L, -1)) {
        return luaL_error(L, "Argument to parse_string is not a string.");
    }
    json_text = (const unsigned char*)lua_tostring(L, -1);
    json_text_sz = strlen((const char *)json_text);

    rc = ib_mpool_create(&mp, "ibjson", NULL);
    if (rc != IB_OK) {
        return luaL_error(L, "Cannot allocate memory pool.");
    }

    cbdata.L = L;
    cbdata.mp = mp;
    cbdata.push_fn = &ibjson_pushval;

    yajl_handle yajl = yajl_alloc(&g_yajl_callbacks, NULL, (void *)&cbdata);
    if (yajl == NULL) {
        ib_mpool_release(mp);
        return luaL_error(L, "Cannot allocate YAJL parser handle.");
    }

    yc = yajl_config(
        yajl,
        yajl_allow_comments,
        yajl_allow_multiple_values);
    if (yc == 0) {
        ib_mpool_release(mp);
        return luaL_error(L, "Cannot configure YAJL parser handle.");
    }

    yc = yajl_parse(yajl, json_text, json_text_sz);
    switch(yc) {
        case yajl_status_error:
            errmsg = yajl_get_error(yajl, 1, json_text, json_text_sz);
            lua_pushstring(L, (const char *)errmsg);
            yajl_free_error(yajl, errmsg);
            ib_mpool_release(mp);
            yajl_free(yajl);
            return lua_error(L);
        case yajl_status_client_canceled:
            ib_mpool_release(mp);
            yajl_free(yajl);
            return luaL_error(L, "Parse error.");
        case yajl_status_ok:
            break;
    }

    yc = yajl_complete_parse(yajl);
    switch(yc) {
        case yajl_status_error:
            errmsg = yajl_get_error(yajl, 1, json_text, json_text_sz);
            lua_pushstring(L, (const char *)errmsg);
            yajl_free_error(yajl, errmsg);
            ib_mpool_release(mp);
            yajl_free(yajl);
            return lua_error(L);
        case yajl_status_client_canceled:
            ib_mpool_release(mp);
            yajl_free(yajl);
            return luaL_error(L, "Parse error.");
        case yajl_status_ok:
            break;
    }

    ib_mpool_release(mp);
    yajl_free(yajl);
    return 1;
}

/* Forward declaration. to allow for mutual recursion. */
static yajl_gen_status ibjson_gen(lua_State *L, yajl_gen gen);

static yajl_gen_status ibjson_gen_list(lua_State *L, yajl_gen gen, int len) {
    yajl_gen_status ygc;

    ygc = yajl_gen_array_open(gen);
    if (ygc != yajl_gen_status_ok) {
        return ygc;
    }

    for (int i = 1; i <= len; ++i) {
        lua_pushinteger(L, i);
        lua_gettable(L, -2);
        ygc = ibjson_gen(L, gen);
        if (ygc != yajl_gen_status_ok) {
            return ygc;
        }
    }

    ygc = yajl_gen_array_close(gen);
    if (ygc != yajl_gen_status_ok) {
        return ygc;
    }

    lua_pop(L, 1);
    return yajl_gen_status_ok;
}

static yajl_gen_status ibjson_gen_map(lua_State *L, yajl_gen gen) {
    yajl_gen_status ygc;

    ygc = yajl_gen_map_open(gen);
    if (ygc != yajl_gen_status_ok) {
        return ygc;
    }

    lua_pushnil(L);

    while (lua_next(L, -2) != 0) {
        size_t s;
        const unsigned char* key =
            (const unsigned char*)lua_tolstring(L, -2, &s);
        ygc = yajl_gen_string(gen, key, s);
        if (ygc != yajl_gen_status_ok) {
            return ygc;
        }

        ygc = ibjson_gen(L, gen);
        if (ygc != yajl_gen_status_ok) {
            return ygc;
        }
    }

    ygc = yajl_gen_map_close(gen);
    if (ygc != yajl_gen_status_ok) {
        return ygc;
    }

    /* Remove the table. */
    lua_pop(L, 1);

    return yajl_gen_status_ok;
}

/**
 * Recursive helper function to generate JSON.
 *
 * @param[in] L Lua stack.
 * @param[in] gen The generator that will be accumulating the JSON text.
 *
 * @returns
 * - @c yajl_gen_status_ok on success.
 * - Status code retured by @c yajl_gen_&lt;type&gt; on error.
 */
static yajl_gen_status ibjson_gen(lua_State *L, yajl_gen gen)
{
    yajl_gen_status ygc;

    /* Successs, when called recursively. */
    if (lua_gettop(L) == 0) {
        return yajl_gen_status_ok;
    } else if (lua_isboolean(L, -1)) {
        ygc = yajl_gen_bool(gen, lua_toboolean(L, -1));
        if (ygc != yajl_gen_status_ok) {
            return ygc;
        }
        lua_pop(L, 1);
    }
    else if (lua_isnil(L, -1)) {
        ygc = yajl_gen_null(gen);
        if (ygc != yajl_gen_status_ok) {
            return ygc;
        }
        lua_pop(L, 1);
    }
    else if (lua_isnumber(L, -1)) {
        lua_Number n = lua_tonumber(L, -1);
        ygc = yajl_gen_double(gen, n);
        if (ygc != yajl_gen_status_ok) {
            return ygc;
        }
        lua_pop(L, 1);
    }
    else if (lua_isstring(L, -1)) {
        size_t s;
        const unsigned char* key =
            (const unsigned char*)lua_tolstring(L, -1, &s);

        ygc = yajl_gen_string(gen, key, s);
        if (ygc != yajl_gen_status_ok) {
            return ygc;
        }

        lua_pop(L, 1);
    }
    else if (lua_istable(L, -1)) {
        int objlen = lua_objlen(L, -1);
        if (objlen == 0) {
            return ibjson_gen_map(L, gen);
        }
        else {
            return ibjson_gen_list(L, gen, objlen);
        }
    }
    else if (lua_iscfunction(L, -1)) {
        luaL_error(L, "Serialization of CFunction to JSON is not supported.");
    }
    else if (lua_isfunction(L, -1)) {
        luaL_error(L, "Serialization of Function to JSON is not supported.");
    }
    else if (lua_islightuserdata(L, -1)) {
        luaL_error(L,
            "Serialization of Light User Data to JSON is not supported.");
    }
    else if (lua_isthread(L, -1)) {
        luaL_error(L, "Serialization of a thread to JSON is not supported.");
    }
    else if (lua_isuserdata(L, -1)) {
        luaL_error(L, "Serialization of user data to JSON is not supported.");
    }
    else {
        luaL_error(L,
            "Unknown Lua type on top of stack: %s", lua_tostring(L, -1));
    }

    return yajl_gen_status_ok;
}

/**
 * Takes the value at the top of the stack and converts it to JSON.
 *
 * A string representing the parsed JSON is pushed back onto the top of
 * the Lua stack. This is the single return value to the Lua runtime.
 *
 * @param[in] L Lua stack.
 * @returns the number of elements returned from the Lua call.
 */
LUALIB_API int ibjson_to_string(lua_State *L)
{
    assert(L != NULL);

    int yc;
    yajl_gen_status ygc;
    const unsigned char *json_text;
    size_t json_text_len;

    if (lua_gettop(L) != 1) {
        return luaL_error(L, "This function only accepts 1 argument.");
    }

    yajl_gen gen = yajl_gen_alloc(NULL);
    if (gen == NULL) {
        return luaL_error(L, "Failed to allocate JSON generator.");
    }

    yc = yajl_gen_config(gen, yajl_gen_beautify, 1);
    if (yc == 0) {
        yajl_gen_free(gen);
        return luaL_error(L, "Failed to configure JSON generator.");
    }

    ygc = ibjson_gen(L, gen);
    if (ygc != yajl_gen_status_ok) {
        yajl_gen_free(gen);
        return luaL_error(L, "Failed to generate JSON: %d", ygc);
    }

    ygc = yajl_gen_get_buf(gen, &json_text, &json_text_len);
    if (ygc != yajl_gen_status_ok) {
        yajl_gen_free(gen);
        return luaL_error(L, "Failed to retrieve JSON text buffer: %d", ygc);
    }

    /* Assert that if we end up here without error, we've parsed the stack. */
    assert(lua_gettop(L) == 0 && "Lua stack was not totally consumed.");

    /* Push return value. */
    lua_pushlstring(L, (char *)json_text, json_text_len);

    /* Cleanup. */
    yajl_gen_free(gen);

    return 1;
}

/**
 * The table of mappings from Lua function names to C implementations.
 */
static const luaL_reg jsonlib[] = {
    {"parse_string", ibjson_parse_string},
    {"to_string",    ibjson_to_string},
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
LUALIB_API int luaopen_ibjson(lua_State *L) {

    luaL_register(L, LUA_IBJSONLIB_NAME, jsonlib);
    assert(lua_istable(L, -1));

    lua_pushstring(L, LUA_IBJSONLIB_VERSION);
    lua_setfield(L, -2, "_VERSION");

    lua_pushstring(L, LUA_IBJSON_COPYRIGHT);
    lua_setfield(L, -2, "_COPYRIGHT");

    lua_pushstring(L, LUA_IBJSON_DESCRIPTION);
    lua_setfield(L, -2, "_DESCRIPTION");

    return 1;
}

/**
 * @}
 */
