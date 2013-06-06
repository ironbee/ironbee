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

#define LUA_LIB
#include "lua.h"
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

static const char *LUA_IBJSONLIB_NAME = "ibjson";
static const char *LUA_IBJSONLIB_VERSION = "1.0";

/* Forward declare the callback type. */
typedef struct ibjson_cbdata_t ibjson_cbdata_t;

/**
 * Function callback to push a value.
 *
 * When parsing JSON different values may be assigned differently.
 * For example, a number parsed after a map key should be assigned to a 
 * table.
 *
 * @param[in] cbdata Callback data.
 */
typedef void(*ibjson_push_fn_t)(ibjson_cbdata_t *cbdata);

/**
 * Callback data passed around a Yajl parse.
 */
struct ibjson_cbdata_t {
    ib_mpool_t *mp;           /**< Memory pool. */
    lua_State *L;             /**< Lua stack. */
    ibjson_push_fn_t push_fn; /**< Current function for pushing value. */
};

/**
 * Nop.
 *
 * @param[in] cbata Callback data.
 */
static void ibjson_pushval(ibjson_cbdata_t *cbdata) {
    /* Nop. The value on the stack is final. */
}

/**
 * Append a value to the list at index -2 in the Lua list.
 *
 * @param[in] cbata Callback data.
 */
static void ibjson_pushlist(ibjson_cbdata_t *cbdata) {
    int i = lua_objlen(cbdata->L, -2) + 1;

    /* Copy the value to the top of the stack. */
    lua_pushvalue(cbdata->L, -1);

    /* Push the index to assign it to. */
    lua_pushinteger(cbdata->L, i);

    lua_replace(cbdata->L, -3);

    lua_settable(cbdata->L, -3);
}

/**
 * Assign the value at index -1 to the key at -2 into the table at -3.
 *
 * @param[in] cbata Callback data.
 */
static void ibjson_pushmap(ibjson_cbdata_t *cbdata) {
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

    /* Remove it. */
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

    /* Remove it. */
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
static int yajl_string(void *ctx, const unsigned char *stringVal, size_t stringLen)
{
    ibjson_cbdata_t *cbdata = (ibjson_cbdata_t *)ctx;

    lua_pushlstring(cbdata->L, (const char *)stringVal, stringLen);

    (cbdata->push_fn)(cbdata);

    return 1;
}

static yajl_callbacks g_yajl_callbacks = {
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
            break;
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
            break;
        case yajl_status_ok:
            break;
    }

    ib_mpool_release(mp);
    yajl_free(yajl);
    return 1;
}

static const luaL_reg jsonlib[] = {
    {"parse_string", ibjson_parse_string},
    {NULL, NULL}
};

LUALIB_API int luaopen_ibjson(lua_State *L) {
    luaL_register(L, LUA_IBJSONLIB_NAME, jsonlib);

    lua_getglobal(L, LUA_IBJSONLIB_NAME);

    lua_pushstring(L, LUA_IBJSONLIB_VERSION);
    lua_setfield(L, -2, "_VERSION");

    lua_pushstring(L, "Copyright (C) 2010-2013 Qualys, Inc.");
    lua_setfield(L, -2, "_COPYRIGHT");

    lua_pushstring(L, "IronBee JSON Interface.");
    lua_setfield(L, -2, "_DESCRIPTION");

    lua_pop(L, 1);

    return 1;
}
