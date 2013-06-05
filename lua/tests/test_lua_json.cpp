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

#include <lua.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

#define TOSTR(x) #x
#define TOSTR2(x) TOSTR(x)
#define FFI_FILE_STR TOSTR2(FFI_FILE)
#define TOP_SRCDIR_STR TOSTR2(TOP_SRCDIR)

class LuaJsonTest : public ::testing::Test {
    protected:
    lua_State *L;

    public:

    virtual void SetUp()
    {
        L = luaL_newstate();
        luaL_openlibs(L);

        ASSERT_EQ(0, cpathAppend("../ironbee/util/.libs/?.so"));

        ASSERT_EQ(0, pathAppend("../?.lua"));
        ASSERT_EQ(0, pathAppend(TOP_SRCDIR_STR "/lua/?.lua"));

        ASSERT_EQ(0, doString("return pcall(require, 'ibjson')"));
        ASSERT_TRUE(lua_isboolean(L, -2));
        if (!lua_toboolean(L, -2)) {
            std::cerr << lua_tostring(L, -1) << std::endl;
        }

        ASSERT_TRUE(lua_istable(L, -1));
        lua_setglobal(L, "ibjson");

        // Clear stack.
        lua_settop(L, 0);
    }

    virtual void TearDown()
    {
        lua_settop(L, 0);
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

    virtual ~LuaJsonTest()
    {
    }
};

TEST_F(LuaJsonTest, LoadingLibrary) {
    lua_getglobal(L, "ibjson");
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "parse_string");
    ASSERT_TRUE(lua_isfunction(L, -1));
}

TEST_F(LuaJsonTest, CallParseString) {
    lua_getglobal(L, "ibjson");
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "parse_string");
    ASSERT_TRUE(lua_isfunction(L, -1));
    lua_replace(L, -2);
    lua_pushstring(L, "{}");
    ASSERT_EQ(0, lua_pcall(L, 1, 1, 0));
}

