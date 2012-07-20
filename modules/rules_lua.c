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
 *****************************************************************************/

#include "rules_lua_private.h"

#include <ironbee/types.h>

#include <lua.h>

#define THREAD_NAME_BUFFER_SZ 20

ib_status_t ib_lua_load_eval(ib_engine_t *ib, lua_State *L, const char *file)
{
    IB_FTRACE_INIT();

    int lua_rc;

    lua_rc = luaL_loadfile(L, file);

    if (lua_rc != 0) {
        ib_log_error(ib, "Failed to load %s - %s (%d)",
                     file,
                     lua_tostring(L, -1),
                     lua_rc);
        lua_pop(L, -1);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Evaluate the loaded ffi file. */
    lua_rc = lua_pcall(L, 0, 0, 0);

    /* Only check errors if ec is not 0 (LUA_OK). */
    switch(lua_rc) {
        case 0:
            IB_FTRACE_RET_STATUS(IB_OK);
        case LUA_ERRRUN:
            ib_log_error(ib, "Error evaluating file %s - %s",
                         file,
                         lua_tostring(L, -1));
            /* Get error string off of the stack. */
            lua_pop(L, 1);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        case LUA_ERRMEM:
            ib_log_error(ib,
                "Failed to allocate memory during FFI evaluation.");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        case LUA_ERRERR:
            ib_log_error(ib,
                "Error fetching error message during FFI evaluation.");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
#if LUA_VERSION_NUM > 501
        /* If LUA_ERRGCMM is defined, include a custom error for it as well.
          This was introduced in Lua 5.2. */
        case LUA_ERRGCMM:
            ib_log_error(ib,
                "Garbage collection error during FFI evaluation.");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
#endif
        default:
            ib_log_error(ib, "Unexpected error(%d) during FFI evaluation.",
                         lua_rc);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
}

ib_status_t ib_lua_load_func(ib_engine_t *ib,
                             lua_State *L,
                             const char *file,
                             const char *func_name)
{
    IB_FTRACE_INIT();
    ib_status_t ib_rc;

    /* Load (compile) the lua module. */
    ib_rc = luaL_loadfile(L, file);

    if (ib_rc != 0) {
        ib_log_error(ib, "Failed to load file module \"%s\" - %s (%d)",
                     file, lua_tostring(L, -1), ib_rc);

        /* Get error string off the stack. */
        lua_pop(L, 1);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    lua_setglobal(L, func_name);

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_lua_func_eval_int(ib_engine_t *ib,
                                 ib_tx_t *tx,
                                 lua_State *L,
                                 const char *func_name,
                                 int *return_value)
{
    IB_FTRACE_INIT();

    int lua_rc;

    if (!lua_checkstack(L, 5)) {
        ib_log_error_tx(tx,
            "Not enough stack space to call Lua rule %s.", func_name);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Push the function on the stack. Preparation to call. */
    lua_getglobal(L, func_name);

    if (!lua_isfunction(L, -1)) {
        ib_log_error_tx(tx, "Variable \"%s\" is not a LUA function",
                        func_name);

        /* Remove wrong parameter from stack. */
        lua_pop(L, 1);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Create a table for the coming function call. */
    lua_newtable(L);

    lua_pushstring(L, "ib_tx");   /* Push key. */
    lua_pushlightuserdata(L, tx); /* Push value. */
    lua_settable(L, -3);          /* Assign to -3 key -2 and val -1. */

    lua_pushstring(L, "ib_engine"); /* Push key. */
    lua_pushlightuserdata(L, ib);   /* Push value. */
    lua_settable(L, -3);            /* Assign to -3 key -2 and val -1*/

    /* Build an ironbee object. */
    lua_getglobal(L, "ibapi");/* Push ib table (module) onto stack. */
    lua_pushstring(L, "ib");  /* Push the key we will store the result at. */
    lua_pushstring(L, "new"); /* Push the name of the function. */
    lua_gettable(L, -3);      /* Get the ib.new function. */
    lua_getglobal(L, "ibapi");/* Push ib table (module) onto stack for self. */
    lua_pushlightuserdata(L, ib); /* Push ib_engine argument to new. */
    lua_pushlightuserdata(L, tx); /* Push ib_tx argument to new. */
    lua_rc = lua_pcall(L, 3, 1, 0); /* Make new ib api object. */

    if (lua_rc != 0) {
        ib_log_error_tx(tx,
            "Error running Lua Rule %s - %s", func_name, lua_tostring(L, -1));
        /* Pop (1) error string, (2) string "ib", and (3) new table, (4) func */
        lua_pop(L, 4);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* At this point the stack is:
     *  |  rule function  |
     *  | anonymous table |
     *  | ib module table |
     *  |   "ib" string   |
     *  |   new ib obj    |
     * Set the table at -4 the key "ib" = the new ib api object.
     */
    lua_settable(L, -4);

    /* Pop the ib module table off the stack leaving just the
     * user rule function and the anonymous table we are building. */
    lua_pop(L, 1);

    ib_log_debug_tx(tx, "Executing user rule %s", func_name);

    /* Call the function on the stack with 1 input, 0 outputs, and errmsg=0. */
    lua_rc = lua_pcall(L, 1, 1, 0);

    /* Only check errors if ec is not 0 (LUA_OK). */
    if (lua_rc != 0) {
        switch(lua_rc) {
            case LUA_ERRRUN:
                ib_log_error_tx(tx,
                    "Error running Lua Rule %s - %s",
                    func_name,
                    lua_tostring(L, -1));

                /* Get error string off of the stack. */
                lua_pop(L, 1);
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            case LUA_ERRMEM:
                ib_log_error_tx(tx,
                    "Failed to allocate memory during Lua rule.");
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            case LUA_ERRERR:
                ib_log_error_tx(tx,
                    "Error fetching error message during Lua rule.");
                IB_FTRACE_RET_STATUS(IB_EINVAL);
#if LUA_VERSION_NUM > 501
            /* If LUA_ERRGCMM is defined, include a custom error for it as
               well. This was introduced in Lua 5.2. */
            case LUA_ERRGCMM:
                ib_log_error_tx(tx,
                    "Garbage collection error during Lua rule.");
                IB_FTRACE_RET_STATUS(IB_EINVAL);
#endif
            default:
                ib_log_error_tx(tx,
                    "Unexpected error (%d) during Lua rule.", lua_rc);
                IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    /* If there is no error, pull the return value off. */
    *return_value = lua_tointeger(L, -1);
    lua_pop(L, -1);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @brief Print a thread name of @a L into the character buffer @a thread_name.
 * @details @a thread_name should be about 20 characters long
 *          to store a %p formatted pointer of @a L prefixed with @c t_.
 * @param[out] thread_name The buffer the thread name is printed into.
 * @param[in] L The lua_State* whose pointer we will use as the thread name.
 */
static inline void sprint_threadname(char *thread_name, lua_State *L)
{
    sprintf(thread_name, "t_%p", (void *)L);
}

ib_status_t ib_lua_new_thread(ib_engine_t *ib,
                              lua_State *L,
                              lua_State **thread)
{
    IB_FTRACE_INIT();
    char *thread_name = (char *)malloc(THREAD_NAME_BUFFER_SZ);

    ib_log_debug(ib, "Setting up new Lua thread.");

    *thread = lua_newthread(L);

    if (*thread == NULL) {
        ib_log_error(ib, "Failed to allocate new Lua execution stack.");
        free(thread_name);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    sprint_threadname(thread_name, *thread);

    ib_log_debug(ib, "Created Lua thread %s.", thread_name);

    /* Store the thread at the global variable referenced. */
    lua_setglobal(L, thread_name);

    free(thread_name);
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_lua_join_thread(ib_engine_t *ib,
                               lua_State* L,
                               lua_State **thread)
{
    IB_FTRACE_INIT();
    char *thread_name = (char *)malloc(THREAD_NAME_BUFFER_SZ);
    sprint_threadname(thread_name, *thread);

    ib_log_debug(ib, "Tearing down Lua thread %s.", thread_name);

    /* Put nil on the stack. */
    lua_pushnil(L);

    /* Erase the reference to the stack to allow GC. */
    lua_setglobal(L, thread_name);

    free(thread_name);
    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_lua_require(ib_engine_t *ib,
                           lua_State *L,
                           const char* module_name,
                           const char* required_name)
{
    IB_FTRACE_INIT();

    int lua_rc;

    lua_getglobal(L, "require");

    lua_pushstring(L, required_name);

    lua_rc = lua_pcall(L, 1, 1, 0);

    if (lua_rc != 0) {
        ib_log_error(ib, "Require failed %s - %s (%d)",
                     required_name,
                     lua_tostring(L, -1),
                     lua_rc);
        lua_pop(L, -1);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Take the result of require(required_name) on the stack and assign it. */
    lua_setglobal(L, module_name);

    IB_FTRACE_RET_STATUS(IB_OK);
}

void ib_lua_add_require_path(ib_engine_t *ib_engine,
                             lua_State *L,
                             const char *path)
{
    IB_FTRACE_INIT();

    lua_getglobal(L, "package");
    lua_pushstring(L, "path");
    lua_pushstring(L, "path");
    lua_gettable(L, -3);
    lua_pushstring(L, ";");
    lua_pushstring(L, path);
    lua_concat(L, 3);
    lua_settable(L, -3);

    IB_FTRACE_RET_VOID();
}
