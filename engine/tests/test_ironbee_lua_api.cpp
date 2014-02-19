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

/**
 * @file
 * @brief IronBee --- Test IronBee Lua API.
 */

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
#include <ironbee/mpool.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

using namespace std;

/**
 * @class IronBeeLuaApi test_ironbee_lua_api.cpp test_ironbee_lua_api.cpp
 *
 * Test the IronBee Lua Api.
 *
 * For Lua Rule testing see @c test_module_rules_lua.cc.
 */
class IronBeeLuaApi : public BaseTransactionFixture
{
public:

    lua_State* L;

    ib_module_t *mod_htp;
    ib_rule_exec_t ib_rule_exec;
    ib_rule_t *ib_rule;

    static const char *ib_conf;

    /**
     * Calls BaseFixture::SetUp(), then creates a new Lua State,
     * loads ffi, and ironbee/api, and then sets ib_engine
     * to a copy of the ironbee engine.
     */
    virtual void SetUp()
    {
        BaseTransactionFixture::SetUp();
        configureIronBee();

        ASSERT_IB_OK(ib_rule_create(ib_engine,
                                    ib_engine->ectx,
                                    __FILE__,
                                    __LINE__,
                                    true,
                                    &ib_rule));
        ib_rule->meta.id = "const_rule_id";
        ib_rule->meta.full_id = "full_const_rule_id";

        // Now, run the transaction
        performTx( );

        memset(&ib_rule_exec, 0, sizeof(ib_rule_exec));
        ib_rule_exec.ib = ib_engine;
        ib_rule_exec.tx = ib_tx;
        ib_rule_exec.rule = ib_rule;

        /* Initialize a new lua state. */
        L = luaL_newstate();

        /* Open standard libraries. */
        luaL_openlibs(L);

        appendToSearchPath(IB_XSTRINGIFY(RULE_BASE_PATH));
        appendToSearchPath(IB_XSTRINGIFY(MODULE_BASE_PATH));

        require("ffi", "ffi");
        require("ibapi", "ironbee/api");

        lua_pushlightuserdata(L, &ib_rule_exec);
        lua_setglobal(L, "ib_rule_exec");
        lua_pushlightuserdata(L, ib_engine);
        lua_setglobal(L, "ib_engine");
        lua_pushlightuserdata(L, ib_tx);
        lua_setglobal(L, "ib_tx");

        /* Construct an IB value. */
        eval("ib = ibapi.ruleapi:new(ib_rule_exec, ib_engine, ib_tx)");
    }

    // Overload the configuration and header generation methods
    void configureIronBee()
    {
        configureIronBeeByString(ib_conf);
    }
    void generateRequestHeader( )
    {
        addRequestHeader("Host", "UnitTest");
    }
    void generateResponseHeader( )
    {
        addResponseHeader("Content-Type", "text/html");
    }

    void require(const string& name, const string& module)
    {
        int rc;

        lua_getglobal(L, "require");
        lua_pushstring(L, module.c_str());
        rc = lua_pcall(L, 1, 1, 0);

        if (rc!=0) {
            throw runtime_error("Failed to require "+module+" - "+
                string(lua_tostring(L, -1)));
            lua_pop(L, 1);
        }

        lua_setglobal(L, name.c_str());
    }

    /**
     * Append the path do a directory to the Lua search path.
     *
     * @param[in] path Path to a directory containing *.lua files.
     *            The string "/?.lua" is appended to the path before the
     *            path is appended to Lua's package.path value.
     */
    void appendToSearchPath(const string& path)
    {
        /* Set lua load path. */
        lua_getglobal(L, "package");
        lua_pushstring(L, "path");
        lua_pushstring(L, "path");
        lua_gettable(L, -3);
        lua_pushstring(L, ";");
        lua_pushstring(L, (path + "/?.lua").c_str());
        lua_concat(L, 3);
        lua_settable(L, -3);
    }

    void eval(const string& luaCode)
    {
        if ( luaL_dostring(L, luaCode.c_str()) != 0 ) {
            string msg("Executing lua code snippet has failed - ");
            msg.append(lua_tostring(L, -1));
            lua_pop(L, 1);
            throw runtime_error(msg);
        }
    }

    /**
     * Close the lua stack and call BaseFixture::TearDown().
     */
    virtual void TearDown()
    {
        lua_close(L);

        ib_state_notify_conn_closed(ib_engine, ib_conn);

        BaseFixture::TearDown();
    }

    virtual ~IronBeeLuaApi() {
    }
};

const char * IronBeeLuaApi::ib_conf =
    "LogLevel 9\n"
    "SensorId AAAABBBB-1111-2222-3333-FFFF00000023\n"
    "SensorName ExampleSensorName\n"
    "SensorHostname example.sensor.tld\n"
    "LoadModule \"ibmod_htp.so\"\n"
    "LoadModule \"ibmod_pcre.so\"\n"
    "LoadModule \"ibmod_rules.so\"\n"
    "LoadModule \"ibmod_lua.so\"\n"
    "LoadModule \"ibmod_user_agent.so\"\n"
    "<Site default>\n"
        "SiteId AAAABBBB-1111-2222-3333-000000000000\n"
        "Hostname *\n"
    "</Site>\n" ;

TEST_F(IronBeeLuaApi, logError)
{
    eval("ib:logError(\"======== Test Log Message %d ========\", 100)");
}

TEST_F(IronBeeLuaApi, logDebug)
{
    eval("ib:logDebug(\"======== Test Log Message %d ========\", 100)");
}

