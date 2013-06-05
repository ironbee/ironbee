
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

TEST_F(LuaTest, LoadFFI) {
    ASSERT_TRUE(L);

    ASSERT_EQ(0, cpathAppend("../?.so"));
    ASSERT_EQ(0, cpathAppend("../.libs/?.so"));
    ASSERT_EQ(0, pathAppend("../?.lua"));
    ASSERT_EQ(0, pathAppend(TOP_SRCDIR_STR "/lua/?.lua"));

    std::cout<<"Loading file "<<FFI_FILE_STR<<std::endl;
    ASSERT_EQ(0, luaL_loadfile(L, FFI_FILE_STR));

    /* If true, there is an error. */
    if(lua_pcall(L, 0, 0, 0)) {
        if (lua_isstring(L, -1)) {
            ASSERT_TRUE(0) << std::string(lua_tostring(L, -1));
        }
    }
}
