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
#include <algorithm>
#include <iostream>
#include <locale>
#include <string>

#define TOSTR(x) #x
#define TOSTR2(x) TOSTR(x)
#define FFI_FILE_STR TOSTR2(FFI_FILE)
#define TOP_SRCDIR_STR TOSTR2(TOP_SRCDIR)

class LuaIBCutilTest : public ::ibtesting::LuaTest {
    public:

    virtual void SetUp()
    {
        LuaTest::SetUp();

        cpathAppend("../ironbee/util/.libs/?.so");

        pathAppend("../?.lua");
        pathAppend(TOP_SRCDIR_STR "/lua/?.lua");

        doString("ibcutil = require('ibcutil')");

        // Clear stack.
        lua_settop(L, 0);
    }

    virtual ~LuaIBCutilTest() { }
};

TEST_F(LuaIBCutilTest, LoadingLibrary) {
    lua_getglobal(L, "ibcutil");
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "to_ib_float");
    ASSERT_TRUE(lua_isfunction(L, -1));
}

