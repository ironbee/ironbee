#include <gtest/gtest.h>

#include <string>
#include <stdexcept>
#include "base_fixture.h"

extern "C" {
#include <ironbee/release.h>
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

    ib_tx_t ib_tx;

    /**
     * Calls BaseFixture::SetUp(), then creates a new Lua State,
     * loads ffi, ironbee-ffi, and ironbee-api, and then sets ib_engine
     * to a copy of the ironbee engine.
     */
    virtual void SetUp()
    {
        BaseFixture::SetUp();

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
        lua_pushlightuserdata(L, &ib_tx);
        lua_setglobal(L, "ib_tx");
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
        BaseFixture::TearDown();
    }
};

TEST_F(IronBeeLuaApi, log_error)
{
    eval("ib = ibapi.new(ib_engine, ib_tx)");
    eval("ib:log_error(\"======== Test Log Message %d ========\", 100)");
}
