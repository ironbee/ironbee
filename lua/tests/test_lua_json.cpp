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
#include <ibtest_lua.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

#define TOSTR(x) #x
#define TOSTR2(x) TOSTR(x)
#define FFI_FILE_STR TOSTR2(FFI_FILE)
#define TOP_SRCDIR_STR TOSTR2(TOP_SRCDIR)

class LuaJsonTest : public ::ibtesting::LuaTest {
    public:

    virtual void SetUp()
    {
        cpathAppend("../ironbee/util/.libs/?.so");

        pathAppend("../?.lua");
        pathAppend(TOP_SRCDIR_STR "/lua/?.lua");

        doString("ibjson = require('ibjson')");

        // Clear stack.
        lua_settop(L, 0);
    }

    virtual ~LuaJsonTest() { }
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

TEST_F(LuaJsonTest, ComplexMap) {
    lua_getglobal(L, "ibjson");
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "parse_string");
    ASSERT_TRUE(lua_isfunction(L, -1));
    lua_replace(L, -2);
    lua_pushstring(L, "{ \"a\" : 1, \"b\" : { \"c\" : \"hi\" } }");
    ASSERT_EQ(0, lua_pcall(L, 1, 1, 0));
    ASSERT_TRUE(lua_istable(L, -1));

    /* Check "a" record in map. */
    lua_getfield(L, -1, "a");
    ASSERT_TRUE(lua_isnumber(L, -1));
    ASSERT_EQ(1, lua_tonumber(L, -1));
    lua_pop(L, 1);

    /* Check "b" record in map. */
    lua_getfield(L, -1, "b");
    ASSERT_TRUE(lua_istable(L, -1));

    /* Check "b.c" (c record in map b). */
    lua_getfield(L, -1, "c");
    ASSERT_TRUE(lua_isstring(L, -1));
    ASSERT_STREQ("hi", lua_tostring(L, -1));
    lua_pop(L, 2);
}

TEST_F(LuaJsonTest, ComplexArray) {
    lua_getglobal(L, "ibjson");
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "parse_string");
    ASSERT_TRUE(lua_isfunction(L, -1));
    lua_replace(L, -2);
    lua_pushstring(L, "[ \"a\",  \"b\", { \"c\" : \"hi\" } ]");
    ASSERT_EQ(0, lua_pcall(L, 1, 1, 0));
    ASSERT_TRUE(lua_istable(L, -1));
    ASSERT_EQ(3U, lua_objlen(L, -1));

    lua_pushnumber(L, 1);
    lua_gettable(L, -2);
    ASSERT_TRUE(lua_isstring(L, -1));
    ASSERT_STREQ("a", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_pushnumber(L, 2);
    lua_gettable(L, -2);
    ASSERT_TRUE(lua_isstring(L, -1));
    ASSERT_STREQ("b", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_pushnumber(L, 3);
    lua_gettable(L, -2);
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "c");
    ASSERT_TRUE(lua_isstring(L, -1));
    ASSERT_STREQ("hi", lua_tostring(L, -1));
    lua_pop(L, 2);
}

TEST_F(LuaJsonTest, GenerateFailNoArgs) {
    ASSERT_THROW(doString("ibjson.to_string()"), std::runtime_error);
}

TEST_F(LuaJsonTest, GenerateFail2Args) {
    ASSERT_THROW(doString("ibjson.to_string(1, 2)"), std::runtime_error);
}

TEST_F(LuaJsonTest, GenerateJSONInt) {
    ASSERT_EQ(1, doString("return ibjson.to_string(1)"));
    ASSERT_TRUE(lua_isnumber(L, -1));
    ASSERT_EQ(1, lua_tonumber(L, -1));
    lua_settop(L, 0);
}

TEST_F(LuaJsonTest, GenerateJSONString) {
    ASSERT_EQ(1, doString("return ibjson.to_string('hi')"));
    ASSERT_TRUE(lua_isstring(L, -1));
    ASSERT_STREQ("\"hi\"\n", lua_tostring(L, -1));
    lua_settop(L, 0);
}

TEST_F(LuaJsonTest, GenerateJSONMap) {
    ASSERT_EQ(1, doString("return ibjson.to_string({ ['a'] = 1 })"));
    ASSERT_TRUE(lua_isstring(L, -1));
    ASSERT_STREQ("{\n    \"a\": 1.0\n}\n", lua_tostring(L, -1));
    lua_settop(L, 0);
}

TEST_F(LuaJsonTest, GenerateJSONArray) {
    ASSERT_EQ(1, doString("return ibjson.to_string( { 'a', 'b' })"));
    ASSERT_TRUE(lua_isstring(L, -1));
    ASSERT_STREQ("[\n    \"a\",\n    \"b\"\n]\n", lua_tostring(L, -1));
    lua_settop(L, 0);
}
