#if defined(__cplusplus) && !defined(__STDC_CONSTANT_MACROS)
/* C99 7.18.4 requires that stdint.h only exposes INT64_C 
 *  *  * and UINT64_C for C++ implementations if this is defined: */
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
