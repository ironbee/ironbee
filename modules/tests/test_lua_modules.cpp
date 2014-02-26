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
#include <unistd.h>
}

class LuaModule : public BaseTransactionFixture
{
};

TEST_F(LuaModule, load_module) {

    ib_var_source_t   *source;         /* Used to check the results. */
    const ib_field_t  *field;          /* Used to check the results. */
    ib_num_t           num;            /* Used to check the results. */
    std::vector<char>  cwd(PATH_MAX);  /* Used to build the module path. */

    /* Where is this test file executing? */
    ASSERT_FALSE(getcwd(&(cwd[0]), cwd.size()) == NULL);

    /* Build the module path using the CWD. */
    const std::string lua_mod_path =
        std::string(&(cwd[0]))+"/test_lua_modules.lua";

    /* Configure IronBee. */
    configureIronBeeByString(
        "LogLevel       info\n"
        "LoadModule     ibmod_lua.so\n"
        "SensorId       B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
        "SensorName     UnitTesting\n"
        "SensorHostname unit-testing.sensor.tld\n"
        "LuaLoadModule "+lua_mod_path+"\n"
        "<Site test-site>\n"
        "    SiteId AAAABBBB-1111-2222-3333-000000000000\n"
        "    Hostname somesite.com\n"
        "</Site>\n"
    );

    /* Execute the test. */
    performTx();

    /* Check the test. */
    ASSERT_EQ(
        IB_OK,
        ib_var_source_acquire(
            &source,
            ib_tx->mm,
            ib_var_store_config(ib_tx->var_store),
            IB_S2SL("LUA_MODULE_COUNTER")));
    ASSERT_EQ(
        IB_OK,
        ib_var_source_get_const(source, &field, ib_tx->var_store));
    ASSERT_EQ(IB_OK, ib_field_value(field, ib_ftype_num_out(&num)));
    ASSERT_EQ(101, num);

}
