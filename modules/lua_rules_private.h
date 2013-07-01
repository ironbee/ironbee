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
 * @brief IronBee --- Lua Rules
 *
 * IronBee Rules as Lua scripts.
 *
 * @author Sam Baskinger <basking2@yahoo.com>
 */
#ifndef __MODULES__LUA_RULES_PRIVATE_H
#define __MODULES__LUA_RULES_PRIVATE_H

#include "lua/ironbee.h"
#include "lua_private.h"
#include "lua_common_private.h"

/**
 * Callback data for the Lua Rules implementation.
 */
struct modlua_rules_cbdata_t {
    ib_module_t *module; /**< Pointer to the current engine's module. */
};
typedef struct modlua_rules_cbdata_t modlua_rules_cbdata_t;

/**
 * Register "lua" as an external rule by ib_rule_register_external_driver().
 *
 * This also builds the @ref modlua_rules_cbdata_t used.
 *
 * @param[in] ib IronBee engine.
 * @param[in] module The module structure passed in the rule calback object.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On error.
 * - Other on registration failure to the engine. See log output.
 */
ib_status_t rules_lua_init(
    ib_engine_t *ib,
    ib_module_t *module
);
#endif /* __MODULES__LUA_RULES_PRIVATE_H */
