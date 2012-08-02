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
 * @brief IronBee &mdash; LUA Module
 *
 * This module integrates with luajit, allowing lua modules to be loaded.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "lua/ironbee.h"

#include <ironbee/array.h>
#include <ironbee/cfgmap.h>
#include <ironbee/core.h>
#include <ironbee/debug.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/provider.h>
#include <ironbee/util.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 *  * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* -- Module Setup -- */

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        lua
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

#define MODLUA_CONN_KEY "lua-runtime"


/* Define the public module symbol. */
IB_MODULE_DECLARE();

typedef struct modlua_chunk_t modlua_chunk_t;
typedef struct modlua_chunk_tracker_t modlua_chunk_tracker_t;
typedef struct modlua_cpart_t modlua_cpart_t;
typedef struct modlua_reg_t modlua_reg_t;
typedef struct modlua_runtime_t modlua_runtime_t;
typedef struct modlua_cfg_t modlua_cfg_t;
typedef struct modlua_wrapper_cbdata_t modlua_wrapper_cbdata_t;

/**
 * @brief A container to store Lua Module byte code dumped by lua_dump.
 */
struct modlua_chunk_t {
    ib_engine_t        *ib;           /**< Engine */
    ib_mpool_t         *mp;           /**< Pool to allocate from */
    const char         *name;         /**< Name for debug */

    /**
     * Array of chunk parts. These are segments of memory allocated
     * from mp which represent a Lua function in byte code.
     */
    ib_array_t         *cparts;
};

/**
 * Structure to track chunk parts while reading. Used by modlua_load_lua_data.
 */
struct modlua_chunk_tracker_t {
    modlua_chunk_t     *chunk;        /**< The chunk that is being read */
    size_t              part;         /**< The current part index */

};

/**
 * Lua Module byte code. This is stored in the member cparts (an ib_list_t)
 * of modlua_chunk_t.
 */
struct modlua_cpart_t {
    uint8_t            *data;         /**< Data */
    size_t              dlen;         /**< Data length */
};

/**
 * @brief Lua runtime
 * @details Created for each connection and stored at MODLUA_CONN_KEY.
 */
struct modlua_runtime_t {
    lua_State          *L;            /**< Lua stack */
};

/** Module Configuration Structure */
struct modlua_cfg_t {
    char               *pkg_path;
    char               *pkg_cpath;
    ib_list_t          *lua_modules;
    ib_list_t          *event_reg[IB_STATE_EVENT_NUM + 1];
    lua_State          *Lconfig;
};

/** Lua Wrapper Callback Data Structure */
struct modlua_wrapper_cbdata_t {
    const char         *fn_config_modname;
    const char         *fn_config_name;
    const char         *fn_blkend_modname;
    const char         *fn_blkend_name;
    const char         *cbdata_type;
    void               *cbdata;
};

/** Config Directive Wrapper Fetch Functions */
ib_void_fn_t modlua_config_wrapper(void);
ib_config_cb_blkend_fn_t modlua_blkend_wrapper(void);

/* Instantiate a module global configuration. */
static modlua_cfg_t modlua_global_cfg = {
    NULL, /* pkg_path */
    NULL  /* pkg_cpath */
};

/* -- Lua Routines -- */

/**
 * @brief Reader callback to lua_load.
 * @details This is used to convert modula_chunk_t* into a function on the
 *          top of the Lua stack.
 * @param[in] L Lua state @a udata will be loaded into.
 * @param[in,out] udata Always a modlua_chunk_tracker_t*.
 *                This stores a pointer to a modlua_chunk_t which contains
 *                an ib_list_t of chunks of data. The modlua_chunk_tracker_t
 *                also contains the current index in that list of chunks
 *                that is next to be delivered to the Lua runtime.
 * @param[in] size The size of the char* returned by this function.
 * @returns The memory pointed to by ((modlua_chunk_tracker_t *)udata)->part
 *          with a size of @a size. This represents a chunk of Lua bytecode
 *          that, when assembled with all the other chunks, will be a Lua
 *          function placed on top of @a L.
 *          NULL is returned on an error or the end of the list of chunks.
 */
