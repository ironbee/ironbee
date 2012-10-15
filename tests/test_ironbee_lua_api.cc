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
#include <ironbee/mpool.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

using namespace std;

/**
 * @class IronBeeLuaApi test_ironbee_lua_api.cc test_ironbee_lua_api.cc
 *
 * Test the IronBee Lua Api.
 *
 * For Lua Rule testing see @c test_module_rules_lua.cc.
 */
class IronBeeLuaApi : public BaseFixture {
public:

    lua_State* L;

    ib_conn_t *ib_conn;
    ib_tx_t *ib_tx;
    ib_module_t *mod_htp;
    ib_rule_exec_t ib_rule_exec;
    ib_rule_t *ib_rule;

    /**
     * Calls BaseFixture::SetUp(), then creates a new Lua State,
     * loads ffi, ironbee-ffi, and ironbee-api, and then sets ib_engine
     * to a copy of the ironbee engine.
     */
    void SetUp()
    {
        BaseFixture::SetUp();

        ASSERT_IB_OK(ib_rule_create(ib_engine,
                                    ib_engine->ectx,
                                    __FILE__,
                                    __LINE__,
                                    true,
                                    &ib_rule));
        ib_rule->meta.id = "const_rule_id";
        ib_rule->meta.full_id = "full_const_rule_id";

        ib_state_notify_cfg_started(ib_engine);

        assert(ib_engine->temp_mp != NULL);
        assert(ib_engine->config_mp != NULL);

        /* We need the ibmod_htp to initialize the ib_tx. */
        configureIronBee("test_ironbee_lua_api.conf");
        assert(IB_OK == ib_state_notify_cfg_finished(ib_engine));

        ib_conn = buildIronBeeConnection();

        sendDataIn("GET / HTTP/1.1\r\nHost: UnitTest\r\n\r\n");
        sendDataOut("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");

        /* Lib htp.c does this, so we do this here. */
        assert(ib_conn->tx!=NULL);
        ib_tx = ib_conn->tx;

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
        require("ironbee", "ironbee-ffi");
        require("ibapi", "ironbee-api");

        lua_pushlightuserdata(L, &ib_rule_exec);
        lua_setglobal(L, "ib_rule_exec");
        lua_pushlightuserdata(L, ib_engine);
        lua_setglobal(L, "ib_engine");
        lua_pushlightuserdata(L, ib_tx);
        lua_setglobal(L, "ib_tx");

        /* Construct an IB value. */
        eval("ib = ibapi:new(ib_rule_exec, ib_engine, ib_tx)");
    }

    void sendDataIn(const string& req) {
        BaseFixture::sendDataIn(ib_conn, req);
    }

    void sendDataOut(const string& req) {
        BaseFixture::sendDataOut(ib_conn, req);
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
    void TearDown()
    {
        lua_close(L);

        ib_state_notify_conn_closed(ib_engine, ib_conn);

        BaseFixture::TearDown();
    }
};

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

  ib_data_get(ib_tx->dpi, "MyList1", &list_field);
  ASSERT_TRUE(NULL!=list_field);

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

