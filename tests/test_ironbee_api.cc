#include <gtest/gtest.h>

#include <string>

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

    virtual void require(const string& namespace, const string& module)
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
