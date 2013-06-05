#define LUA_LIB
#include "lua.h"
#include "lauxlib.h"

static const char *LUA_IBJSONLIB_NAME = "ibjson";
//static const char *LUA_IBJSONLIB_VERSION = "1.0.0";

static const luaL_reg jsonlib[] = {
    {NULL, NULL}
};

LUALIB_API int luaopen_json(lua_State *L) {
    luaL_register(L, LUA_IBJSONLIB_NAME, jsonlib);

    return 1;
}