TEST_F(IronBeeLuaApi, add_and_get)
{
    const char* val = "myStringValue";

    // Call ib:setString("key1", "myStringValue")
    eval("ib:add(\"key1\", \"myStringValue\")");
    eval("ib:add(\"key2\", 4)");

    eval("return ib:get(\"key1\")");
    eval("return ib:get(\"key2\")");

    ASSERT_STREQ(val, lua_tostring(L, -2));
    ASSERT_EQ(4, lua_tonumber(L, -1));

    lua_pop(L, 2);

}

TEST_F(IronBeeLuaApi, get)
{
    eval("t = ib:get(\"request_headers\")");

    eval("for k,v in pairs(t) do\n"
         "  ib:logDebug(\"IronBeeLuaApi.get: %s=%s\", v[1], v[2])"
         "end");
}

TEST_F(IronBeeLuaApi, getFieldList)
{
  eval("t = ib:getFieldList()");

  eval("for k,v in pairs(t) do\n"
       "  print(string.format(\"%s=%s\", k, v))\n"
       "end");
}

TEST_F(IronBeeLuaApi, request_headers)
{
  eval("return ib:get(\"request_headers\")[1][2]");

  ASSERT_STREQ("UnitTest", lua_tostring(L, -1));

  lua_pop(L, 1);
}

TEST_F(IronBeeLuaApi, get_names_request_headers)
{
  eval("return ib:getNames(\"request_headers\")[1]");

  ASSERT_STREQ("Host", lua_tostring(L, -1));

  lua_pop(L, 1);
}

TEST_F(IronBeeLuaApi, get_values_request_headers)
{
  eval("return ib:getValues(\"request_headers\")[1]");

  ASSERT_STREQ("UnitTest", lua_tostring(L, -1));

  lua_pop(L, 1);
}


TEST_F(IronBeeLuaApi, add_list)
{
  ib_field_t* list_field;

  eval("ib:add(\"MyList1\", {})");

  list_field = getVar("MyList1");
  ASSERT_TRUE(list_field);

  eval("ib:add(\"MyList1\", { { \"a\", \"b\" }, { \"c\", 21 } } )");
  eval("return ib:get(\"MyList1\")[1][1]");
  eval("return ib:get(\"MyList1\")[1][2]");
  eval("return ib:get(\"MyList1\")[2][1]");
  eval("return ib:get(\"MyList1\")[2][2]");

  ASSERT_STREQ("a", lua_tostring(L, -4));
  ASSERT_STREQ("b", lua_tostring(L, -3));
  ASSERT_STREQ("c", lua_tostring(L, -2));
  ASSERT_EQ(21, lua_tonumber(L, -1));
  lua_pop(L, 2);
}

TEST_F(IronBeeLuaApi, set)
{
    eval("ib:add(\"MyInt\", 4)");
    eval("ib:add(\"MyString\", \"my string\")");
    eval("ib:add(\"MyTable\", { { \"a\", \"b\" } })");

    eval("ib:logInfo(ib:get(\"MyInt\")+1)");
    eval("ib:set(\"MyInt\", ib:get(\"MyInt\")+1)");
    eval("ib:set(\"MyString\", \"my other string\")");
    eval("ib:set(\"MyTable\", { { \"c\", \"d\" } })");

    eval("return ib:get(\"MyInt\")");
    eval("return ib:get(\"MyString\")");
    eval("return ib:get(\"MyTable\")[1][1]");
    eval("return ib:get(\"MyTable\")[1][2]");

    ASSERT_EQ(5, lua_tonumber(L, -4));
    ASSERT_STREQ("my other string", lua_tostring(L, -3));
    ASSERT_STREQ("c", lua_tostring(L, -2));
    ASSERT_STREQ("d", lua_tostring(L, -1));
    lua_pop(L, 4);

    eval("return ib:getValues(\"MyInt\")[1]");
    eval("return ib:getNames(\"MyInt\")[1]");
    ASSERT_EQ(5, lua_tonumber(L, -2));
    ASSERT_STREQ("MyInt", lua_tostring(L, -1));
    lua_pop(L, 2);
}

TEST_F(IronBeeLuaApi, add_event)
{
    eval("ib:addEvent(\"Saw some failure\")");
    eval("ib:addEvent(\"Saw some failure\", { system = \"public\" } )");
}

TEST_F(IronBeeLuaApi, read_event)
{
    eval("ib:addEvent(\"Saw some failure\")");
    eval("ib:addEvent(\"Saw some failure\", { system = \"public\" } )");
    eval("ib:forEachEvent(function(e)\n"
         "    if e:getSuppress() ~= \"none\" then\n"
         "        cause_a_crash()\n"
         "    end\n"
         "    print(e:getRuleId())\n"
         "    e:setSuppress(\"incomplete\")\n"
         "    if e:getSuppress() ~= \"incomplete\" then\n"
         "        cause_a_crash()\n"
         "    end\n"
         "end)");
}

TEST_F(IronBeeLuaApi, read_event2)
{
    eval("ib:addEvent(\"Saw some failure\")");
    eval("ib:addEvent(\"Saw some failure\", { system = \"public\" } )");
    eval("for i,e in ib:events() do\n"
         "    if e:getSuppress() ~= \"none\" then\n"
         "        cause_a_crash()\n"
         "    end\n"
         "    print(e:getRuleId())\n"
         "    e:setSuppress(\"incomplete\")\n"
         "    if e:getSuppress() ~= \"incomplete\" then\n"
         "        cause_a_crash()\n"
         "    end\n"
         "end");
}

TEST_F(IronBeeLuaApi, read_event3)
{
    eval("ib:addEvent(\"Saw some failure\", { tags = { \"t1\", \"t2\" }} )");
    eval("for i,e in ib:events() do\n"
         "    print(e:getRuleId())\n"
         "    for j,t in e:tags() do \n"
         "        print(t)\n"
         "    end\n"
         "end");
}
