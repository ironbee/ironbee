#include <gtest/gtest.h>

#include <string>
#include <stdexcept>

extern "C" {
#include <ironbee/release.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

using namespace std;

class IronBeeLuaApi : public ::testing::Test {
public:

    lua_State* L;

    virtual void SetUp()
    {
        /* Initialize a new lua state. */
        L = luaL_newstate();

        /* Open standard libraries. */
        luaL_openlibs(L);

        appendToSearchPath(IB_XSTRINGIFY(RULE_BASE_PATH));
        appendToSearchPath(IB_XSTRINGIFY(MODULE_BASE_PATH));
    }

    virtual void require(const string& name, const string& module)
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

    virtual void appendToSearchPath(const string& path)
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

    virtual void TearDown()
    {
        lua_close(L);
    }
};

TEST_F(IronBeeLuaApi, test001) {
}
