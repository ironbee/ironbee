
#include <lua.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

#define TOSTR(x) #x
#define TOSTR2(x) TOSTR(x)
#define FFI_FILE_STR TOSTR2(FFI_FILE)

TEST(LuaJIT, MakeStack) {
    lua_State *L = luaL_newstate();

    ASSERT_TRUE(L);

    lua_close(L);
}

TEST(LuaJIT, LoadFFI) {
    lua_State *L = luaL_newstate();

    ASSERT_TRUE(L);

    luaL_openlibs(L);

    std::cout<<"Loading file "<<FFI_FILE_STR<<std::endl;
    ASSERT_EQ(0, luaL_loadfile(L, FFI_FILE_STR));

    /* If true, there is an error. */
    if(lua_pcall(L, 0, 0, 0)) {
        if (lua_isstring(L, -1)) {
            ASSERT_TRUE(0) << std::string(lua_tostring(L, -1));
        }
    }

    lua_close(L);
}
