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
 * @brief IronBee --- Lua Integration.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#ifndef __MODULES__LUA_COMMON_H
#define __MODULES__LUA_COMMON_H

#include "lua/ironbee.h"

#include <ironbee/cfgmap.h>
#include <ironbee/config.h>
#include <ironbee/engine.h>
#include <ironbee/list.h>
#include <ironbee/rule_engine.h>
#include <ironbee/util.h>

#include <lua.h>

/**
 * Load the lua file into the given Lua state and execute it with no args.
 *
 * @param[in] ib IronBee engine used to log.
 * @param[out] L The Lua state to load the file into.
 * @param[in] file File to evaluate.
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
ib_status_t ib_lua_load_eval(ib_engine_t *ib, lua_State *L, const char *file);

/**
 * Add a lua rule stored in a file to the Ironbee engine.
 *
 * @param[in,out] ib Used for logging and adding the Lua rule to.
 * @param[in,out] L The Lua state used to load @a file and store the rule.
 * @param[in] func_name The name the contents of the file will be stored
 *                      under.
 * @param[in] file The file that holds the Lua script that makes up the rule.
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
ib_status_t ib_lua_load_func(
    ib_engine_t *ib,
    lua_State   *L,
    const char  *file,
    const char  *func_name
);

/**
 * Call the Lua function @a func_name in the @c lua_State @a L and treat it
 * as an IronBee rule.
 *
 * @param[in] ib The IronBee context. Used for logging.
 * @param[in,out] tx The transaction object. This is passed to the rule
 *                as the local variable @c tx.
 * @param[in] L The Lua execution state/stack to use to call the rule.
 * @param[in] func_name The name of the Lua function to call.
 * @param[out] return_value The integer that this function evaluates to.
 *
 * @returns IronBee status.
 */
ib_status_t ib_lua_func_eval_int(
    ib_engine_t *ib,
    ib_tx_t     *tx,
    lua_State   *L,
    const char  *func_name,
    int         *return_value
);

/**
 * Load a Lua module into @a L as the module named @a module_name.
 *
 * This is equivalent to module_name = require(required_name)
 * in Lua.
 *
 * @param[out] ib The IronBee engine used to log errors.
 * @param[in,out] L The Lua state that evaluates and holds the result.
 * @param[in] module_name The name of the module after it has been required.
 * @param[in] required_name The argument to the @c require function in Lua.
 *            Typically a file in Lua's search path.
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
ib_status_t ib_lua_require(
    ib_engine_t *ib,
    lua_State   *L,
    const char  *module_name,
    const char  *required_name
);

/**
 * Append the given path to Lua's package.path variable.
 *
 * This is used so that users can easily extend where Lua searches for
 * modules to load. Strings should be full lua search paths such as
 * @c /my/path/?.lua.
 *
 * @param[in] ib_engine The engine used for logging.
 * @param[in,out] L The Lua state to be modified.
 * @param[in] path The search path to be appended to package.path.
 *
 */
void ib_lua_add_require_path(
    ib_engine_t *ib_engine,
    lua_State   *L,
    const char  *path
);

#endif /* __MODULES__LUA_COMMON_H */