static const char *modlua_data_reader(lua_State *L, void *udata, size_t *size)
{
    IB_FTRACE_INIT();

    /* Holds the modlua_chunk_t and the index into its list of chunks. */
    modlua_chunk_tracker_t *tracker = (modlua_chunk_tracker_t *)udata;

    /* A container for the ib_list_t of the chunks as well as other
     * relevant pointers, e.g., the IronBee engine. */
    modlua_chunk_t *chunk = tracker->chunk;

    /* The list of data chunks that represent a Lua function in byte code.
     * We must return one of these list elements. */
    ib_engine_t *ib_engine = chunk->ib;

    /* We will set this to the list element we are going to return. */
    modlua_cpart_t *cpart;

    /* Return code. */
    ib_status_t rc;

    /* Get the Lua byte code from the list. */
    rc = ib_array_get(chunk->cparts, tracker->part, &cpart);
    if (rc != IB_OK) {
        ib_log_error(ib_engine,  "No more chunk parts to read: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_CONSTSTR(NULL);
    }

    /* Return to the caller (Lua) the size of the byte code pointer we
     * are about to return to it via cpart->data. */
    *size = cpart->dlen;

    /* Increment the iteration through our list of chunks. */
    ++tracker->part;

    /* Return the chunk of Lua byte code. */
    IB_FTRACE_RET_CONSTSTR((const char *)cpart->data);
}

/**
 * @brief Callback for lua_dump to store compiled bytecode.
 * @param[in] L Lua state.
 * @param[in] data Byte code to be copied.
 * @param[in] size Length of @a data.
 * @param[out] udata User memory that will store the byte code. This is
 *             always a modlua_chunk_t*.
 * @returns 0 on success and -1 on failure.
 */
static int modlua_data_writer(lua_State *L,
                              const void *data,
                              size_t size,
                              void *udata)
{
    IB_FTRACE_INIT();
    modlua_chunk_t *chunk = (modlua_chunk_t *)udata;
    //ib_engine_t *ib = chunk->ib;
    modlua_cpart_t *cpart;
    ib_status_t rc;

    //ib_log_debug3(ib, "Lua writing part size=%" PRIuMAX, size);

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

static int modlua_load_lua_data(ib_engine_t *ib_engine,
                                lua_State *L,
                                modlua_chunk_t *chunk)
{
    IB_FTRACE_INIT();
    modlua_chunk_tracker_t tracker;
    int ec;

    tracker.chunk = chunk;
    tracker.part = 0;
    ec = lua_load(L, modlua_data_reader, &tracker, chunk->name);

    IB_FTRACE_RET_INT(ec);
}

#define IB_FFI_MODULE  ironbee-ffi
#define IB_FFI_MODULE_STR IB_XSTRINGIFY(IB_FFI_MODULE)

#define IB_FFI_MODULE_WRAPPER     _IRONBEE_CALL_MODULE_HANDLER
#define IB_FFI_MODULE_WRAPPER_STR IB_XSTRINGIFY(IB_FFI_MODULE_WRAPPER)

#define IB_FFI_MODULE_CFG_WRAPPER     _IRONBEE_CALL_CONFIG_HANDLER
#define IB_FFI_MODULE_CFG_WRAPPER_STR IB_XSTRINGIFY(IB_FFI_MODULE_CFG_WRAPPER)

#define IB_FFI_MODULE_EVENT_WRAPPER     _IRONBEE_CALL_EVENT_HANDLER
#define IB_FFI_MODULE_EVENT_WRAPPER_STR IB_XSTRINGIFY(IB_FFI_MODULE_EVENT_WRAPPER)

static ib_status_t modlua_register_event_handler(ib_engine_t *ib_engine,
                                                 ib_context_t *ctx,
                                                 const char *event_name,
                                                 ib_module_t *module)
{
    IB_FTRACE_INIT();
    ib_mpool_t *pool = ib_engine_pool_config_get(ib_engine);
    modlua_cfg_t *modcfg;
    ib_state_event_type_t event;
    ib_status_t rc;

    /* Get the module config. */
    rc = ib_context_module_config(ctx, IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert(ib_engine,  "Failed to fetch module %s config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    if (strcmp("ConnStarted", event_name) == 0) {
        event = conn_started_event;
    }
    else if (strcmp("ConnOpened", event_name) == 0) {
        event = conn_opened_event;
    }
    else if (strcmp("HandleContextConn", event_name) == 0) {
        event = handle_context_conn_event;
    }
    else if (strcmp("HandleConnect", event_name) == 0) {
        event = handle_connect_event;
    }
    else if (strcmp("TxStarted", event_name) == 0) {
        event = tx_started_event;
    }
    else if (strcmp("RequestStarted", event_name) == 0) {
        event = request_started_event;
    }
    else if (strcmp("RequestHeaderFinished", event_name) == 0) {
        event = request_header_finished_event;
    }
    else if (strcmp("RequestHeaderData", event_name) == 0) {
        event = request_header_data_event;
    }
    else if (strcmp("HandleRequestHeader", event_name) == 0) {
        event = handle_request_header_event;
    }
    else if (strcmp("HandleContextTx", event_name) == 0) {
        event = handle_context_tx_event;
    }
    else if (strcmp("RequestBody", event_name) == 0) {
        event = request_body_data_event;
    }
    else if (strcmp("HandleRequest", event_name) == 0) {
        event = handle_request_event;
    }
    else if (strcmp("RequestFinished", event_name) == 0) {
        event = request_finished_event;
    }
    else if (strcmp("TxProcess", event_name) == 0) {
        event = tx_process_event;
    }
    else if (strcmp("ResponseStarted", event_name) == 0) {
        event = response_started_event;
    }
    else if (strcmp("ResponseHeaderFinished", event_name) == 0) {
        event = response_header_finished_event;
    }
    else if (strcmp("ResponseHeaderData", event_name) == 0) {
        event = response_header_data_event;
    }
    else if (strcmp("HandleResponseHeader", event_name) == 0) {
        event = handle_response_header_event;
    }
    else if (strcmp("ResponseBody", event_name) == 0) {
        event = response_body_data_event;
    }
    else if (strcmp("HandleResponse", event_name) == 0) {
        event = handle_response_event;
    }
    else if (strcmp("ResponseFinished", event_name) == 0) {
        event = response_finished_event;
    }
    else if (strcmp("HandlePostprocess", event_name) == 0) {
        event = handle_postprocess_event;
    }
    else if (strcmp("TxFinished", event_name) == 0) {
        event = tx_finished_event;
    }
    else if (strcmp("ConnClosed", event_name) == 0) {
        event = conn_closed_event;
    }
    else if (strcmp("HandleDisconnect", event_name) == 0) {
        event = handle_disconnect_event;
    }
    else if (strcmp("ConnFinished", event_name) == 0) {
        event = conn_finished_event;
    }
    else if (strcmp("ConnDataIn", event_name) == 0) {
        event = conn_data_in_event;
    }
    else if (strcmp("ConnDataOut", event_name) == 0) {
        event = conn_data_out_event;
    }
    else {
        ib_log_error(ib_engine,  "Unhandled event %s", event_name);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_log_debug3(ib_engine,
                 "Registering lua event handler m=%p event=%d: onEvent%s",
                 module,
                 event,
                 event_name);

    /* Create an event list if required. */
    if (modcfg->event_reg[event] == NULL) {
        rc = ib_list_create(modcfg->event_reg + event, pool);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }
    }

    /* Add the lua module to the lua event list. */
    ib_log_debug3(ib_engine, "Adding module=%p to event=%d list=%p",
                 module, event, modcfg->event_reg[event]);
    rc = ib_list_push(modcfg->event_reg[event], (void *)module);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @brief Load the given file into the given Lua state.
 * @param[in] ib_engine IronBee engine for logging.
 * @param[in,out] L Lua state the file will be loaded into.
 * @param[in] file The Lua script to be loaded.
 * @param[out] pchunk If @a file is successfully loaded, it is dumped
 *             by lua_dump into *pchunk. The memory in *pchunk is
 *             allocated from the @a ib_engine memory pool and does not
 *             need to be explicitly freed by the caller.
 * @returns IB_OK or appropriate status.
 */
static ib_status_t modlua_load_lua_file(ib_engine_t *ib_engine,
                                        lua_State *L,
                                        const char *file,
                                        modlua_chunk_t **pchunk)
{
    IB_FTRACE_INIT();
    ib_mpool_t *pool = ib_engine_pool_config_get(ib_engine);
    modlua_chunk_t *chunk;
    char *name;
    char *name_start;
    char *name_end;
    int name_len;
    ib_status_t ib_rc;
    int sys_rc;

    /* Figure out the name based on the file. */
    /// @todo Need a better way???
    name_start = rindex(file, '/');
    if (name_start == NULL) {
        name_start = (char *)file;
    }
    else {
        ++name_start;
    }
    name_end = index(name_start, '.');
    if (name_end == NULL) {
        name_end = index(name_start, 0);
    }
    name_len = (name_end-name_start);
    name = ib_mpool_alloc(pool, name_len + 1);
    if (name == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    memcpy(name, name_start, name_len);
    name[name_len] = '\0';

    ib_log_debug2(ib_engine, "Loading lua module \"%s\": %s",
                 name, file);

    /* Save the Lua chunk. */
    ib_log_debug3(ib_engine, "Allocating chunk");
    chunk = (modlua_chunk_t *)ib_mpool_alloc(pool, sizeof(*chunk));
    if (chunk == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    chunk->ib = ib_engine;
    chunk->mp = ib_engine_pool_config_get(ib_engine);
    chunk->name = name;
    *pchunk = chunk;

    ib_log_debug3(ib_engine, "Creating array for chunk parts");
    /// @todo Make this initial size configurable
    ib_rc = ib_array_create(&chunk->cparts, pool, 32, 32);
    if (ib_rc != IB_OK) {
        IB_FTRACE_RET_STATUS(ib_rc);
    }

    ib_log_debug2(ib_engine, "Using precompilation via lua_dump.");

    /* Load (compile) the lua module. */
    sys_rc = luaL_loadfile(L, file);
    if (sys_rc != 0) {
        ib_log_error(ib_engine,  "Failed to load lua module \"%s\" - %s (%d)",
                     file, lua_tostring(L, -1), sys_rc);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Dump the byte code into lua chunk. */
    sys_rc = lua_dump(L, modlua_data_writer, chunk);
    if (sys_rc != 0) {
        ib_log_error(ib_engine,  "Failed to save lua module \"%s\" - %s (%d)",
                     file, lua_tostring(L, -1), sys_rc);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    lua_pushstring(L, name);
    sys_rc = lua_pcall(L, 1, 0, 0);
    if (sys_rc != 0) {
        ib_log_error(ib_engine,  "Failed to run lua module \"%s\" - %s (%d)",
                     file, lua_tostring(L, -1), sys_rc);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modlua_init_lua_wrapper(ib_engine_t *ib,
                                           ib_module_t *module,
                                           void        *cbdata)
{
    IB_FTRACE_INIT();
    modlua_cfg_t *maincfg;
    lua_State *L;
    const char *funcname = "onModuleLoad";
    ib_status_t rc;

    /* Get the main module config. */
    rc = ib_context_module_config(ib_context_main(ib),
                                  IB_MODULE_STRUCT_PTR, (void *)&maincfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch module %s main config: %s",
                     module->name, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Use the configuration lua state. */
    L = maincfg->Lconfig;

    lua_getglobal(L, IB_FFI_MODULE_WRAPPER_STR);
    if (lua_isfunction(L, -1)) {
        int ec;

        ib_log_debug3(ib,
                     "Executing lua module handler \"%s.%s\" via wrapper",
                     module->name, funcname);
        lua_pushlightuserdata(L, ib);
        lua_pushstring(L, module->name);
        lua_pushstring(L, funcname);
        /// @todo Use errfunc w/debug.traceback()
        ec = lua_pcall(L, 3, 1, 0);
        if (ec != 0) {
            ib_log_error(ib,
                         "Failed to exec lua wrapper for \"%s.%s\": "
                         "%s (%d)",
                         module->name, funcname,
                         lua_tostring(L, -1), ec);
            rc = IB_EINVAL;
        }
        else if (lua_isnumber(L, -1)) {
            lua_Integer li = lua_tointeger(L, -1);
            rc = (ib_status_t)(int)li;
        }
        else {
            ib_log_error(ib,
                         "Expected %s returned from lua "
                         "\"%s.%s\", but received %s",
                         lua_typename(L, LUA_TNUMBER),
                         module->name, funcname,
                         lua_typename(L, lua_type(L, -1)));
            rc = IB_EINVAL;
        }
    }
    else {
        ib_log_error(ib,
                     "Lua module wrapper function not available - "
                     "could not execute \"%s.%s\" handler",
                     module->name, funcname);
        rc = IB_EUNKNOWN;
    }
    lua_pop(L, 1); /* cleanup stack */

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Load Lua file into IronBee main configuration.
 * @param[in] ib IronBee engine providing access to Lua state.
 * @param[in] file The file to load.
 * @returns IB_OK on success or other values on failure.
 */
static ib_status_t modlua_module_load(ib_engine_t *ib,
                                      const char *file)
{
    IB_FTRACE_INIT();
    modlua_chunk_t *chunk;
    modlua_cfg_t *maincfg;
    ib_list_t *mlist = (ib_list_t *)IB_MODULE_STRUCT.data;
    lua_State *L;
    ib_module_t *module;
    ib_status_t rc;
    int use_onload = 0;

    /* Get the main module config. */
    rc = ib_context_module_config(ib_context_main(ib),
                                  IB_MODULE_STRUCT_PTR, (void *)&maincfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch module %s main config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Uses the configuration lua state. */
    L = maincfg->Lconfig;
    if (L == NULL) {
        ib_log_error(ib,
                     "Cannot load lua module \"%s\": "
                     "Lua support not available.",
                     file);
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Load the lua file. */
    rc = modlua_load_lua_file(ib, L, file, &chunk);
    if (rc != IB_OK) {
        ib_log_error(ib, "Ignoring failed lua module \"%s\": %s", file, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Check if there is an onModuleLoad module. */
    ib_log_debug3(ib, "Analyzing lua module \"%s\"", chunk->name);
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_getfield(L, -1, chunk->name);
    lua_getfield(L, -1, "onModuleLoad");
    if (lua_isfunction(L, -1)) {
        use_onload = 1;
    }
    lua_pop(L, 4); /* Cleanup the stack */

    /* Create the Lua module as if it was a normal module. */
    ib_log_debug3(ib, "Creating lua module structure");
    rc = ib_module_create(&module, ib);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Initialize the loaded module. */
    ib_log_debug3(ib, "Init lua module structure");
    IB_MODULE_INIT_DYNAMIC(
        module,                         /* Module */
        file,                           /* Module code filename */
        chunk,                          /* Module data */
        ib,                             /* Engine */
        chunk->name,                    /* Module name */
        NULL,                           /* Global config data */
        0,                              /* Global config data length */
        NULL,                           /* Config copier */
        NULL,                           /* Config copier data */
        NULL,                           /* Configuration field map */
        NULL,                           /* Config directive map */
        (use_onload ? modlua_init_lua_wrapper : NULL), /* Initialize function */
        NULL,                           /* Callback data */
        NULL,                           /* Finish function */
        NULL,                           /* Callback data */
        NULL,                           /* Context open function */
        NULL,                           /* Callback data */
        NULL,                           /* Context close function */
        NULL,                           /* Callback data */
        NULL,                           /* Context destroy function */
        NULL                            /* Callback data */
    );

    /* Track loaded lua modules. */
    rc = ib_list_push(mlist, module);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Initialize and register the new lua module with the engine. */
    ib_log_debug3(ib, "Init lua module");
    rc = ib_module_init(module, ib);

    IB_FTRACE_RET_STATUS(rc);
}

/// @todo This should be triggered by directive.
static ib_status_t modlua_lua_module_init(ib_engine_t *ib,
                                          ib_context_t *ctx,
                                          const char *name)
{
    IB_FTRACE_INIT();
    ib_mpool_t *pool = ib_engine_pool_config_get(ib); /// @todo config pool???
    lua_State *L;
    ib_module_t *module;
    modlua_cfg_t *maincfg;
    modlua_cfg_t *modcfg;
    modlua_chunk_t *chunk;
    ib_status_t rc;
    int ec;

    /* Skip loading the built-in module. */
    /// @todo Do not hard-code this
    if (strcasecmp("ironbee-ffi", name) == 0) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Get the main module config. */
    rc = ib_context_module_config(ib_context_main(ib),
                                  IB_MODULE_STRUCT_PTR, (void *)&maincfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch module %s main config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Get the module config. */
    rc = ib_context_module_config(ctx, IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch module %s config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug2(ib, "Init lua module ctx=%p maincfg=%p Lconfig=%p: %s", ctx, maincfg, maincfg->Lconfig, name);

    /* Use the config lua state. */
    L = maincfg->Lconfig;

    /* Lookup the module. */
    rc = ib_engine_module_get(ib, name, &module);
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
    ib_list_push(modcfg->lua_modules, module);

    /* Get the lua chunk for this module. */
    chunk = (modlua_chunk_t *)module->data;

    /* Load the module lua code. */
    ec = modlua_load_lua_data(ib, L, chunk);
    if (ec != 0) {
        ib_log_error(ib, "Failed to init lua module \"%s\" - %s (%d)",
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
    lua_pushstring(L, module->name);
    ec = lua_pcall(L, 1, 0, 0);
    if (ec != 0) {
        ib_log_error(ib, "Failed to execute lua module \"%s\" - %s (%d)",
                     name, lua_tostring(L, -1), ec);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Initialize the loaded module. */
    ib_log_debug2(ib, "Initializing lua module \"%s\"", module->name);
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_getfield(L, -1, module->name);
    ib_log_debug3(ib, "Module load returned type=%s",
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
                    ib_log_debug3(ib, "Lua module \"%s\" %s=\"%s\"",
                                 module->name, key, val);
                }
                else if (lua_isfunction(L, -1)) {
                    val = lua_topointer(L, -1);

                    /* If it is an onEvent function, then register the
                     * function as a lua event handler.
                     */
                    if (strncmp("onEvent", key, 7) == 0) {
                        ib_log_debug3(ib,
                                     "Lua module \"%s\" registering event "
                                     "handler: %s",
                                     module->name, key);

                        /* key + 7 it the event name following "onEvent" */
                        rc = modlua_register_event_handler(ib,
                                                           ctx,
                                                           key + 7,
                                                           module);
                        if (rc != IB_OK) {
                            ib_log_error(ib,
                                         "Failed to register "
                                         "lua event handler \"%s\": %s",
                                         key, ib_status_to_string(rc));
                            IB_FTRACE_RET_STATUS(rc);
                        }
                    }
                    else {
                        ib_log_debug3(ib, "KEY:%s; VAL:%p", key, val);
                    }
                }
            }
            lua_pop(L, 1);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


static ib_status_t modlua_load_ironbee_module(ib_engine_t *ib,
                                              modlua_cfg_t *modcfg,
                                              lua_State *L)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;
    modlua_chunk_t *chunk;
    ib_module_t *m;
    ib_core_cfg_t *corecfg = NULL;
    const size_t pathmax = 512;
    char *path = NULL;

    rc = ib_engine_module_get(ib, IB_FFI_MODULE_STR, &m);
    if (rc == IB_OK) {
        int ec;

        chunk = (modlua_chunk_t *)m->data;
        ib_log_debug3(ib, "Lua %p module \"%s\" module=%p chunk=%p",
                     L, IB_FFI_MODULE_STR, m, chunk);
        ec = modlua_load_lua_data(ib, L, chunk);
        if (ec != 0) {
            ib_log_error(ib, "Failed to init lua module \"%s\" - %s (%d)",
                         IB_FFI_MODULE_STR, lua_tostring(L, -1), ec);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        lua_pushstring(L, m->name);
        ec = lua_pcall(L, 1, 0, 0);
        if (ec != 0) {
            ib_log_error(ib, "Failed to execute lua module \"%s\" - %s (%d)",
                         IB_FFI_MODULE_STR, lua_tostring(L, -1), ec);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }
    else if (rc == IB_ENOENT) {
        rc = ib_context_module_config(ib_context_main(ib),
                                      ib_core_module(),
                                      (void *)&corecfg);

        if (rc != IB_OK) {
            ib_log_error(ib, "Failed to retrieve core configuration.");
            IB_FTRACE_RET_STATUS(rc);
        }

        path = malloc(pathmax);

        if (path==NULL) {
            ib_log_error(ib, "Cannot allocate memory for module path.");
            IB_FTRACE_RET_STATUS(IB_EALLOC);
        }

        size_t len = snprintf(path, pathmax, "%s/%s",
                              corecfg->module_base_path,
                              "ironbee-ffi.lua");

        if (len >= pathmax) {
            ib_log_error(ib,
                         "Filename too long: %s/%s",
                         corecfg->module_base_path,
                         "ironbee-ffi.lua");
            free(path);
            path = NULL;
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }

        /* Load the ironbee FFI module. */
        rc = modlua_module_load(ib, path);
        free(path);
        path = NULL;

        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }
    else {
        IB_FTRACE_RET_STATUS(rc);
    }


    /* Get the common global "package" field. */
    lua_getglobal(L, "package");

    /* Cache wrapper functions to global for faster lookup. */
    lua_getfield(L, -1, "preload");
    lua_getfield(L, -1, IB_FFI_MODULE_STR);
    lua_getfield(L, -1, IB_FFI_MODULE_WRAPPER_STR);
    lua_setglobal(L, IB_FFI_MODULE_WRAPPER_STR);
    lua_getfield(L, -1, IB_FFI_MODULE_CFG_WRAPPER_STR);
    lua_setglobal(L, IB_FFI_MODULE_CFG_WRAPPER_STR);
    lua_getfield(L, -1, IB_FFI_MODULE_EVENT_WRAPPER_STR);
    lua_setglobal(L, IB_FFI_MODULE_EVENT_WRAPPER_STR);
    lua_pop(L, 2); /* Cleanup stack. */

    /* Set package paths if configured. */
    if (modcfg->pkg_path != NULL) {
        ib_log_debug(ib, "Using lua package.path=\"%s\"",
                     modcfg->pkg_path);
        lua_getfield(L, -1, "path");
        lua_pushstring(L, modcfg->pkg_path);
        lua_setglobal(L, "path");
    }
    if (modcfg->pkg_cpath != NULL) {
        ib_log_debug(ib, "Using lua package.cpath=\"%s\"",
                     modcfg->pkg_cpath);
        lua_getfield(L, -1, "cpath");
        lua_pushstring(L, modcfg->pkg_cpath);
        lua_setglobal(L, "cpath");
    }

    /* Cleanup common "package" field off the stack. */
    lua_pop(L, 1);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Get the lua runtime from the connection.
 *
 * @param conn Connection
 *
 * @returns Lua runtime
 */
static modlua_runtime_t *modlua_runtime_get(ib_conn_t *conn)
{
    IB_FTRACE_INIT();
    modlua_runtime_t *lua;

    ib_hash_get(conn->data, &lua, MODLUA_CONN_KEY);

    IB_FTRACE_RET_PTR(modlua_runtime_t, lua);
}



/* -- Event Handlers -- */

/**
 * Initialize the lua runtime for the configuration.
 *
 * @param ib Engine
 * @param param Unused
 * @param cbdata Unused
 *
 * @return Status code
 */
static ib_status_t modlua_init_lua_runtime_cfg(ib_engine_t *ib,
                                               void *param,
                                               void *cbdata)
{
    IB_FTRACE_INIT();
    modlua_cfg_t *modcfg;
    ib_status_t rc;

    /* Get the module config. */
    rc = ib_context_module_config(ib_context_main(ib),
                                  IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch module %s config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Setup a fresh Lua state for this configuration. */
    if (modcfg->Lconfig == NULL) {
        ib_log_debug2(ib, "Initializing lua runtime for configuration.");
        modcfg->Lconfig = luaL_newstate();
        if (modcfg->Lconfig == NULL) {
            ib_log_error(ib, "Failed to initialize lua module.");
            IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
        }

        luaL_openlibs(modcfg->Lconfig);

        /* Preload ironbee module (static link). */
        modlua_load_ironbee_module(ib, modcfg, modcfg->Lconfig);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Destroy the lua runtime for the configuration.
 *
 * @param ib Engine.
 * @param event Event type.
 * @param cbdata Unused.
 *
 * @return Status code.
 */
static ib_status_t modlua_destroy_lua_runtime_cfg(ib_engine_t *ib,
                                                  ib_state_event_type_t event,
                                                  void *cbdata)
{
    IB_FTRACE_INIT();
    modlua_cfg_t *modcfg;
    ib_status_t rc;

    /* Get the module config. */
    rc = ib_context_module_config(ib_context_main(ib),
                                  IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch module %s config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug2(ib, "Destroying lua runtime for configuration.");
    lua_close(modcfg->Lconfig);
    modcfg->Lconfig = NULL;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Initialize the lua runtime for this connection.
 *
 * @param ib Engine.
 * @param event Event type.
 * @param conn Connection.
 * @param cbdata Unused.
 *
 * @return Status code.
 */
static ib_status_t modlua_init_lua_runtime(ib_engine_t *ib,
                                           ib_state_event_type_t event,
                                           ib_conn_t *conn,
                                           void *cbdata)
{
    IB_FTRACE_INIT();

    assert(event == conn_started_event);

    lua_State *L;
    modlua_cfg_t *modcfg;
    modlua_runtime_t *lua;
    ib_list_node_t *node;
    ib_status_t rc;

    /* Get the module config. */
    rc = ib_context_module_config(conn->ctx, IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch module %s config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Setup a fresh Lua state for this connection. */
    ib_log_debug3(ib, "Initializing lua runtime for conn=%p", conn);
    L = luaL_newstate();
    luaL_openlibs(L);

    /* Create the lua runtime and store it with the connection. */
    ib_log_debug3(ib, "Creating lua runtime for conn=%p", conn);
    lua = ib_mpool_alloc(conn->mp, sizeof(*lua));
    if (lua == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }
    lua->L = L;
    rc = ib_hash_set(conn->data, MODLUA_CONN_KEY, lua);
    ib_log_debug2(ib, "Setting lua runtime for conn=%p lua=%p L=%p", conn, lua, L);
    if (rc != IB_OK) {
        ib_log_debug(ib, "Failed to set lua runtime: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Preload ironbee module for other modules to use. */
    modlua_load_ironbee_module(ib, modcfg, L);

    /* Run through each lua module to be used in this context and
     * load it into the lua runtime.
     */
    if (modcfg->lua_modules != NULL) {
        IB_LIST_LOOP(modcfg->lua_modules, node) {
            ib_module_t *m = (ib_module_t *)ib_list_node_data(node);
            modlua_chunk_t *chunk = (modlua_chunk_t *)m->data;
            int ec;

            ib_log_debug2(ib, "Loading lua module \"%s\" into runtime for conn=%p", m->name, conn);
            rc = modlua_load_lua_data(ib, L, chunk);
            if (rc != IB_OK) {
                IB_FTRACE_RET_STATUS(rc);
            }

            ib_log_debug3(ib, "Executing lua chunk=%p", chunk);
            lua_pushstring(L, m->name);
            ec = lua_pcall(L, 1, 0, 0);
            if (ec != 0) {
                ib_log_error(ib, "Failed to execute lua module \"%s\" - %s (%d)",
                             m->name, lua_tostring(L, -1), ec);
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Destroy the lua runtime for this connection.
 *
 * @param ib Engine.
 * @param event Event type.
 * @param conn Connection.
 * @param cbdata Unused.
 *
 * @return Status code.
 */
static ib_status_t modlua_destroy_lua_runtime(ib_engine_t *ib,
                                              ib_state_event_type_t event,
                                              ib_conn_t *conn,
                                              void *cbdata)
{
    IB_FTRACE_INIT();

    assert(event == conn_finished_event);

    modlua_runtime_t *lua;
    ib_status_t rc;

    ib_log_debug3(ib, "Destroying lua runtime for conn=%p", conn);

    lua = modlua_runtime_get(conn);
    if (lua != NULL) {
        lua_close(lua->L);
    }

    rc = ib_hash_remove(conn->data, NULL, MODLUA_CONN_KEY);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t modlua_exec_lua_handler(ib_engine_t *ib,
                                           void *arg,
                                           modlua_runtime_t *lua,
                                           const char *modname,
                                           ib_state_event_type_t event)
{
    IB_FTRACE_INIT();
    lua_State *L = lua->L;
    const char *funcname = NULL;
    ib_status_t rc = IB_OK;

    /* Order here is by most common use. */
    /// @todo No longer needed.
    switch(event) {
        /* Normal Event Handlers */
        case handle_request_header_event:
            funcname = "onEventHandleRequestHeader";
            break;
        case handle_request_event:
            funcname = "onEventHandleRequest";
            break;
        case handle_response_header_event:
            funcname = "onEventHandleResponseHeader";
            break;
        case handle_response_event:
            funcname = "onEventHandleResponse";
            break;
        case handle_postprocess_event:
            funcname = "onEventHandlePostprocess";
            break;
        case handle_connect_event:
            funcname = "onEventHandleConnect";
            break;
        case handle_disconnect_event:
            funcname = "onEventHandleDisconnect";
            break;
        case handle_context_conn_event:
            funcname = "onEventHandleContextConn";
            break;
        case handle_context_tx_event:
            funcname = "onEventHandleContextTx";
            break;

        /* Request Handlers */
        case request_header_finished_event:
            funcname = "onEventRequestHeader";
            break;
        case request_header_data_event:
            funcname = "onEventRequestHeaderData";
            break;
        case request_body_data_event:
            funcname = "onEventRequestBody";
            break;
        case request_started_event:
            funcname = "onEventRequestStarted";
            break;
        case request_finished_event:
            funcname = "onEventRequestFinished";
            break;

        /* Response Handlers */
        case response_started_event:
            funcname = "onEventResponseStarted";
            break;
        case response_header_data_event:
            funcname = "onEventResponseDataHeader";
            break;
        case response_header_finished_event:
            funcname = "onEventResponseHeader";
            break;
        case response_body_data_event:
            funcname = "onEventResponseBody";
            break;
        case response_finished_event:
            funcname = "onEventResponseFinished";
            break;

        /* Transaction Handlers */
        case tx_started_event:
            funcname = "onEventTxStarted";
            break;
        case tx_process_event:
            funcname = "onEventTxProcess";
            break;
        case tx_finished_event:
            funcname = "onEventTxFinished";
            break;

        /* Connection Handlers */
        case conn_data_in_event:
            funcname = "onEventConnDataIn";
            break;
        case conn_data_out_event:
            funcname = "onEventConnDataOut";
            break;
        case conn_started_event:
            funcname = "onEventConnStarted";
            break;
        case conn_opened_event:
            funcname = "onEventConnOpened";
            break;
        case conn_closed_event:
            funcname = "onEventConnClosed";
            break;
        case conn_finished_event:
            funcname = "onEventConnFinished";
            break;

        default:
            IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Call event handler via a wrapper. */
    lua_getglobal(L, IB_FFI_MODULE_EVENT_WRAPPER_STR);
    if (lua_isfunction(L, -1)) {
        int ec;

        ib_log_debug3(ib,
                     "Executing lua handler \"%s.%s\" via wrapper",
                     modname, funcname);
        lua_pushlightuserdata(L, ib);
        lua_pushstring(L, modname);
        lua_pushstring(L, funcname);
        lua_pushinteger(L, event);
        lua_pushlightuserdata(L, arg);
        lua_pushnil(L); /* reserved */
        /// @todo Use errfunc w/debug.traceback()
        ec = lua_pcall(L, 6, 1, 0);
        if (ec != 0) {
            ib_log_error(ib, "Failed to exec lua wrapper for \"%s.%s\" - %s (%d)",
                         modname, funcname, lua_tostring(L, -1), ec);
            rc = IB_EINVAL;
        }
        else if (lua_isnumber(L, -1)) {
            lua_Integer li = lua_tointeger(L, -1);
            rc = (ib_status_t)(int)li;
        }
        else {
            ib_log_error(ib,
                         "Expected %s returned from lua \"%s.%s\", "
                         "but received %s",
                         lua_typename(L, LUA_TNUMBER),
                         modname, funcname,
                         lua_typename(L, lua_type(L, -1)));
            rc = IB_EINVAL;
        }
    }
    else {
        ib_log_error(ib,
                     "Lua event wrapper function not available - "
                     "could not execute \"%s.%s\" handler",
                     modname, funcname);
    }
    lua_pop(L, 1); /* cleanup stack */


    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Generic event handler for Lua connection data events.
 *
 * @param ib Engine.
 * @param event Event type.
 * @param conndata Connection data.
 * @param cbdata not used.
 *
 * @return Status code.
 */
static ib_status_t modlua_handle_conndata_event(ib_engine_t *ib,
                                                    ib_state_event_type_t event,
                                                    ib_conndata_t *conndata,
                                                    void *cbdata)
{
    IB_FTRACE_INIT();
    ib_conn_t *conn = conndata->conn;
    modlua_cfg_t *modcfg;
    modlua_runtime_t *lua;
    ib_list_t *luaevents;
    ib_list_node_t *node;
    ib_status_t rc;

    /* Get the module config. */
    /// @todo For now, context is in main, not conn
    rc = ib_context_module_config(conn->ctx,
    //rc = ib_context_module_config(ib_context_main(ib),
                                  IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch module %s config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Verify event is in range for an event. */
    if (event >= IB_STATE_EVENT_NUM) {
        ib_log_error(ib, "Lua event was out of range: %u", event);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

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
        ib_log_error(ib, "Failed to fetch lua runtime for conn=%p", conn);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Run through the luaevents list, which is a list of loaded
     * lua modules that have an event handler for this event. The
     * corresponding lua event handler (onEventFoo) need to be
     * executed for each module in the list.
     */
    IB_LIST_LOOP(luaevents, node) {
        ib_module_t *m = (ib_module_t *)ib_list_node_data(node);
        ib_log_debug3(ib, "Lua module \"%s\" (%p) has handler for event[%d]=%s",
                     m->name, m, event, ib_state_event_name(event));
        rc = modlua_exec_lua_handler(ib, conndata, lua, m->name, event);
        if (rc != IB_OK) {
            ib_log_error(ib, "Error executing lua handler");
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Generic event handler for Lua transaction data events.
 *
 * @param ib Engine
 * @param tx Transaction.
 * @param event Event type
 * @param txdata Transaction data
 * @param cbdata Not used
 *
 * @return Status code
 */
static ib_status_t modlua_handle_txdata_event(ib_engine_t *ib,
                                                  ib_tx_t *tx,
                                                  ib_state_event_type_t event,
                                                  ib_txdata_t *txdata,
                                                  void *cbdata)
{
    IB_FTRACE_INIT();
    ib_conn_t *conn = tx->conn;
    modlua_cfg_t *modcfg;
    modlua_runtime_t *lua;
    ib_list_t *luaevents;
    ib_list_node_t *node;
    ib_status_t rc;

    /* Get the module config. */
    /// @todo For now, context is in main, not tx
    rc = ib_context_module_config(tx->ctx,
    //rc = ib_context_module_config(ib_context_main(ib),
                                  IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx, "Failed to fetch module %s config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Verify cbdata is in range for an event. */
    if (event >= IB_STATE_EVENT_NUM) {
        ib_log_error_tx(tx, "Lua event was out of range: %u", event);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Get the list of lua events. If it is NULL, then there are no
     * registered lua events of this type, so just exit cleanly.
     */
    luaevents = modcfg->event_reg[event];
    if (luaevents == NULL) {
        ib_log_error_tx(tx, "No lua events found");
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Get the lua runtime. */
    lua = modlua_runtime_get(conn);
    if (lua == NULL) {
        ib_log_error_tx(tx, "Failed to fetch lua runtime for tx=%p", tx);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Run through the luaevents list, which is a list of loaded
     * lua modules that have an event handler for this event. The
     * corresponding lua event handler (onEventFoo) need to be
     * executed for each module in the list.
     */
    IB_LIST_LOOP(luaevents, node) {
        ib_module_t *module = (ib_module_t *)ib_list_node_data(node);
        ib_log_debug3_tx(tx,
                     "Lua module \"%s\" (%p) has handler for event[%d]=%s",
                     module->name, module, event, ib_state_event_name(event));
        rc = modlua_exec_lua_handler(ib, txdata, lua, module->name, event);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Error executing lua handler");
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Generic event handler for Lua connection events.
 *
 * @param ib Engine.
 * @param event Event type.
 * @param conn Connection.
 * @param cbdata Not used.
 *
 * @return Status code.
 */
static ib_status_t modlua_handle_conn_event(ib_engine_t *ib,
                                                ib_state_event_type_t event,
                                                ib_conn_t *conn,
                                                void *cbdata)
{
    IB_FTRACE_INIT();
    modlua_cfg_t *modcfg;
    modlua_runtime_t *lua;
    ib_list_t *luaevents;
    ib_list_node_t *node;
    ib_status_t rc;

    /* Get the module config. */
    /// @todo For now, context is in main, not conn
    rc = ib_context_module_config(conn->ctx,
    //rc = ib_context_module_config(ib_context_main(ib),
                                  IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch module %s config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Verify cbdata is in range for an event. */
    if (event >= IB_STATE_EVENT_NUM) {
        ib_log_error(ib, "Lua event was out of range: %u", event);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

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
        ib_log_error(ib, "Failed to fetch lua runtime for conn=%p", conn);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Run through the luaevents list, which is a list of loaded
     * lua modules that have an event handler for this event. The
     * corresponding lua event handler (onEventFoo) need to be
     * executed for each module in the list.
     */
    IB_LIST_LOOP(luaevents, node) {
        ib_module_t *module = (ib_module_t *)ib_list_node_data(node);
        ib_log_debug3(ib,
                     "Lua module \"%s\" (%p) has handler for event[%d]=%s",
                     module->name, module, event, ib_state_event_name(event));
        rc = modlua_exec_lua_handler(ib, conn, lua, module->name, event);
        if (rc != IB_OK) {
            ib_log_error(ib, "Error executing lua handler");
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Generic event handler for Lua transaction events.
 *
 * @param ib Engine.
 * @param tx Transaction.
 * @param event Event type.
 * @param cbdata Not used.
 *
 * @return Status code.
 */
static ib_status_t modlua_handle_tx_event(ib_engine_t *ib,
                                              ib_tx_t *tx,
                                              ib_state_event_type_t event,
                                              void *cbdata)
{
    IB_FTRACE_INIT();
    modlua_cfg_t *modcfg;
    modlua_runtime_t *lua;
    ib_list_t *luaevents;
    ib_list_node_t *node;
    ib_status_t rc;

    /* Get the module config. */
    /// @todo For now, context is in main, not tx
    rc = ib_context_module_config(tx->ctx,
    //rc = ib_context_module_config(ib_context_main(ib),
                                  IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx, "Failed to fetch module %s config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Verify cbdata is in range for an event. */
    if (event >= IB_STATE_EVENT_NUM) {
        ib_log_error_tx(tx, "Lua event was out of range: %u", event);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

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
        ib_log_error_tx(tx, "Failed to fetch lua runtime for tx=%p conn=%p", tx, tx->conn);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Run through the luaevents list, which is a list of loaded
     * lua modules that have an event handler for this event. The
     * corresponding lua event handler (onEventFoo) need to be
     * executed for each module in the list.
     */
    IB_LIST_LOOP(luaevents, node) {
        ib_module_t *m = (ib_module_t *)ib_list_node_data(node);
        ib_log_debug3_tx(tx, "Lua module \"%s\" (%p) has handler for event[%d]=%s",
                     m->name, m, event, ib_state_event_name(event));
        rc = modlua_exec_lua_handler(ib, tx, lua, m->name, event);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Error executing lua handler");
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Generic event handler for Lua transaction events.
 *
 * @param ib Engine.
 * @param tx Transaction.
 * @param event Event type.
 * @param line The parsed request line
 * @param cbdata Not used.
 *
 * @return Status code.
 */
static ib_status_t modlua_handle_reqline_event(ib_engine_t *ib,
                                               ib_tx_t *tx,
                                               ib_state_event_type_t event,
                                               ib_parsed_req_line_t *line,
                                               void *cbdata)
{
    IB_FTRACE_INIT();
    modlua_cfg_t *modcfg;
    modlua_runtime_t *lua;
    ib_list_t *luaevents;
    ib_list_node_t *node;
    ib_status_t rc;

    /* Get the module config. */
    /// @todo For now, context is in main, not tx
    rc = ib_context_module_config(tx->ctx,
    //rc = ib_context_module_config(ib_context_main(ib),
                                  IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx, "Failed to fetch module %s config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Verify cbdata is in range for an event. */
    if (event >= IB_STATE_EVENT_NUM) {
        ib_log_error_tx(tx, "Lua event was out of range: %u", event);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

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
        ib_log_error_tx(tx, "Failed to fetch lua runtime for tx=%p conn=%p",
                        tx, tx->conn);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Run through the luaevents list, which is a list of loaded
     * lua modules that have an event handler for this event. The
     * corresponding lua event handler (onEventFoo) need to be
     * executed for each module in the list.
     */
    IB_LIST_LOOP(luaevents, node) {
        ib_module_t *m = (ib_module_t *)ib_list_node_data(node);
        ib_log_debug3_tx(tx,
                         "Lua module \"%s\" (%p) has handler for event[%d]=%s",
                         m->name, m, event, ib_state_event_name(event));
        rc = modlua_exec_lua_handler(ib, line, lua, m->name, event);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Error executing lua handler");
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Generic event handler for Lua transaction events.
 *
 * @param ib Engine.
 * @param tx Transaction.
 * @param event Event type.
 * @param line The parsed response line
 * @param cbdata Not used.
 *
 * @return Status code.
 */
static ib_status_t modlua_handle_respline_event(ib_engine_t *ib,
                                                ib_tx_t *tx,
                                                ib_state_event_type_t event,
                                                ib_parsed_resp_line_t *line,
                                                void *cbdata)
{
    IB_FTRACE_INIT();
    modlua_cfg_t *modcfg;
    modlua_runtime_t *lua;
    ib_list_t *luaevents;
    ib_list_node_t *node;
    ib_status_t rc;

    /* Get the module config. */
    /// @todo For now, context is in main, not tx
    rc = ib_context_module_config(tx->ctx,
    //rc = ib_context_module_config(ib_context_main(ib),
                                  IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert_tx(tx, "Failed to fetch module %s config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Verify cbdata is in range for an event. */
    if (event >= IB_STATE_EVENT_NUM) {
        ib_log_error_tx(tx, "Lua event was out of range: %u", event);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

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
        ib_log_error_tx(tx, "Failed to fetch lua runtime for tx=%p conn=%p",
                        tx, tx->conn);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Run through the luaevents list, which is a list of loaded
     * lua modules that have an event handler for this event. The
     * corresponding lua event handler (onEventFoo) need to be
     * executed for each module in the list.
     */
    IB_LIST_LOOP(luaevents, node) {
        ib_module_t *m = (ib_module_t *)ib_list_node_data(node);
        ib_log_debug3_tx(tx,
                         "Lua module \"%s\" (%p) has handler for event[%d]=%s",
                         m->name, m, event, ib_state_event_name(event));
        rc = modlua_exec_lua_handler(ib, line, lua, m->name, event);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Error executing lua handler");
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Module Routines -- */

static ib_status_t modlua_init(ib_engine_t *ib,
                               ib_module_t *m,
                               void        *cbdata)
{
    IB_FTRACE_INIT();
    ib_list_t *mlist;
    ib_status_t rc;

    /* Set up defaults */
    modlua_global_cfg.lua_modules = NULL;
    memset(
        &modlua_global_cfg.event_reg,
        0,
        sizeof(modlua_global_cfg.event_reg)
    );
    modlua_global_cfg.Lconfig = NULL;

    /* Setup a list to track loaded lua modules. */
    rc = ib_list_create(&mlist, ib_engine_pool_config_get(ib));
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to create lua module list: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }
    m->data = mlist;

    /* Initialize the lua runtime for the configuration. */
    rc = modlua_init_lua_runtime_cfg(ib, NULL, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Hooks to initialize/destroy the lua runtime for configuration. */
    rc = ib_hook_null_register(ib, cfg_finished_event,
                               modlua_destroy_lua_runtime_cfg,
                               NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }

    /* Hook to initialize the lua runtime with the connection. */
    rc = ib_hook_conn_register(ib, conn_started_event,
                          modlua_init_lua_runtime,
                          NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }

    /* Hook to destroy the lua runtime with the connection. */
    rc = ib_hook_conn_register(ib, conn_finished_event,
                          modlua_destroy_lua_runtime,
                          NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }

    /* Register data event handlers. */
    rc = ib_hook_conndata_register(ib, conn_data_in_event,
                                   modlua_handle_conndata_event,
                                   NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_conndata_register(ib, conn_data_out_event,
                                   modlua_handle_conndata_event,
                                   NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }

    /* Register connection event handlers. */
    rc = ib_hook_conn_register(ib, conn_started_event,
                               modlua_handle_conn_event,
                               NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_conn_register(ib, conn_opened_event,
                               modlua_handle_conn_event,
                               NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_conn_register(ib, handle_context_conn_event,
                               modlua_handle_conn_event,
                               NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_conn_register(ib, handle_connect_event,
                               modlua_handle_conn_event,
                               NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_conn_register(ib, conn_closed_event,
                               modlua_handle_conn_event,
                               NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_conn_register(ib, handle_disconnect_event,
                               modlua_handle_conn_event,
                               NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_conn_register(ib, conn_finished_event,
                               modlua_handle_conn_event,
                               NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }

    /* Register transaction event handlers. */
    rc = ib_hook_tx_register(ib, tx_started_event,
                             modlua_handle_tx_event,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_parsed_req_line_register(ib, request_started_event,
                                          modlua_handle_reqline_event,
                                          NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_tx_register(ib, request_header_finished_event,
                             modlua_handle_tx_event,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_tx_register(ib, handle_context_tx_event,
                             modlua_handle_tx_event,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_tx_register(ib, handle_request_header_event,
                             modlua_handle_tx_event,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_txdata_register(ib, request_body_data_event,
                                 modlua_handle_txdata_event,
                                 NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_tx_register(ib, handle_request_event,
                             modlua_handle_tx_event,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_tx_register(ib, request_finished_event,
                             modlua_handle_tx_event,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_tx_register(ib, tx_process_event,
                             modlua_handle_tx_event,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_parsed_resp_line_register(ib, response_started_event,
                                           modlua_handle_respline_event,
                                           NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_tx_register(ib, response_header_finished_event,
                             modlua_handle_tx_event,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_tx_register(ib, handle_response_header_event,
                             modlua_handle_tx_event,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_txdata_register(ib, response_body_data_event,
                                 modlua_handle_txdata_event,
                                 NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_tx_register(ib, handle_response_event,
                             modlua_handle_tx_event,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_tx_register(ib, response_finished_event,
                             modlua_handle_tx_event,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_tx_register(ib, handle_postprocess_event,
                             modlua_handle_tx_event,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }
    rc = ib_hook_tx_register(ib, tx_finished_event,
                             modlua_handle_tx_event,
                             NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register hook: %s",
                     ib_status_to_string(rc));
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modlua_context_close(ib_engine_t  *ib,
                                        ib_module_t  *m,
                                        ib_context_t *ctx,
                                        void         *cbdata)
{
    IB_FTRACE_INIT();
    modlua_cfg_t *modcfg;
    ib_list_t *mlist = (ib_list_t *)m->data;
    ib_list_node_t *node;
    ib_status_t rc;

    /* Get the module config. */
    rc = ib_context_module_config(ctx, IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch module %s config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Check for a valid lua state. */
    if (modcfg->Lconfig == NULL) {
        ib_log_error(ib, "Lua support not available");
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* Init the lua modules that were loaded */
    /// @todo Need a directive for this instead of loading all per context
    IB_LIST_LOOP(mlist, node) {
        ib_module_t *mlua = (ib_module_t *)ib_list_node_data(node);
        modlua_lua_module_init(ib, ctx, mlua->name);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}


/* -- Module Configuration -- */

static IB_CFGMAP_INIT_STRUCTURE(modlua_config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".pkg_path",
        IB_FTYPE_NULSTR,
        modlua_cfg_t,
        pkg_path
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".pkg_cpath",
        IB_FTYPE_NULSTR,
        modlua_cfg_t,
        pkg_cpath
    ),

    IB_CFGMAP_INIT_LAST
};




/* -- Configuration Directives -- */

/* C config callback wrapper for all Lua implemented config directives. */
static ib_status_t modlua_dir_lua_wrapper(ib_cfgparser_t *cp,
                                          const char *name,
                                          ib_list_t *args,
                                          void *cbdata)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib = cp->ib;
    modlua_wrapper_cbdata_t *wcbdata = (modlua_wrapper_cbdata_t *)cbdata;
    ib_list_node_t *node;
    modlua_cfg_t *maincfg;
    lua_State *L;
    ib_status_t rc;

    ib_log_debug2(ib, "Handling Lua Directive: %s", name);
    /* Get the main module config. */
    rc = ib_context_module_config(ib_context_main(ib),
                                  IB_MODULE_STRUCT_PTR, (void *)&maincfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch module main config: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Use the configuration lua state. */
    L = maincfg->Lconfig;

    lua_getglobal(L, IB_FFI_MODULE_WRAPPER_STR);
    if (lua_isfunction(L, -1)) {
        int ec;

        ib_log_debug3(ib,
                     "Executing lua config handler \"%s.%s\" via wrapper",
                     wcbdata->fn_config_modname,
                     wcbdata->fn_config_name);
        lua_pushlightuserdata(L, ib);
        lua_pushstring(L, wcbdata->fn_config_modname);
        lua_pushstring(L, wcbdata->fn_config_name);
        if (strcmp("string", wcbdata->cbdata_type) == 0) {
            lua_pushstring(L, (const char *)wcbdata->cbdata);
        }
        else if (strcmp("number", wcbdata->cbdata_type) == 0) {
            lua_pushnumber(L, *(lua_Number *)wcbdata->cbdata);
        }
        else {
            lua_pushlightuserdata(L, wcbdata->cbdata);
        }
        IB_LIST_LOOP(args, node) {
            lua_pushstring(L, (const char *)ib_list_node_data(node));
        }
        /// @todo Use errfunc w/debug.traceback()
        ec = lua_pcall(L, 4 + ib_list_elements(args), 1, 0);
        if (ec != 0) {
            ib_log_error(ib,
                         "Failed to exec lua directive wrapper for \"%s.%s\": "
                         "%s (%d)",
                         wcbdata->fn_config_modname,
                         wcbdata->fn_config_name,
                         lua_tostring(L, -1), ec);
            rc = IB_EINVAL;
        }
        else if (lua_isnumber(L, -1)) {
            lua_Integer li = lua_tointeger(L, -1);
            rc = (ib_status_t)(int)li;
        }
        else {
            ib_log_error(ib,
                         "Expected %s returned from lua "
                         "\"%s.%s\", but received %s",
                         lua_typename(L, LUA_TNUMBER),
                         wcbdata->fn_config_modname,
                         wcbdata->fn_config_name,
                         lua_typename(L, lua_type(L, -1)));
            rc = IB_EINVAL;
        }
    }
    else {
        ib_log_error(ib,
                     "Lua config wrapper function not available - "
                     "could not execute \"%s.%s\" handler",
                     wcbdata->fn_config_modname,
                     wcbdata->fn_config_name);
        rc = IB_EUNKNOWN;
    }
    lua_pop(L, 1); /* cleanup stack */

    IB_FTRACE_RET_STATUS(rc);
}

/* C blkend callback wrapper for all Lua implemented config directives. */
static ib_status_t modlua_blkend_lua_wrapper(ib_cfgparser_t *cp,
                                             const char *name,
                                             void *cbdata)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib = cp->ib;
    modlua_wrapper_cbdata_t *wcbdata = (modlua_wrapper_cbdata_t *)cbdata;
    modlua_cfg_t *maincfg;
    lua_State *L;
    ib_status_t rc;

    ib_log_debug2(ib, "Handling Lua Directive: %s", name);
    /* Get the main module config. */
    rc = ib_context_module_config(ib_context_main(ib),
                                  IB_MODULE_STRUCT_PTR, (void *)&maincfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch module main config: %s", ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Use the configuration lua state. */
    L = maincfg->Lconfig;

    lua_getglobal(L, IB_FFI_MODULE_WRAPPER_STR);
    if (lua_isfunction(L, -1)) {
        int ec;

        ib_log_debug3(ib,
                     "Executing lua config handler \"%s.%s\" via wrapper",
                     wcbdata->fn_config_modname,
                     wcbdata->fn_config_name);
        lua_pushlightuserdata(L, ib);
        lua_pushstring(L, wcbdata->fn_config_modname);
        lua_pushstring(L, wcbdata->fn_config_name);
        if (strcmp("string", wcbdata->cbdata_type) == 0) {
            lua_pushstring(L, (const char *)wcbdata->cbdata);
        }
        else if (strcmp("number", wcbdata->cbdata_type) == 0) {
            lua_pushnumber(L, *(lua_Number *)wcbdata->cbdata);
        }
        else {
            lua_pushlightuserdata(L, wcbdata->cbdata);
        }
        /// @todo Use errfunc w/debug.traceback()
        ec = lua_pcall(L, 4, 1, 0);
        if (ec != 0) {
            ib_log_error(ib,
                         "Failed to exec lua directive wrapper for \"%s.%s\": "
                         "%s (%d)",
                         wcbdata->fn_config_modname,
                         wcbdata->fn_config_name,
                         lua_tostring(L, -1), ec);
            rc = IB_EINVAL;
        }
        else if (lua_isnumber(L, -1)) {
            lua_Integer li = lua_tointeger(L, -1);
            rc = (ib_status_t)(int)li;
        }
        else {
            ib_log_error(ib,
                         "Expected %s returned from lua "
                         "\"%s.%s\", but received %s",
                         lua_typename(L, LUA_TNUMBER),
                         wcbdata->fn_config_modname,
                         wcbdata->fn_config_name,
                         lua_typename(L, lua_type(L, -1)));
            rc = IB_EINVAL;
        }
    }
    else {
        ib_log_error(ib,
                     "Lua config wrapper function not available - "
                     "could not execute \"%s.%s\" handler",
                     wcbdata->fn_config_modname,
                     wcbdata->fn_config_name);
        rc = IB_EUNKNOWN;
    }
    lua_pop(L, 1); /* cleanup stack */

    IB_FTRACE_RET_STATUS(rc);
}

ib_void_fn_t modlua_config_wrapper(void)
{
    return (ib_void_fn_t)modlua_dir_lua_wrapper;
}

ib_config_cb_blkend_fn_t modlua_blkend_wrapper(void)
{
    return modlua_blkend_lua_wrapper;
}

static ib_status_t modlua_dir_param1(ib_cfgparser_t *cp,
                                     const char *name,
                                     const char *p1,
                                     void *cbdata)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib = cp->ib;
    ib_status_t rc;
    ib_core_cfg_t *corecfg = NULL;
    const size_t pathmax = 512;
    char *path = NULL;
    size_t p1_len = strlen(p1);
    size_t p1_unescaped_len;
    char *p1_unescaped = malloc(p1_len+1);

    if ( p1_unescaped == NULL ) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    rc = ib_util_unescape_string(p1_unescaped,
                                 &p1_unescaped_len,
                                 p1,
                                 p1_len,
                                 IB_UTIL_UNESCAPE_NULTERMINATE |
                                 IB_UTIL_UNESCAPE_NONULL);

    if (rc != IB_OK) {
        const char *msg = (rc == IB_EBADVAL)?
            "Value for parameter \"%s\" may not contain NULL bytes: %s":
            "Value for parameter \"%s\" could not be unescaped: %s";
        ib_log_debug(ib, msg, name, p1);
        free(p1_unescaped);
        IB_FTRACE_RET_STATUS(rc);
    }


    rc = ib_context_module_config(ib_context_main(ib),
                                  ib_core_module(),
                                  (void *)&corecfg);

    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve core configuration.");
        IB_FTRACE_RET_STATUS(rc);
    }

    if (strcasecmp("LuaLoadModule", name) == 0) {
        if (*p1_unescaped == '/') {
            modlua_module_load(ib, p1_unescaped);
        }
        else {
            path = malloc(pathmax);

            if (path==NULL) {
                ib_log_error(ib, "Cannot allocate memory for module path.");
                free(p1_unescaped);
                IB_FTRACE_RET_STATUS(IB_EALLOC);
            }

            size_t len = snprintf(path, pathmax, "%s/%s",
                                  corecfg->module_base_path,
                                  p1_unescaped);

            if (len >= pathmax) {
                ib_log_error(ib, "Filename too long: %s %s", name, p1_unescaped);
                free(path);
                path = NULL;
                free(p1_unescaped);
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            }

            modlua_module_load(ib, path);
            free(path);
            path = NULL;
        }
    }
    else if (strcasecmp("LuaPackagePath", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);
        rc = ib_context_set_string(ctx, MODULE_NAME_STR ".pkg_path", p1_unescaped);
        free(p1_unescaped);
        IB_FTRACE_RET_STATUS(rc);
    }
    else if (strcasecmp("LuaPackageCPath", name) == 0) {
        ib_context_t *ctx = cp->cur_ctx ? cp->cur_ctx : ib_context_main(ib);
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);
        rc = ib_context_set_string(ctx, MODULE_NAME_STR ".pkg_cpath", p1_unescaped);
        free(p1_unescaped);
        IB_FTRACE_RET_STATUS(rc);
    }
    else {
        ib_log_error(ib, "Unhandled directive: %s %s", name, p1_unescaped);
        free(p1_unescaped);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    free(p1_unescaped);
    IB_FTRACE_RET_STATUS(IB_OK);
}

static IB_DIRMAP_INIT_STRUCTURE(modlua_directive_map) = {
    IB_DIRMAP_INIT_PARAM1(
        "LuaLoadModule",
        modlua_dir_param1,
        NULL
    ),

    IB_DIRMAP_INIT_PARAM1(
        "LuaPackagePath",
        modlua_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "LuaPackageCPath",
        modlua_dir_param1,
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
    NULL,                                /**< Callback data */
    NULL,                                /**< Finish function */
    NULL,                                /**< Callback data */
    NULL,                                /**< Context open function */
    NULL,                                /**< Callback data */
    modlua_context_close,                /**< Context close function */
    NULL,                                /**< Callback data */
    NULL,                                /**< Context destroy function */
    NULL                                 /**< Callback data */
);
