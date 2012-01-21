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
#ifndef __MODULES__RULES_LUA_H
#define __MODULES__RULES_LUA_H

#include <ironbee/cfgmap.h>
#include <ironbee/debug.h>
#include <ironbee/engine.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>
#include <ironbee/util.h>
#include <ironbee/list.h>
#include <ironbee/config.h>
#include <ironbee/rule_engine.h>
#include <lua.h>

#include "lua/ironbee.h"
/**
 * @brief Load the ironbee-ffi.lua file into the given Lua state.
 * @param[in] ib IronBee engine used to log.
 * @param[out] L The Lua state to load the file into.
 * @param[in] file File to evaluate.
 * @returns The IronBee status.
 */
ib_status_t ib_lua_load_eval(ib_engine_t *ib, lua_State *L, const char *file);

/**
 * @brief Add a lua rule stored in a file to the Ironbee engine.
 * @param[in,out] ib Used for logging and adding the Lua rule to.
 * @param[in,out] L The Lua state used to load @a file and store the rule.
 * @param[in] func_name The name the contents of the file will be stored under.
 * @param[in] file The file that holds the Lua script that makes up the rule.
 * @returns The IronBee status. IB_OK on success.
 */
ib_status_t ib_lua_load_func(ib_engine_t *ib,
                             lua_State *L,
                             const char *file,
                             const char *func_name);

/**
 * @brief Call the Lua function \a func_name in the @c lua_State @a L and treat it as an IronBee rule.
 * @param[in] ib The IronBee context. Used for logging.
 * @param[in,out] tx The transaction object. This is passed to the rule
 *                as the local variable @c tx.
 * @param[in] L The Lua execution state/stack to use to call the rule.
 * @param[in] func_name The name of the Lua function to call.
 * @returns IronBee status.
 */
ib_status_t ib_lua_func_eval(ib_engine_t *ib,
                             ib_tx_t *tx,
                             lua_State *L,
                             const char *func_name);

/**
 * @brief Spawn a new Lua thread and place a pointer to it in @a L.
 * @details This will create a new Lua state and store a reference to
 *          it in the global state name t_%h where %h is the value of *thread.
 * @param[out] ib The IronBee engine used to log errors.
 * @param[in,out] L The Lua state off of which to create the new thread.
 * @param[out] thread The pointer to the newly created Lua thread.
 * @returns IB_OK on success.
 */
ib_status_t ib_lua_new_thread(ib_engine_t *ib,
                              lua_State *L,
                              lua_State **thread);

/**
 * @brief Destroy a new Lua thread pointed to by @a L.
 * @details This modifies the global state of L by removing the 
 *          reference to the thread name t_%h. The result is that
 *          the thread should be garbage collected.
 * @param[out] ib The IronBee engine used to log errors.
 * @param[in,out] L The Lua state off of which to create the new thread.
 * @param[in] thread The pointer to the thread to schedule for GC.
 * @returns IB_OK on success.
 */
ib_status_t ib_lua_join_thread(ib_engine_t *ib,
                               lua_State* L,
                               lua_State **thread);

#endif // __MODULES__RULES_LUA_H
