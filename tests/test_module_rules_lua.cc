// Licensed to Qualys, Inc. (QUALYS) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// QUALYS licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee - poc_sig module tests
/// 
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

extern "C" {
#include "rules_lua.h"
}

#define TESTING

#include "ibtest_util.c"

#include <string>

namespace {
    const char* luafile = TEST_LUA_FILE;
}

TEST(TestIronBeeModuleRulesLua, DISABLED_load_eval)
{
    ib_engine_t *ib;
    ibtest_engine_create(&ib);
    ib_tx_t tx;

    lua_State *L = luaL_newstate();

    ASSERT_NE(static_cast<lua_State*>(NULL), L);

    ASSERT_EQ(IB_OK, ib_lua_load_eval(ib, L, luafile));

    ibtest_engine_destroy(ib);
}

TEST(TestIronBeeModuleRulesLua, DISABLED_load_func_eval)
{
    ib_engine_t *ib;
    ibtest_engine_create(&ib);
    ib_tx_t tx;

    lua_State *L = luaL_newstate();
    ASSERT_NE(static_cast<lua_State*>(NULL), L);
    ASSERT_EQ(IB_OK, ib_lua_load_eval(ib, L, luafile));
    ASSERT_EQ(IB_OK, ib_lua_load_func(ib, L, luafile, "f1"));
    ASSERT_EQ(IB_OK, ib_lua_func_eval(ib, &tx, L, "f1"));

    ibtest_engine_destroy(ib);
}

TEST(TestIronBeeModuleRulesLua, DISABLED_new_state)
{
    ib_engine_t *ib;
    ibtest_engine_create(&ib);
    ib_tx_t tx;
    lua_State *L = luaL_newstate();
    ASSERT_NE(static_cast<lua_State*>(NULL), L);
    lua_State *L2;

    ASSERT_EQ(IB_OK, ib_lua_new_thread(ib, L, &L2));
    ASSERT_NE(static_cast<lua_State*>(NULL), L2);
    ASSERT_EQ(IB_OK, ib_lua_load_eval(ib, L2, luafile));
    ASSERT_EQ(IB_OK, ib_lua_load_func(ib, L2, luafile, "f1"));
    ASSERT_EQ(IB_OK, ib_lua_func_eval(ib, &tx, L2, "f1"));
    ASSERT_EQ(IB_OK, ib_lua_join_thread(ib, L, &L2));

    ibtest_engine_destroy(ib);
}
