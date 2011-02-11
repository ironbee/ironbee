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

/**
 * @file
 * @brief IronBee - LUA Module
 *
 * This module integrates liblua.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#include <ironbee/ironbee.h>
#include <ironbee/util.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>

#include "lua/ironbee.h"

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 *  * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/// @todo Fix this:
#ifndef X_MODULE_BASE_PATH
#define X_MODULE_BASE_PATH IB_XSTRINGIFY(MODULE_BASE_PATH) "/"
#endif

/* -- Module Setup -- */

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        lua
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

#define MODLUA_CONN_KEY "lua-runtime"


/* Define the public module symbol. */
IB_MODULE_DECLARE();

typedef struct modlua_chunk_t modlua_chunk_t;
typedef struct modlua_chunk_fp_tracker_t modlua_chunk_fp_tracker_t;
typedef struct modlua_chunk_tracker_t modlua_chunk_tracker_t;
typedef struct modlua_cpart_t modlua_cpart_t;
typedef struct modlua_reg_t modlua_reg_t;
typedef struct modlua_runtime_t modlua_runtime_t;
typedef struct modlua_cfg_t modlua_cfg_t;

/** Lua Module Binary Data Chunk */
struct modlua_chunk_t {
    ib_engine_t        *ib;           /**< Engine */
    ib_mpool_t         *mp;           /**< Pool to allocate from */
    const char         *name;         /**< Name for debug */
    ib_array_t         *cparts;       /**< Chunk (array of chunk parts) */
};

/** Structure to track file reader chunk parts */
struct modlua_chunk_fp_tracker_t {
    modlua_chunk_t     *chunk;        /**< The chunk that is being loaded */
    FILE               *fp;           /**< File pointer */
    char                buf[BUFSIZ];  /**< The read buffer */
};

/** Structure to track chunk parts while reading */
struct modlua_chunk_tracker_t {
    modlua_chunk_t     *chunk;        /**< The chunk that is being read */
    size_t              part;         /**< The current part index */

};

/** Lua Module Binary Data in Parts */
struct modlua_cpart_t {
    uint8_t            *data;         /**< Data */
    size_t              dlen;         /**< Data length */
};

/** Lua runtime */
struct modlua_runtime_t {
    lua_State          *L;            /**< Lua stack */
};

/** Module Configuration Structure */
struct modlua_cfg_t {
    ib_list_t          *lua_modules;
    ib_list_t          *event_reg[IB_STATE_EVENT_NUM + 1];
};

/* Instantiate a module global configuration. */
static modlua_cfg_t modlua_global_cfg;

static ib_status_t modlua_context_init(ib_engine_t *ib,
                                       ib_context_t *ctx);

/* -- Lua Routines -- */

static const char *modlua_file_loader(lua_State *L,
                                      void *udata,
                                      size_t *size)
{
    IB_FTRACE_INIT(modlua_reader);
    modlua_chunk_fp_tracker_t *tracker = (modlua_chunk_fp_tracker_t *)udata;
    modlua_chunk_t *chunk = tracker->chunk;
    ib_engine_t *ib = chunk->ib;
    modlua_cpart_t *cpart;
    ib_status_t rc;

    if (feof(tracker->fp)) {
        return NULL;
    }

    /* Read a chunk part. */
    *size = fread(tracker->buf, 1, sizeof(tracker->buf), tracker->fp);

    ib_log_debug(ib, 4, "Lua loading part size=%" PRIuMAX, *size);

    /* Add a chunk part to the list. */
    cpart = (modlua_cpart_t *)ib_mpool_alloc(chunk->mp, sizeof(*cpart));
    if (cpart == NULL) {
        IB_FTRACE_RET_CONSTSTR(NULL);
    }
    cpart->data = ib_mpool_alloc(chunk->mp, *size);
    if (cpart->data == NULL) {
        IB_FTRACE_RET_CONSTSTR(NULL);
    }
    cpart->dlen = *size;
    memcpy(cpart->data, tracker->buf, *size);
    rc = ib_array_appendn(chunk->cparts, cpart);
    if (rc != IB_OK) {
        IB_FTRACE_RET_CONSTSTR(NULL);
    }

    IB_FTRACE_RET_CONSTSTR((const char *)cpart->data);
}

static const char *modlua_reader(lua_State *L,
                                 void *udata,
                                 size_t *size)
{
    IB_FTRACE_INIT(modlua_reader);
    modlua_chunk_tracker_t *tracker = (modlua_chunk_tracker_t *)udata;
    modlua_chunk_t *chunk = tracker->chunk;
    //ib_engine_t *ib = chunk->ib;
    modlua_cpart_t *cpart;
    ib_status_t rc;

    rc = ib_array_get(chunk->cparts, tracker->part, &cpart);
    if (rc != IB_OK) {
        //ib_log_error(ib, 4, "No more chunk parts to read: %d", rc);
        return NULL;
    }
    
    *size = cpart->dlen;

    //ib_log_debug(ib, 9, "Lua reading part=%d size=%" PRIuMAX, (int)tracker->part, *size);

    tracker->part++;

    IB_FTRACE_RET_CONSTSTR((const char *)cpart->data);
}

