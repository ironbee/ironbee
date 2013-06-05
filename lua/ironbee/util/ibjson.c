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

/**
 * Callback data passed around a Yajl parse.
 */
struct ibjson_cbdata_t {
    ib_mpool_t *mp;
    lua_State *L;
};

typedef struct ibjson_cbdata_t ibjson_cbdata_t;

/*****************************************************************************
 * YAJL callback definitions.
 ****************************************************************************/

static int yajl_boolean(void *ctx, int boolVal)
{
    return 1;
}
static int yajl_double(void *ctx, double doubleVal)
{
    return 1;
}
static int yajl_end_array(void *ctx)
{
    return 1;
}
static int yajl_end_map(void *ctx)
{
    return 1;
}
static int yajl_integer(void *ctx, long long integerVal)
{
    return 1;
}
static int yajl_map_key(void *ctx, const unsigned char *key, size_t stringLen)
{
    return 1;
}
static int yajl_null(void *ctx)
{
    return 1;
}
static int yajl_number(void *ctx, const char *numberVal, size_t numberLen)
{
    return 1;
}
static int yajl_start_array(void *ctx)
{
    return 1;
}
static int yajl_start_map(void *ctx)
{
    return 1;
}
static int yajl_string(void *ctx, const unsigned char *stringVal, size_t stringLen)
{
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

    yajl_handle yajl = yajl_alloc(&g_yajl_callbacks, NULL, (void *)&cbdata);

    if (yajl == NULL) {
        ib_mpool_release(mp);
        return luaL_error(L, "Cannot allocate YAJL parser handle.");
    }

    /* Start with nil on the stack. */
    lua_pushnil(L);

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
