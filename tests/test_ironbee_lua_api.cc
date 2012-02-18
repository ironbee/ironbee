#include <gtest/gtest.h>

#include <iostream>
#include <string>
#include <stdexcept>
#include "base_fixture.h"

extern "C" {
#include <ironbee/release.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
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

    /**
     * Calls BaseFixture::SetUp(), then creates a new Lua State,
     * loads ffi, ironbee-ffi, and ironbee-api, and then sets ib_engine
     * to a copy of the ironbee engine.
     */
    virtual void SetUp()
    {
        BaseFixture::SetUp();

        ib_state_notify_cfg_started(ib_engine);

        assert(ib_engine->temp_mp != NULL);
        assert(ib_engine->config_mp != NULL);

        /* We need the ibmod_htp to initialize the ib_tx. */
        configureIronBee("test_ironbee_lua_api.conf");
        assert(IB_OK == ib_state_notify_cfg_finished(ib_engine));

        ib_conn_create(ib_engine, &ib_conn, NULL);
        ib_conn->local_ipstr = "1.0.0.1";
        ib_conn->remote_ipstr = "1.0.0.2";
        ib_conn->remote_port = 65534;
        ib_conn->local_port = 80;
        ib_state_notify_conn_opened(ib_engine, ib_conn);

        sendDataIn("GET / HTTP/1.1\r\nHost: UnitTest\r\n\r\n");
        sendDataOut("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");

        /* Lib htp.c does this, so we do this here. */
        assert(ib_conn->tx!=NULL);
        ib_tx = ib_conn->tx;

        /* Initialize a new lua state. */
        L = luaL_newstate();

        /* Open standard libraries. */
        luaL_openlibs(L);

        appendToSearchPath(IB_XSTRINGIFY(RULE_BASE_PATH));
        appendToSearchPath(IB_XSTRINGIFY(MODULE_BASE_PATH));

        require("ffi", "ffi");
        require("ironbee", "ironbee-ffi");
        require("ibapi", "ironbee-api");

        lua_pushlightuserdata(L, ib_engine);
        lua_setglobal(L, "ib_engine");
        lua_pushlightuserdata(L, ib_tx);
        lua_setglobal(L, "ib_tx");
    
        /* Construct an IB value. */
        eval("ib = ibapi.new(ib_engine, ib_tx)");
    }

    void sendDataIn(const string& req) {
        ib_conndata_t *ib_conndata;
        ib_conn_data_create(ib_conn, &ib_conndata, req.size());
        ib_conndata->dlen = req.size();
        memcpy(ib_conndata->data, req.data(), req.size());
        ib_state_notify_conn_data_in(ib_engine, ib_conndata);
    }

    void sendDataOut(const string& req) {
        ib_conndata_t *ib_conndata;
        ib_conn_data_create(ib_conn, &ib_conndata, req.size());
        ib_conndata->dlen = req.size();
        memcpy(ib_conndata->data, req.data(), req.size());
        ib_state_notify_conn_data_out(ib_engine, ib_conndata);
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

        //ib_state_notify_conn_closed(ib_engine, ib_conn);

        BaseFixture::TearDown();
    }
};

TEST_F(IronBeeLuaApi, log_error)
{
    eval("ib:log_error(\"======== Test Log Message %d ========\", 100)");
}

TEST_F(IronBeeLuaApi, log_debug)
{
    eval("ib:log_debug(\"======== Test Log Message %d ========\", 100)");
}

TEST_F(IronBeeLuaApi, set_string)
{
    const char* key = "key1";
    const char* val = "myStringValue";

    // Call ib:setString("key1", "myStringValue")
    eval(std::string("ib:setString(\"")+key+"\", \""+val+"\")");

    ib_field_t* ib_field;
    ib_data_get_ex(ib_tx->dpi, key, strlen(key), &ib_field);

    ASSERT_TRUE(NULL!=ib_field);
    ASSERT_STREQ(val, static_cast<char*>(ib_field_value(ib_field)));
}

TEST_F(IronBeeLuaApi, get_string)
{
    const char* key = "key2";
    const char* const_value = "myStringValue";
    char* value = (char*)malloc(sizeof(const_value)+1);

    strcpy(value, const_value);
    ib_field_t* ib_field;
    ib_data_add_nulstr_ex(ib_tx->dpi, key, strlen(key), value, &ib_field);
    ib_data_get_ex(ib_tx->dpi, key, strlen(key), &ib_field);

    ASSERT_TRUE(NULL!=ib_field);
    ASSERT_STREQ(const_value, static_cast<char*>(ib_field_value(ib_field)));

    // Run return ib:getString("key2")
    // We require the return to put the value on the lua stack to check.
    eval(std::string("return ib:getString(\"")+key+"\")");
    ASSERT_STREQ(const_value, lua_tostring(L, -1));
    lua_pop(L, 1);

    free(value);
}