static int modlua_writer(lua_State *L,
                         const void *data,
                         size_t size,
                         void *udata)
{
    IB_FTRACE_INIT(modlua_writer);
    modlua_chunk_t *chunk = (modlua_chunk_t *)udata;
    fprintf(stderr, "CHUNK: %p\n", (void *)chunk);
    fflush(stderr);
    //ib_engine_t *ib = chunk->ib;
    modlua_cpart_t *cpart;
    ib_status_t rc;

    //ib_log_debug(ib, 9, "Lua writing part size=%" PRIuMAX, size);

    /* Add a chunk part to the list. */
    cpart = (modlua_cpart_t *)ib_mpool_alloc(chunk->mp, sizeof(*cpart));
    if (cpart == NULL) {
        IB_FTRACE_RET_INT(-1);
    }
    cpart->data = ib_mpool_alloc(chunk->mp, size);
    if (cpart->data == NULL) {
        IB_FTRACE_RET_INT(-1);
    }
    cpart->dlen = size;
    memcpy(cpart->data, data, size);
    rc = ib_array_appendn(chunk->cparts, cpart);
    if (rc != IB_OK) {
        IB_FTRACE_RET_INT(-1);
    }

    IB_FTRACE_RET_INT(0);
}


/* -- Lua Wrappers -- */

static int modlua_load(ib_engine_t *ib, lua_State *L, modlua_chunk_t *chunk)
{
    IB_FTRACE_INIT(modlua_load);
    modlua_chunk_tracker_t tracker;
    int ec;

    tracker.chunk = chunk;
    tracker.part = 0;
    //ib_log_debug(ib, 9, "Loading chunk=%p", chunk);
    ec = lua_load(L, modlua_reader, &tracker, chunk->name);

    IB_FTRACE_RET_INT(ec);
}

static ib_status_t modlua_load_ironbee_module(ib_engine_t *ib,
                                              lua_State *L)
{
    IB_FTRACE_INIT(modlua_load_ironbee_module);
    ib_status_t rc = IB_OK;
    int ec;

    /* Preload ironbee module (static link). */
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    lua_pushcfunction(L, luaopen_ironbee);
    lua_setfield(L, -2, "ironbee");
    lua_getfield(L, -1, "ironbee");
    lua_pushstring(L, "ironbee");
    ec = lua_pcall(L, 1, 0, 0);
    if (ec != 0) {
        ib_log_error(ib, 1, "Failed to load ironbee lua module - %s (%d)",
                     lua_tostring(L, -1), ec);
        rc = IB_EINVAL;
    }
    lua_pop(L, 2); /* cleanup stack */

    IB_FTRACE_RET_STATUS(rc);
}

