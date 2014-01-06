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
/// @brief IronBee --- LUA rules module tests
///
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"
#include "base_fixture.h"

extern "C" {
#include "lua_common_private.h"
}

#include "ibtest_util.hpp"
#include "engine_private.h"
#include <ironbee/hash.h>
#include <ironbee/mpool.h>
#include <ironbee/field.h>
#include <ironbee/uuid.h>
#include <string>

namespace {
    // Defined in Makefile.am to point to test_module_rules_lua.lua.
    const char* luafile = TEST_LUA_FILE;
}

/**
 * Test rules.
 */
class TestIronBeeModuleRulesLua : public BaseTransactionFixture
{
    protected:
    ib_rule_t *rule;

    public:

    TestIronBeeModuleRulesLua() : BaseTransactionFixture() {
    }

    virtual void SetUp(){
        BaseTransactionFixture::SetUp();
        loadModule("ibmod_rules.so");
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
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ibapi", "ironbee/api"));
    ASSERT_EQ(IB_OK, ib_lua_load_eval(ib_engine, L, luafile));

}

TEST_F(TestIronBeeModuleRulesLua, load_func_eval)
{
    int res = 0;
    ib_tx_t tx;
    tx.ib = ib_engine;
    ASSERT_EQ(IB_OK, ib_uuid_create_v4(tx.id));
    ASSERT_EQ(IB_OK, ib_conn_create(ib_engine, &tx.conn, NULL));

    ib_rule_exec_t rule_exec;
    memset(&rule_exec, 0, sizeof(rule_exec));
    rule_exec.ib = ib_engine;
    rule_exec.tx = &tx;
    rule_exec.rule = rule;

    lua_State *L = luaL_newstate();
    ASSERT_NE(static_cast<lua_State*>(NULL), L);
    luaL_openlibs(L);
    setSearchPath(L);
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ffi", "ffi"));
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ibapi", "ironbee/api"));
    ASSERT_EQ(IB_OK, ib_lua_load_func(ib_engine, L, luafile, "f1"));
    ASSERT_EQ(IB_OK, ib_lua_func_eval_int(ib_engine, &tx, L, "f1", &res));
    ASSERT_EQ(5, res);
}

TEST_F(TestIronBeeModuleRulesLua, new_state)
{
    int res = 0;
    ib_tx_t tx;
    tx.ib = ib_engine;
    tx.mp = ib_engine->mp;

    ib_rule_exec_t rule_exec;
    memset(&rule_exec, 0, sizeof(rule_exec));
    rule_exec.ib = ib_engine;
    rule_exec.tx = &tx;
    rule_exec.rule = rule;

    ib_tx_generate_id(&tx);
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    setSearchPath(L);
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ffi", "ffi"));
    ASSERT_EQ(IB_OK, ib_lua_require(ib_engine, L, "ibapi", "ironbee/api"));
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
    const ib_operator_t *op;
    void *instance_data;
    ib_num_t result;

    ib_field_t* field1;

    const char* op_name = "test_module_rules_lua.lua";
    const char* rule_name = "luarule001";

    char* str1 = (char *)ib_mpool_strdup(ib_engine->mp, "string 1");
    ASSERT_TRUE(str1);

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
    ASSERT_EQ(IB_OK,
        ib_operator_lookup(ib_engine, op_name, &op)
    );

    ASSERT_EQ(IB_OK, ib_operator_inst_create(op,
                                             ib_context_main(ib_engine),
                                             IB_OP_CAPABILITY_NON_STREAM,
                                             rule_name,
                                             &instance_data));
    performTx();

    // Attempt to match.
    ASSERT_EQ(IB_OK, ib_operator_inst_execute(op,
                                          instance_data,
                                          ib_tx,
                                          field1,
                                          NULL,
                                          &result));

    // This time we should succeed.
    ASSERT_TRUE(result);
}
