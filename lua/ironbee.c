#include "ironbee_config_auto.h"

#include <string.h>

#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/module.h>

#include "ironbee.h"

/**
 * @internal
 *
 * Register the module table with ironbee.
 *
 * Currently a module MUST call this to register itself with the
 * ironbee engine.
 *
 * @todo Really need a way to not have to do this.
 *
 * EX:
 *   local modname = "example"
 *   local ironbee = require("ironbee")
 *   ...
 *   module(modname)
 *   ironbee.register_module(_M)
 *
 * Lua parameter stack:
 *  1) module table
 *
 * @param L Lua state
 * @retval 1 On success
 */
static int register_module(lua_State *L)
{
    if ((lua_gettop(L) != 1) || !lua_istable(L, 1)) {
        fprintf(stderr,
                "The register_module function takes a single table arg.\n");
        fflush(stderr);
        return 0;
    }

    /// @todo Right now, just set a global var until I figure out a better way.

    lua_pushvalue(L, 1);
    lua_setglobal(L, "ironbee-module");

    return 1;
}

/**
 * @internal
 *
 * Log to debug log
 *
 * Lua parameter stack:
 *  1) engine handle
 *  2) level
 *  3) format
 *  4) ...
 *
 * @param L Lua state
 */
static int log_debug(lua_State *L)
{
    ib_engine_t *ib = (ib_engine_t *)lua_topointer(L, 1);
    int level = luaL_checkint(L, 2);
    int nargs = lua_gettop(L);
    const char *msg;
    int ec;

    /*
     * Call string.format() to do the actual formatting.
     *
     * This is done as lua cannot bind a vararg C function.  Instead,
     * this reorganizes the stack, replacing the "level" arg with the
     * format function, then calls string.format with the remaining args.
     * This allows string.format to do the formatting so that a single
     * string arg can be passed to the underlying ironbee log function.
     */
    /// @todo Store the format function for faster access???
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "format"); /* string.format() */
    lua_replace(L, 2); /* replace the "level" arg with format function */
    lua_pop(L, 1); /* cleanup the stack used to find string table */
    ec = lua_pcall(L, (nargs - 2), 1, 0); /* format(fmt, ...) */
    if (ec != 0) {
        ib_log_error(ib, 1, "Failed to exec string.format - %s (%d)",
                     lua_tostring(L, -1), ec);
        return 0;
    }
    msg = lua_tostring(L, -1); /* formatted string */

    /* Call the ironbee API with the formatted message. */
    ib_log_debug(ib, level, msg);

    return 1;
}

/**
 * @internal
 *
 * Map Lua methods to C functions.
 */
static const struct luaL_Reg ironbee_lib[] = {
    { "register_module", register_module },
    { "log_debug", log_debug },
    { NULL, NULL }
};

int luaopen_ironbee(lua_State *L)
{
    /* Create the module environment table for encapsulating
     * private (module wide) data.
     */
    lua_newtable(L);
    lua_replace(L, LUA_ENVIRONINDEX);

    luaL_register(L, IRONBEE_NAME, ironbee_lib);

    return 1;
}