/// @todo Neet to call this with a context
static ib_status_t modlua_register_event_handler(ib_engine_t *ib,
                                                 ib_context_t *ctx,
                                                 const char *event_name,
                                                 ib_module_t *m)
{
    IB_FTRACE_INIT(modlua_register_event_handler);
    ib_mpool_t *pool = ib_engine_pool_config_get(ib);
    modlua_cfg_t *modcfg;
    ib_state_event_type_t event;
    ib_status_t rc;

    /* Get the module config. */
    rc = ib_context_module_config(ctx, &IB_MODULE_SYM, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to fetch module %s config: %d",
                     MODULE_NAME_STR, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    if (strcmp("HandleRequestHeaders", event_name) == 0) {
        event = handle_request_headers_event;
    }
    else {
        ib_log_debug(ib, 4, "Unhandled event %s", event_name);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_log_debug(ib, 9, "Registering lua event handler m=%p event=%d: onEvent%s",
                 m, event, event_name);

    /* Create an event list if required. */
    if (modcfg->event_reg[event] == NULL) {
        rc = ib_list_create(modcfg->event_reg + event, pool);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }

    /* Add the lua module to the lua event list. */
    ib_log_debug(ib, 9, "Adding module=%p to event=%d list=%p",
                 m, event, modcfg->event_reg[event]);
    rc = ib_list_push(modcfg->event_reg[event], (void *)m);

    IB_FTRACE_RET_STATUS(rc);
}

/// @todo This should be triggered by directive.
static ib_status_t modlua_module_load(ib_engine_t *ib,
                                      ib_module_t **pm,
                                      const char *file)
{
    IB_FTRACE_INIT(modlua_module_load);
    ib_mpool_t *pool = ib_engine_pool_config_get(ib);
    modlua_chunk_t *chunk;
    char *name;
    char *name_start;
    char *name_end;
    lua_State *L;
    ib_module_t *m;
    ib_status_t rc;
    int ec;

    if (pm != NULL) {
        *pm = NULL;
    }

    /* Figure out the name based on the file. */
    /// @todo Need a better way - get the name like we do in init
    name_start = rindex(file, '/');
    if (name_start == NULL) {
        name_start = (char *)file;
    }
    else {
        name_start++;
    }
    name_end = index(name_start, '.');
    if (name_end == NULL) {
        name_end = index(name_start, 0);
    }
    name = ib_mpool_alloc(pool, (name_end - name_start) + 1);
    if (name == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    memcpy(name, name_start, (name_end - name_start));


    /* Setup a fresh new Lua state to load each module. */
    L = lua_open();
    luaL_openlibs(L);

    /* Preload ironbee module (static link). */
    rc = modlua_load_ironbee_module(ib, L);
    if (rc != IB_OK) {
        lua_close(L);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_log_debug(ib, 4, "Loading lua module \"%s\": %s",
                 name, file);

    /* Save the Lua chunk. */
    ib_log_debug(ib, 4, "Allocating chunk");
    chunk = (modlua_chunk_t *)ib_mpool_alloc(pool, sizeof(*chunk));
    if (chunk == NULL) {
        lua_close(L);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    chunk->ib = ib;
    chunk->mp = ib_engine_pool_config_get(ib);
    chunk->name = name;

    ib_log_debug(ib, 4, "Creating array for chunk parts");
    /// @todo Make this initial size configurable
    rc = ib_array_create(&chunk->cparts, pool, 32, 32);
    if (rc != IB_OK) {
        lua_close(L);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Check for luajit, which does not implement lua_dump() and thus
     * must store the source vs the bytecode.
     */
    lua_getglobal(L, "jit");
    if (lua_istable(L, -1)) {
        modlua_chunk_fp_tracker_t tracker;

        ib_log_debug(ib, 4, "Using luajit without precompilation.");

        /* Load (compile) the module, also saving the source for later use. */
        tracker.chunk = chunk;
        tracker.fp = fopen(file, "r");
        if (tracker.fp == NULL) {
            ib_log_error(ib, 1, "Failed to load lua module \"%s\" - %s (%d)",
                         file, strerror(errno), errno);
            lua_close(L);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        ec = lua_load(L, modlua_file_loader, &tracker, name);
        if ((ec != 0) || ferror(tracker.fp)) {
            ib_log_error(ib, 1, "Failed to load lua module \"%s\" - %s (%d)",
                         file, strerror(errno), errno);
            fclose(tracker.fp);
            lua_close(L);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        fclose(tracker.fp);
    }
    else {
        ib_log_debug(ib, 4, "Using precompilation via lua_dump.");

        /* Load (compile) the lua module. */
        ec = luaL_loadfile(L, file);
        if (ec != 0) {
            ib_log_error(ib, 1, "Failed to load lua module \"%s\" - %s (%d)",
                         file, lua_tostring(L, -1), ec);
            lua_close(L);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        ec = lua_dump(L, modlua_writer, chunk);
        if (ec != 0) {
            ib_log_error(ib, 1, "Failed to save lua module \"%s\" - %s (%d)",
                         file, lua_tostring(L, -1), ec);
            lua_close(L);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    ib_log_debug(ib, 4, "Executing module code");
    lua_pushstring(L, name);
    ec = lua_pcall(L, 1,0,0);
    if (ec != 0) {
        ib_log_error(ib, 1, "Failed to run lua module \"%s\" - %s (%d)",
                     file, lua_tostring(L, -1), ec);
        lua_close(L);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    lua_pop(L, 1); /* cleanup "jit" on stack */


    /* Create the Lua module as if it was a normal module. */
    ib_log_debug(ib, 4, "Creating lua module structure");
    rc = ib_module_create(&m, ib);
    if (rc != IB_OK) {
        lua_close(L);
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug(ib, 4, "Init lua module structure");
    IB_MODULE_INIT_DYNAMIC(
        m,                              /**< Module */
        file,                           /**< Module code filename */
        chunk,                          /**< Module data */
        ib,                             /**< Engine */
        name,                           /**< Module name */
        NULL,                           /**< Global config data */
        0,                              /**< Global config data length */
        NULL,                           /**< Configuration field map */
        NULL,                           /**< Config directive map */
        NULL,                           /**< Initialize function */
        NULL,                           /**< Finish function */
        NULL                            /**< Context init function */
    );

    /* Initialize and register the new lua module with the engine. */
    ib_log_debug(ib, 4, "Init lua module");
    rc = ib_module_init(m, ib);

    /* Shutdown Lua. */
    lua_close(L);

    if (pm != NULL) {
        *pm = m;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/// @todo This should be triggered by directive.
static ib_status_t modlua_module_init(ib_engine_t *ib,
                                      ib_context_t *ctx,
                                      const char *name)
{
    IB_FTRACE_INIT(modlua_module_init);
    ib_mpool_t *pool = ib_engine_pool_config_get(ib); /// @todo config pool???
    lua_State *L;
    ib_module_t *m;
    modlua_cfg_t *modcfg;
    modlua_chunk_t *chunk;
    ib_status_t rc;
    int ec;

    /* Get the module config. */
    rc = ib_context_module_config(ctx, &IB_MODULE_SYM, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to fetch module %s config: %d",
                     MODULE_NAME_STR, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug(ib, 4, "Init lua module ctx=%p: %s", ctx, name);

    /* Setup a fresh new Lua state to load each module. */
    L = lua_open();
    luaL_openlibs(L);

    /* Preload ironbee module (static link). */
    modlua_load_ironbee_module(ib, L);

    /* Lookup the module. */
    rc = ib_engine_module_get(ib, name, &m);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Track which modules are used in this context. */
    if (modcfg->lua_modules == NULL) {
        rc = ib_list_create(&modcfg->lua_modules, pool);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }
    ib_list_push(modcfg->lua_modules, m);

    /* Get the lua chunk for this module. */
    chunk = (modlua_chunk_t *)m->data;
    ib_log_debug(ib, 9, "Lua module \"%s\" module=%p chunk=%p", name, m, chunk);

    /* Load the module lua code. */
    ec = modlua_load(ib, L, chunk);
    if (ec != 0) {
        ib_log_error(ib, 1, "Failed to init lua module \"%s\" - %s (%d)",
                     name, lua_tostring(L, -1), ec);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /*
     * Execute the Lua chunk to load the module into the Lua universe.
     * Then, analyze the exported functions, registering them
     * with the engine as required.
     *
     * Function prefixes are as follows:
     *
     *   onEvent: These are event handlers
     */
    ib_log_debug(ib, 9, "Executing lua chunk=%p", chunk);
    lua_pushstring(L, m->name);
    ec = lua_pcall(L, 1, 0, 0);
    if (ec != 0) {
        ib_log_error(ib, 1, "Failed to execute lua module \"%s\" - %s (%d)",
                     name, lua_tostring(L, -1), ec);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Currently a module must set the global "ironbee-module" variable
     * on load.
     */
    /// @todo Fix this.  Probably need to override the loader so that
    ///       we can just "require" with a name?  Or maybe just have
    ///       the module call a defined function (ironbee.register_module)???
    ///       In any case, we just need the freaking name or table :(
    lua_getglobal(L, "ironbee-module");
    ib_log_debug(ib, 9, "Module load returned type=%s",
                 lua_typename(L, lua_type(L, -1)));
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            const char *key = NULL;
            const char *val = NULL;
            if (lua_isstring(L, -2)) {
                key = lua_tostring(L, -2);
                if (lua_isstring(L, -1)) {
                    val = lua_tostring(L, -1);
                    ib_log_debug(ib, 4, "Lua module \"%s\" %s=\"%s\"",
                                 m->name, key, val);
                }
                else if (lua_isfunction(L, -1)) {
                    val = lua_topointer(L, -1);

                    /* If it is an onEvent function, then register the
                     * function as a lua event handler.
                     */
                    if (strncmp("onEvent", key, 7) == 0) {
                        ib_log_debug(ib, 4,
                                     "Lua module \"%s\" registering event "
                                     "handler: %s",
                                     m->name, key);

                        /* key + 7 it the event name following "onEvent" */
                        rc = modlua_register_event_handler(ib, ctx, key + 7, m);
                        if (rc != IB_OK) {
                            ib_log_error(ib, 3,
                                         "Failed to register "
                                         "lua event handler \"%s\": %d",
                                         key, rc);
                            IB_FTRACE_RET_STATUS(rc);
                        }
                    }
                    else {
                        ib_log_debug(ib, 4, "KEY:%s; VAL:%p", key, val);
                    }
                }
            }
            lua_pop(L, 1);
        }
    }

    lua_close(L);

    IB_FTRACE_RET_STATUS(IB_OK);
}


/**
 * @internal
 * Get the lua runtime from the connection.
 *
 * @param conn Connection
 *
 * @returns Lua runtime
 */
static modlua_runtime_t *modlua_runtime_get(ib_conn_t *conn)
{
    IB_FTRACE_INIT(modlua_runtime_get);
    modlua_runtime_t *lua;

    ib_hash_get(conn->data, MODLUA_CONN_KEY, &lua);

    IB_FTRACE_RET_PTR(modlua_runtime_t, lua);
}



/* -- Event Handlers -- */

/**
 * @internal
 * Initialize the lua runtime for this connection.
 *
 * @param ib Engine
 * @param conn Connection
 * @param cbdata Unused
 *
 * @return Status code
 */
static ib_status_t modlua_init_lua_runtime(ib_engine_t *ib,
                                           ib_conn_t *conn,
                                           void *cbdata)
{
    IB_FTRACE_INIT(modlua_init_lua_runtime);
    lua_State *L;
    modlua_cfg_t *modcfg;
    modlua_runtime_t *lua;
    ib_list_node_t *node;
    ib_status_t rc;

    /* Get the module config. */
    rc = ib_context_module_config(conn->ctx, &IB_MODULE_SYM, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to fetch module %s config: %d",
                     MODULE_NAME_STR, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Setup a fresh Lua state for this connection. */
    ib_log_debug(ib, 4, "Initializing lua runtime for conn=%p", conn);
    L = lua_open();
    luaL_openlibs(L);

    /* Create the lua runtime and store it with the connection. */
    ib_log_debug(ib, 4, "Creating lua runtime for conn=%p", conn);
    lua = ib_mpool_alloc(conn->mp, sizeof(*lua));
    if (lua == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    lua->L = L;
    rc = ib_hash_set(conn->data, MODLUA_CONN_KEY, lua);
    ib_log_debug(ib, 9, "Setting lua runtime for conn=%p lua=%p", conn, lua);
    if (rc != IB_OK) {
        ib_log_debug(ib, 3, "Failed to set lua runtime: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Preload ironbee module (static link). */
    modlua_load_ironbee_module(ib, L);

    /* Run through each lua module to be used in this context and
     * load it into the lua runtime.
     */
    IB_LIST_LOOP(modcfg->lua_modules, node) {
        ib_module_t *m = (ib_module_t *)ib_list_node_data(node);
        modlua_chunk_t *chunk = (modlua_chunk_t *)m->data;
        int ec;

        ib_log_debug(ib, 4, "Loading lua module \"%s\" into runtime for conn=%p", m->name, conn);
        rc = modlua_load(ib, L, chunk);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }

        ib_log_debug(ib, 9, "Executing lua chunk=%p", chunk);
        lua_pushstring(L, m->name);
        ec = lua_pcall(L, 1, 0, 0);
        if (ec != 0) {
            ib_log_error(ib, 1, "Failed to execute lua module \"%s\" - %s (%d)",
                         m->name, lua_tostring(L, -1), ec);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Destroy the lua runtime for this connection.
 *
 * @param ib Engine
 * @param conn Connection
 * @param cbdata Unused
 *
 * @return Status code
 */
static ib_status_t modlua_destroy_lua_runtime(ib_engine_t *ib,
                                              ib_conn_t *conn,
                                              void *cbdata)
{
    IB_FTRACE_INIT(modlua_destroy_lua_runtime);
    modlua_runtime_t *lua;
    ib_status_t rc;

    ib_log_debug(ib, 4, "Destroying lua runtime for conn=%p", conn);

    lua = modlua_runtime_get(conn);
    if (lua != NULL) {
        lua_close(lua->L);
    }

    rc = ib_hash_remove(conn->data, MODLUA_CONN_KEY, NULL);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t modlua_exec_lua_handler(ib_engine_t *ib,
                                           void *arg,
                                           modlua_runtime_t *lua,
                                           const char *modname,
                                           ib_state_event_type_t event)
{
    IB_FTRACE_INIT(modlua_exec_lua_handler);
    lua_State *L = lua->L;
    const char *funcname = NULL;
    ib_status_t rc = IB_OK;
    int ec;

    switch(event) {
        case handle_request_headers_event:
            funcname = "onEventHandleRequestHeaders";
            break;
        default:
            IB_FTRACE_RET_STATUS(IB_EINVAL);
            break;
    }

    lua_checkstack(L, 10);
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_getfield(L, -1, modname);
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, funcname);
        if (lua_isfunction(L, -1)) {
            ib_log_debug(ib, 4, "Executing lua handler \"%s.%s\"",
                         modname, funcname);
            lua_pushlightuserdata(L, ib);
            lua_pushlightuserdata(L, arg);
            lua_pushnil(L);
            ec = lua_pcall(L, 3, 1, 0);
            if (ec != 0) {
                ib_log_error(ib, 1, "Failed to exec lua \"%s.%s\" - %s (%d)",
                             modname, funcname, lua_tostring(L, -1), ec);
                rc = IB_EINVAL;
            }
            else if (lua_isnumber(L, -1)) {
                rc = (ib_status_t)(int)lua_tointeger(L, -1);
            }
            else {
                ib_log_error(ib, 1,
                             "Expected %s returned from lua \"%s.%s\", "
                             "but received %s",
                             lua_typename(L, LUA_TNUMBER),
                             modname, funcname,
                             lua_typename(L, lua_type(L, -1)));
                rc = IB_EINVAL;
            }
            lua_pop(L, 1); /* cleanup return val on stack */
        }
        else {
            ib_log_debug(ib, 4, "Function lookup returned type=%s",
                         lua_typename(L, lua_type(L, -1)));
        }
    }
    else {
        ib_log_debug(ib, 4, "Module lookup returned type=%s",
                     lua_typename(L, lua_type(L, -1)));
    }
    lua_pop(L, 3); /* cleanup stack */

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @internal
 * Generic event handler for Lua connection data events.
 *
 * @param ib Engine
 * @param conndata Connection data
 * @param cbdata Event number passed as pointer value
 *
 * @return Status code
 */
static ib_status_t modlua_handle_lua_conndata_event(ib_engine_t *ib,
                                                    ib_conndata_t *conndata,
                                                    void *cbdata)
{
    IB_FTRACE_INIT(modlua_handle_lua_conndata_event);
    ib_conn_t *conn = conndata->conn;
    ib_state_event_type_t event;
    modlua_cfg_t *modcfg;
    modlua_runtime_t *lua;
    ib_list_t *luaevents;
    ib_list_node_t *node;
    ib_status_t rc;

    /* Get the module config. */
    rc = ib_context_module_config(conn->ctx,
                                  &IB_MODULE_SYM, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to fetch module %s config: %d",
                     MODULE_NAME_STR, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Verify cbdata is in range for an event. */
    if ((uintptr_t)cbdata >= IB_STATE_EVENT_NUM) {
        ib_log_error(ib, 3, "Lua event was out of range: %" PRIxMAX,
                     (uintptr_t)cbdata);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    event = (ib_state_event_type_t)(uintptr_t)cbdata;


    /* Get the list of lua events. If it is NULL, then there are no
     * registered lua events of this type, so just exit cleanly.
     */
    luaevents = modcfg->event_reg[event];
    if (luaevents == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Get the lua runtime. */
    lua = modlua_runtime_get(conn);
    if (lua == NULL) {
        ib_log_error(ib, 3, "Failed to fetch lua runtime for conn=%p", conn);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Run through the luaevents list, which is a list of loaded
     * lua modules that have an event handler for this event. The
     * corresponding lua event handler (onEventFoo) need to be 
     * executed for each module in the list.
     */
    IB_LIST_LOOP(luaevents, node) {
        ib_module_t *m = (ib_module_t *)ib_list_node_data(node);
        ib_log_debug(ib, 9, "Lua module \"%s\" (%p) has handler for event[%d]=%s",
                     m->name, m, event, ib_state_event_name(event));
        rc = modlua_exec_lua_handler(ib, conndata, lua, m->name, event);
        if (rc != IB_OK) {
            ib_log_error(ib, 3, "Error executing lua handler");
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Generic event handler for Lua connection events.
 *
 * @param ib Engine
 * @param conn Connection
 * @param cbdata Event number passed as pointer value
 *
 * @return Status code
 */
static ib_status_t modlua_handle_lua_conn_event(ib_engine_t *ib,
                                                ib_conn_t *conn,
                                                void *cbdata)
{
    IB_FTRACE_INIT(modlua_handle_lua_conn_event);
    ib_state_event_type_t event;
    modlua_cfg_t *modcfg;
    modlua_runtime_t *lua;
    ib_list_t *luaevents;
    ib_list_node_t *node;
    ib_status_t rc;

    /* Get the module config. */
    rc = ib_context_module_config(conn->ctx,
                                  &IB_MODULE_SYM, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to fetch module %s config: %d",
                     MODULE_NAME_STR, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Verify cbdata is in range for an event. */
    if ((uintptr_t)cbdata >= IB_STATE_EVENT_NUM) {
        ib_log_error(ib, 3, "Lua event was out of range: %" PRIxMAX,
                     (uintptr_t)cbdata);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    event = (ib_state_event_type_t)(uintptr_t)cbdata;


    /* Get the list of lua events. If it is NULL, then there are no
     * registered lua events of this type, so just exit cleanly.
     */
    luaevents = modcfg->event_reg[event];
    if (luaevents == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Get the lua runtime. */
    lua = modlua_runtime_get(conn);
    if (lua == NULL) {
        ib_log_error(ib, 3, "Failed to fetch lua runtime for conn=%p", conn);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Run through the luaevents list, which is a list of loaded
     * lua modules that have an event handler for this event. The
     * corresponding lua event handler (onEventFoo) need to be 
     * executed for each module in the list.
     */
    IB_LIST_LOOP(luaevents, node) {
        ib_module_t *m = (ib_module_t *)ib_list_node_data(node);
        ib_log_debug(ib, 9, "Lua module \"%s\" (%p) has handler for event[%d]=%s",
                     m->name, m, event, ib_state_event_name(event));
        rc = modlua_exec_lua_handler(ib, conn, lua, m->name, event);
        if (rc != IB_OK) {
            ib_log_error(ib, 3, "Error executing lua handler");
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Generic event handler for Lua transaction events.
 *
 * @param ib Engine
 * @param tx Transaction
 * @param cbdata Event number passed as pointer value
 *
 * @return Status code
 */
static ib_status_t modlua_handle_lua_tx_event(ib_engine_t *ib,
                                              ib_tx_t *tx,
                                              void *cbdata)
{
    IB_FTRACE_INIT(modlua_handle_lua_tx_event);
    ib_state_event_type_t event;
    modlua_cfg_t *modcfg;
    modlua_runtime_t *lua;
    ib_list_t *luaevents;
    ib_list_node_t *node;
    ib_status_t rc;

    /* Get the module config. */
    rc = ib_context_module_config(tx->ctx,
                                  &IB_MODULE_SYM, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to fetch module %s config: %d",
                     MODULE_NAME_STR, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Verify cbdata is in range for an event. */
    if ((uintptr_t)cbdata >= IB_STATE_EVENT_NUM) {
        ib_log_error(ib, 3, "Lua event was out of range: %" PRIxMAX,
                     (uintptr_t)cbdata);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    event = (ib_state_event_type_t)(uintptr_t)cbdata;


    /* Get the list of lua events. If it is NULL, then there are no
     * registered lua events of this type, so just exit cleanly.
     */
    luaevents = modcfg->event_reg[event];
    if (luaevents == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Get the lua runtime. */
    lua = modlua_runtime_get(tx->conn);
    if (lua == NULL) {
        ib_log_error(ib, 3, "Failed to fetch lua runtime for tx=%p conn=%p", tx, tx->conn);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Run through the luaevents list, which is a list of loaded
     * lua modules that have an event handler for this event. The
     * corresponding lua event handler (onEventFoo) need to be 
     * executed for each module in the list.
     */
    IB_LIST_LOOP(luaevents, node) {
        ib_module_t *m = (ib_module_t *)ib_list_node_data(node);
        ib_log_debug(ib, 9, "Lua module \"%s\" (%p) has handler for event[%d]=%s",
                     m->name, m, event, ib_state_event_name(event));
        rc = modlua_exec_lua_handler(ib, tx, lua, m->name, event);
        if (rc != IB_OK) {
            ib_log_error(ib, 3, "Error executing lua handler");
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Module Routines -- */

static ib_status_t modlua_init(ib_engine_t *ib)
{
    IB_FTRACE_INIT(modlua_init);

    /* Zero the lua event registration lists. */
    memset(modlua_global_cfg.event_reg, 0, sizeof(modlua_global_cfg.event_reg));

    /* Load lua modules */
    ib_log_debug(ib, 4, "Loading test lua module");
    modlua_module_load(ib, NULL, X_MODULE_BASE_PATH "example.lua");

    /* Hook to initialize the lua runtime with the connection. */
    ib_hook_register(ib, conn_started_event,
                     (ib_void_fn_t)modlua_init_lua_runtime,
                     (void *)conn_started_event);

    /* Hook to destroy the lua runtime with the connection. */
    ib_hook_register(ib, conn_finished_event,
                     (ib_void_fn_t)modlua_destroy_lua_runtime,
                     (void *)conn_finished_event);

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modlua_fini(ib_engine_t *ib)
{
    IB_FTRACE_INIT(modlua_fini);
    IB_FTRACE_RET_STATUS(IB_OK);
}


static ib_status_t modlua_context_init(ib_engine_t *ib,
                                       ib_context_t *ctx)
{
    IB_FTRACE_INIT(modlua_context_init);
    modlua_cfg_t *modcfg;
    ib_status_t rc;

    /* Get the module config. */
    rc = ib_context_module_config(ctx, &IB_MODULE_SYM, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to fetch module %s config: %d",
                     MODULE_NAME_STR, rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* For now, lua modules contexts are configured via main context
     * and then copied into any sub-context.
     */
#if 0
    /* Zero the lua event registration lists. */
    memset(modcfg->event_reg, 0, sizeof(modcfg->event_reg));
#endif

    /* Init the lua modules that were loaded */
    modlua_module_init(ib, ctx, "example");

    /* Register connection data event handlers. */
    ib_hook_register_context(ctx, conn_data_in_event,
                             (ib_void_fn_t)modlua_handle_lua_conndata_event,
                             (void *)conn_data_in_event);
    ib_hook_register_context(ctx, conn_data_out_event,
                             (ib_void_fn_t)modlua_handle_lua_conndata_event,
                             (void *)conn_data_out_event);

    /* Register connection event handlers. */
    ib_hook_register_context(ctx, conn_started_event,
                             (ib_void_fn_t)modlua_handle_lua_conn_event,
                             (void *)conn_started_event);
    ib_hook_register_context(ctx, conn_opened_event,
                             (ib_void_fn_t)modlua_handle_lua_conn_event,
                             (void *)conn_opened_event);
    ib_hook_register_context(ctx, handle_context_conn_event,
                             (ib_void_fn_t)modlua_handle_lua_conn_event,
                             (void *)handle_context_conn_event);
    ib_hook_register_context(ctx, handle_connect_event,
                             (ib_void_fn_t)modlua_handle_lua_conn_event,
                             (void *)handle_connect_event);
    ib_hook_register_context(ctx, conn_closed_event,
                             (ib_void_fn_t)modlua_handle_lua_conn_event,
                             (void *)conn_closed_event);
    ib_hook_register_context(ctx, handle_disconnect_event,
                             (ib_void_fn_t)modlua_handle_lua_conn_event,
                             (void *)handle_disconnect_event);
    ib_hook_register_context(ctx, conn_finished_event,
                             (ib_void_fn_t)modlua_handle_lua_conn_event,
                             (void *)conn_finished_event);

    /* Register transaction event handlers. */
    ib_hook_register_context(ctx, tx_started_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)tx_started_event);
    ib_hook_register_context(ctx, request_started_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)request_started_event);
    ib_hook_register_context(ctx, request_headers_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)request_headers_event);
    ib_hook_register_context(ctx, handle_context_tx_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)handle_context_tx_event);
    ib_hook_register_context(ctx, handle_request_headers_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)handle_request_headers_event);
    ib_hook_register_context(ctx, request_body_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)request_body_event);
    ib_hook_register_context(ctx, handle_request_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)handle_request_event);
    ib_hook_register_context(ctx, request_finished_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)request_finished_event);
    ib_hook_register_context(ctx, tx_process_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)tx_process_event);
    ib_hook_register_context(ctx, response_started_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)response_started_event);
    ib_hook_register_context(ctx, response_headers_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)response_headers_event);
    ib_hook_register_context(ctx, handle_response_headers_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)handle_response_headers_event);
    ib_hook_register_context(ctx, response_body_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)response_body_event);
    ib_hook_register_context(ctx, handle_response_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)handle_response_event);
    ib_hook_register_context(ctx, response_finished_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)response_finished_event);
    ib_hook_register_context(ctx, log_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)log_event);
    ib_hook_register_context(ctx, handle_postprocess_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)handle_postprocess_event);
    ib_hook_register_context(ctx, tx_finished_event,
                             (ib_void_fn_t)modlua_handle_lua_tx_event,
                             (void *)tx_finished_event);

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Module Configuration -- */

static IB_CFGMAP_INIT_STRUCTURE(modlua_config_map) = {
    /* NOTE: event_reg is used internally only and not mapable. */

    IB_CFGMAP_INIT_LAST
};


/* -- Configuration Directives -- */

static ib_status_t modlua_dir_param1(ib_cfgparser_t *cp,
                                     const char *name,
                                     const char *p1,
                                     void *cbdata)
{
    IB_FTRACE_INIT(core_dir_param1);
    ib_engine_t *ib = cp->ib;
    //ib_status_t rc;

    if (strcasecmp("LoadModuleLua", name) == 0) {
        ib_log_debug(ib, 4, "TODO: Handle Directive: %s %s", name, p1);
    }
    else {
        ib_log_error(ib, 1, "Unhandled directive: %s %s", name, p1);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static IB_DIRMAP_INIT_STRUCTURE(modlua_directive_map) = {
    IB_DIRMAP_INIT_PARAM1(
        "LoadModuleLua",
        modlua_dir_param1,
        NULL,
        NULL
    ),

    /* End */
    IB_DIRMAP_INIT_LAST
};


/* -- Module Definition -- */

/**
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /**< Default metadata */
    MODULE_NAME_STR,                     /**< Module name */
    IB_MODULE_CONFIG(&modlua_global_cfg),/**< Global config data */
    modlua_config_map,                   /**< Configuration field map */
    modlua_directive_map,                /**< Config directive map */
    modlua_init,                         /**< Initialize function */
    modlua_fini,                         /**< Finish function */
    modlua_context_init,                 /**< Context init function */
);

