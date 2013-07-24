
#include <lua.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

#define TOSTR(x) #x
#define TOSTR2(x) TOSTR(x)
#define FFI_FILE_STR TOSTR2(FFI_FILE)
#define TOP_SRCDIR_STR TOSTR2(TOP_SRCDIR)

TEST(LuaJIT, MakeStack) {
    lua_State *L = luaL_newstate();

    ASSERT_TRUE(L);

    lua_close(L);
}

class LuaTest : public ::testing::Test {
    protected:
    lua_State *L;

    public:

    virtual void SetUp()
    {
        L = luaL_newstate();
        luaL_openlibs(L);
    }

    virtual void TearDown()
    {
        lua_close(L);
    }

    int doString(std::string path) {
        return luaL_dostring(L, path.c_str());
    }

    int cpathAppend(std::string cpath) {
        return doString("package.cpath = package.cpath .. \";" + cpath + "\"");
    }

    int pathAppend(std::string path) {
        return doString("package.path = package.path .. \";" + path + "\"");
    }

    virtual ~LuaTest()
    {
    }
};
