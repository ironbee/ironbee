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
/// @brief IronBee &mdash; LUA rules module tests
///
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"
#include "base_fixture.h"

extern "C" {
#include "rules_lua_private.h"
}

#include "ibtest_util.hh"
#include "engine_private.h"
#include <ironbee/hash.h>
#include <ironbee/mpool.h>
#include <ironbee/field.h>
#include <string>

namespace {
    const char* luafile = TEST_LUA_FILE;
}

/**
 * Test rules.
 */
class TestIronBeeModuleRulesLua : public BaseFixture {
    private:

    ib_module_t *m_module;

    protected:
    ib_rule_t *rule;

    public:

    TestIronBeeModuleRulesLua() : BaseFixture() {
    }

    virtual void SetUp(){
        BaseFixture::SetUp();
        loadModule(&m_module, "ibmod_rules.so");
        ASSERT_IB_OK(ib_rule_create(ib_engine,
                                    ib_engine->ectx,
                                    __FILE__,
                                    __LINE__,
                                    true,
                                    &rule));
    }

    void setSearchPath(lua_State *L)
    {
        std::string rule_path(IB_XSTRINGIFY(RULE_BASE_PATH));
        std::string module_path(IB_XSTRINGIFY(MODULE_BASE_PATH));

        rule_path.append("/?.lua");
        module_path.append("/?.lua");
        ib_lua_add_require_path(ib_engine, L, rule_path.c_str());
        ib_lua_add_require_path(ib_engine, L, module_path.c_str());
    }
};

TEST_F(TestIronBeeModuleRulesLua, load_eval)
{
    lua_State *L = luaL_newstate();

    luaL_openlibs(L);

    setSearchPath(L);

    ASSERT_NE(static_cast<lua_State*>(NULL), L);
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ffi", "ffi"));
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ironbee", "ironbee-ffi"));
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ibapi", "ironbee-api"));
    ASSERT_EQ(IB_OK, ib_lua_load_eval(ib_engine, L, luafile));

}

TEST_F(TestIronBeeModuleRulesLua, load_func_eval)
{
    int res = 0;
    ib_tx_t tx;
    tx.ib = ib_engine;
    tx.id = "tx_id.TestIronBeeModuleRulesLua.load_func_eval";

    lua_State *L = luaL_newstate();
    ASSERT_NE(static_cast<lua_State*>(NULL), L);
    luaL_openlibs(L);
    setSearchPath(L);
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ffi", "ffi"));
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ironbee", "ironbee-ffi"));
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ibapi", "ironbee-api"));
    ASSERT_EQ(IB_OK, ib_lua_load_func(ib_engine, L, luafile, "f1"));
    ASSERT_EQ(IB_OK, ib_lua_func_eval_int(ib_engine, &tx, L, "f1", &res));
    ASSERT_EQ(5, res);
}

TEST_F(TestIronBeeModuleRulesLua, new_state)
{
    int res = 0;
    ib_tx_t tx;
    tx.ib = ib_engine;

    ib_tx_generate_id(&tx, ib_engine->mp);
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    setSearchPath(L);
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ffi", "ffi"));
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ironbee", "ironbee-ffi"));
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ibapi", "ironbee-api"));
    ASSERT_NE(static_cast<lua_State*>(NULL), L);

    lua_State *L2;

    ASSERT_EQ(IB_OK, ib_lua_new_thread(ib_engine, L, &L2));
    ASSERT_NE(static_cast<lua_State*>(NULL), L2);
    ASSERT_EQ(IB_OK, ib_lua_load_func(ib_engine, L2, luafile, "f1"));
    ASSERT_EQ(IB_OK, ib_lua_func_eval_int(ib_engine, &tx, L2, "f1", &res));
    ASSERT_EQ(IB_OK, ib_lua_join_thread(ib_engine, L, &L2));
    ASSERT_EQ(5, res);
}

TEST_F(TestIronBeeModuleRulesLua, operator_test)
{
    ib_tx_t tx;
    tx.ib = ib_engine;

    ib_operator_t op;
    ib_operator_inst_t *op_inst=NULL;
    ib_num_t result;

    ib_field_t* field1;

    const char* op_name = "lua:test_module_rules_lua.lua";
    const char* rule_name = "luarule001";

    char* str1 = (char *) ib_mpool_alloc(ib_engine->mp, (strlen("string 1")+1));
    strcpy(str1, "string 1");

    ib_tx_generate_id(&tx, ib_engine->mp);

    // Create field 1.
    ASSERT_EQ(IB_OK,
        ib_field_create(
            &field1,
            ib_engine->mp,
            IB_FIELD_NAME("field1"),
            IB_FTYPE_NULSTR,
            ib_ftype_nulstr_in(str1)
        )
    );

    /* Configure the operator. */
    configureIronBee();

    // Ensure that the operator exists.
    ASSERT_EQ(IB_OK, ib_hash_get(ib_engine->operators, (void **)&op, op_name));

    ASSERT_EQ(IB_OK, ib_operator_inst_create(ib_engine,
                                             NULL,
                                             rule,
                                             IB_OP_FLAG_PHASE,
                                             op_name,
                                             "unused parameter.",
                                             IB_OPINST_FLAG_NONE,
                                             &op_inst));

    op_inst->data = (void *) rule_name;

    // Attempt to match.
    ASSERT_EQ(IB_OK, op_inst->op->fn_execute(
        ib_engine, &tx, rule, op_inst->data, op_inst->flags, field1, &result));

    // This time we should succeed.
    ASSERT_TRUE(result);
}
