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
 * @brief Base IronBee C header data for Lua Integration.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */


#if defined(__cplusplus) && !defined(__STDC_CONSTANT_MACROS)
/* C99 7.18.4 requires that stdint.h only exposes INT64_C
 * and UINT64_C for C++ implementations if this is defined: */
#define __STDC_CONSTANT_MACROS
#endif
#include <stdint.h>

#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#ifndef _LUA_IRONBEE_H_
#define _LUA_IRONBEE_H_

#define IRONBEE_NAME "ironbee"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ironbee_t ironbee_t;

struct ironbee_t {
    size_t     len;
    uint8_t    data[1];
};

/**
 * Called to register with Lua when library is opened.
 *
 * @param L Lua state
 * @retval 1 On success (value is on stack)
 */
int luaopen_ironbee(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif /* _LUA_IRONBEE_H_ */
