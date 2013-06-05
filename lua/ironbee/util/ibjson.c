#define LUA_LIB
#include "lua.h"
#include "lauxlib.h"

#include <yajl/yajl_parse.h>  
#include <yajl/yajl_gen.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#endif
#include <yajl/yajl_tree.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

static const char *LUA_IBJSONLIB_NAME = "ibjson";
static const char *LUA_IBJSONLIB_VERSION = "1.0";

LUALIB_API int ibjson_parse_string(lua_State *L) {
    // TODO - unimplemented.
    lua_pushnil(L);
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

