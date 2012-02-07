
#include "gtest/gtest.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

TEST(luajit, init_test)
{
    lua_State *L = luaL_newstate();

    ASSERT_NE(static_cast<lua_State*>(NULL), L);
}


