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

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee --- Lua Test Fixture.
///
/// @author Sam Baskinger <sbaskinger@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#ifndef __IBTEST_LUA_HPP__
#define __IBTEST_LUA_HPP__

#include <lua.hpp>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <stdexcept>

namespace ibtesting {

/**
 * Unless commit() is called, this resets the Lua stack at destruction time.
 *
 * This object *only* resets the Lua stack size. It does not
 * consider the Lua runtime state.
 */
class LuaStackTx {
private:
    //! Lua stack to operate on.
    lua_State *m_L;

    //! The height to set the Lua stack to on object destruction or commit().
    int m_stack_top;
public:
    //! Record this stack and its size in order to rollback.
    LuaStackTx(lua_State *L) : m_L(L), m_stack_top(lua_gettop(L)) { }

    //! Clear the stack by setting it size to what it was at obj creation time.
    void rollback() { lua_settop(m_L, m_stack_top); }

    //! Return how much the stack has grown.
    int growth() { return lua_gettop(m_L) - m_stack_top; }

    //! Commit the Lua stack state.
    void commit() { m_stack_top = lua_gettop(m_L); }

    //! Destroy this object by calling rollback().
    ~LuaStackTx() { rollback(); }
};

class LuaTest : public ::testing::Test {
    protected:
    lua_State *L;

    public:
    LuaTest(): L(NULL) {}
    ~LuaTest() {}

    virtual void SetUp()
    {
        L = luaL_newstate();
        luaL_openlibs(L);
    }

    virtual void TearDown()
    {
        lua_settop(L, 0);
        lua_close(L);
    }

    /**
     * Load and run the string as Lua code.
     *
     * If a runtime error is detected, it is wrapped in a std::runtime_error
     * object and thrown with the Lua error string as the message..
     *
     * @param[in] code Lua code to evaluate. This should expect no
     *            arguments but may return any number of arguments.
     *            All arguments are cleared off the stack when returning.
     * @throws std::runtime_error.
     * @returns The number of new arguments on the Lua stack.
     */
    int doString(std::string code) {
        int rc;
        int new_args;

        /* When this is destroyed, clear/reset the stack top. */
        LuaStackTx ltx(L);

        rc = luaL_loadstring(L, code.c_str());
        switch(rc) {
            case 0:
                /* No error. */
                break;
            case LUA_ERRSYNTAX:
                std::cerr << lua_tostring(L, -1) << std::endl;
                throw std::runtime_error(lua_tostring(L, -1));
            case LUA_ERRMEM:
                std::cerr << lua_tostring(L, -1) << std::endl;
                throw std::runtime_error(lua_tostring(L, -1));
            default:
                assert(0 && "Unhandled error condition.");
        }

        rc = lua_pcall(L, 0, LUA_MULTRET, 0);
        switch(rc) {
            case 0:
                /* No error. */
                break;
            case LUA_ERRRUN:
                std::cerr << lua_tostring(L, -1) << std::endl;
                throw std::runtime_error(lua_tostring(L, -1));
            case LUA_ERRMEM:
                std::cerr << lua_tostring(L, -1) << std::endl;
                throw std::runtime_error(lua_tostring(L, -1));
            case LUA_ERRERR:
                std::cerr << lua_tostring(L, -1) << std::endl;
                throw std::runtime_error(lua_tostring(L, -1));
            default:
                assert(0 && "Unhandled error condition.");
        }

        // Success! Leave the Lua stack alone.
        new_args = ltx.growth();
        ltx.commit();
        return new_args;
    }

    void cpathAppend(std::string cpath) {
        doString("package.cpath = package.cpath .. \";" + cpath + "\"");
    }

    void pathAppend(std::string path) {
        doString("package.path = package.path .. \";" + path + "\"");
    }
};
}
#endif // __IBTEST_LUA_HPP__
