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

#include <gtest/gtest.h>

#include <iostream>
#include <string>
#include <stdexcept>
#include "base_fixture.h"

extern "C" {
#include "engine_private.h"
#include "rule_engine_private.h"
#include <ironbee/release.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/mm.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

/**
 * @file
 * @brief IronBee Tests For Lua Modules
 *
 * Tests for IronBee Lua Modules.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

using namespace std;

/**
 * @class IronBeeLuaModules test_ironbee_lua_modules.cpp
 * test_ironbee_lua_modules.cpp
 *
 * Test the IronBee Lua Api.
 *
 * For Lua Rule testing see test_module_rules_lua.cc.
 */
struct IronBeeLuaModules : public BaseTransactionFixture
{

    ib_module_t *mod_htp;

    static const char *c_ib_conf;

    /**
     * Calls BaseFixture::SetUp(), then creates a new Lua State,
     * loads ffi, and ironbee/api, and then sets ib_engine
     * to a copy of the ironbee engine.
     */
    virtual void SetUp()
    {
        BaseTransactionFixture::SetUp();
        configureIronBeeByString(c_ib_conf);
        performTx();
    }
    void generateRequestHeader( )
    {
        addRequestHeader("Host", "UnitTest");
    }
    void generateResponseHeader( )
    {
        addResponseHeader("Content-Type", "text/html");
    }

    /**
     * Close the lua stack and call BaseFixture::TearDown().
     */
    virtual void TearDown() {
        ib_state_notify_conn_closed(ib_engine, ib_conn);

        BaseFixture::TearDown();
    }

    virtual ~IronBeeLuaModules() {
    }
};

const char * IronBeeLuaModules::c_ib_conf =
    "LogLevel 9\n"
    "SensorId AAAABBBB-1111-2222-3333-FFFF00000023\n"
    "SensorName ExampleSensorName\n"
    "SensorHostname example.sensor.tld\n"
    "LoadModule \"ibmod_htp.so\"\n"
    "LoadModule \"ibmod_pcre.so\"\n"
    "LoadModule \"ibmod_rules.so\"\n"
    "LoadModule \"ibmod_lua.so\"\n"
    "ModuleBasePath \".\"\n"
    "LuaInclude \"test_ironbee_lua_configs.lua\"\n"
    "<Site default>\n"
        "SiteId AAAABBBB-1111-2222-3333-000000000000\n"
        "Hostname *\n"
    "</Site>\n" ;

TEST_F(IronBeeLuaModules, load){
}
