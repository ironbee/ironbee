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

#include <ironbee/cfgmap.h>
#include <ironbee/debug.h>
#include <ironbee/engine.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>
#include <ironbee/util.h>
#include <ironbee/list.h>
#include <ironbee/config.h>
#include <ironbee/rule_engine.h>
#include <ironbee/rule_parser.h>

#include "lua/ironbee.h"
#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 *  * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>

#include <lua.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        rules
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/// @todo Fix this:
#ifndef X_MODULE_BASE_PATH
#define X_MODULE_BASE_PATH IB_XSTRINGIFY(MODULE_BASE_PATH)
#endif

/**
 * @brief LuaJit FFI definitions.
 * @details This is loaded into the Lua environment for use by Lua rules.
 */
static const char* c_ffi_file = X_MODULE_BASE_PATH "/ironbee-ffi.lua";

#define THREAD_NAME_BUFFER_SZ 20

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/**
 * Ironbee's root rule state.
 */
static lua_State *g_ironbee_rules_lua;

/**
 * @brief Semaphore ID used to protect Lua thread creation and destruction.
 */
static int g_lua_lock;

/**
 * @brief Callback type for functions executed protected by g_lua_lock.
 * @details This callback should take a @c ib_engine_t* which is used
 *          for logging and a @c lua_State** which will be assigned a
 *          new @c lua_State*.
 */
typedef ib_status_t(*critical_section_fn_t)(ib_engine_t*, lua_State**);

/**
 * @brief Counter used to generate internal rule IDs.
 */
static int ironbee_loaded_rule_count;

/**
 * @brief Add a lua rule stored in a file to the Ironbee engine.
 * @param[in,out] ib Used for logging and adding the Lua rule to.
 * @param[in,out] L The Lua state used to load @a file and store the rule.
 * @param[in] func_name The name the contents of the file will be stored under.
 * @param[in] file The file that holds the Lua script that makes up the rule.
 * @returns The IronBee status. IB_OK on success.
 */
static ib_status_t add_lua_rule(ib_engine_t *ib,
                                lua_State* L,
                                const char* func_name,
                                const char* file)
{
    IB_FTRACE_INIT(add_lua_rule);
    ib_status_t ib_rc;

    /* Load (compile) the lua module. */
    ib_rc = luaL_loadfile(L, file);
  
    if (ib_rc != 0) {
        ib_log_error(ib, 1, "Failed to load file module \"%s\" - %s (%d)",
                     file, lua_tostring(L, -1), ib_rc);

        /* Get error string off the stack. */
        lua_pop(L, 1);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
  
    lua_setglobal(L, func_name);

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @brief Load the ironbee-ffi.lua file into the given Lua state.
 * @param[in] ib IronBee engine used to log.
 * @param[out] L The Lua state to load the file into.
 * @returns The IronBee status.
 */
static ib_status_t load_ironbee_ffi(ib_engine_t* ib, lua_State* L)
{
    IB_FTRACE_INIT(load_inrbee_ffi);

    int lua_rc;

    lua_rc = luaL_loadfile(L, c_ffi_file);

    if (lua_rc != 0) {
        ib_log_error(ib, 1, "Failed to load %s - %s (%d)", 
                     c_ffi_file,
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
            ib_log_error(ib, 1, "Error evaluating ffi %s - %s",
                         c_ffi_file,
                         lua_tostring(L, -1));
            /* Get error string off of the stack. */
            lua_pop(L, 1);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        case LUA_ERRMEM:
            ib_log_error(ib, 1,
                "Failed to allocate memory during FFI evaluation.");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        case LUA_ERRERR:
            ib_log_error(ib, 1,
                "Error fetching error message during FFI evaluation.");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
#if LUA_VERSION_NUM > 501
        /* If LUA_ERRGCMM is defined, include a custom error for it as well. 
          This was introduced in Lua 5.2. */
        case LUA_ERRGCMM:
            ib_log_error(ib, 1,
                "Garbage collection error during FFI evaluation.");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
#endif
        default:
            ib_log_error(ib, 1, "Unexpected error(%d) during FFI evaluation.",
                         lua_rc);
            IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
}

/**
 * @brief Call the Lua function \a func_name in the @c lua_State @a L and treat it as an IronBee rule.
 * @param[in] ib The IronBee context. Used for logging.
 * @param[in,out] tx The transaction object. This is passed to the rule
 *                as the local variable @c tx.
 * @param[in] L The Lua execution state/stack to use to call the rule.
 * @param[in] func_name The name of the Lua function to call.
 * @returns IronBee status.
 */
static ib_status_t call_lua_rule(ib_engine_t *ib,
                                 ib_tx_t *tx,
                                 lua_State *L,
                                 const char *func_name)
{
    IB_FTRACE_INIT(call_lua_rule);
  
    int lua_rc;
  
    if (!lua_checkstack(L, 5)) {
        ib_log_error(ib, 1, 
            "Not enough stack space to call Lua rule %s.", func_name);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
  
    /* Push the function on the stack. Preparation to call. */
    lua_getglobal(L, func_name);
  
    if (!lua_isfunction(L, -1)) {
        ib_log_error(ib, 1, "Variable \"%s\" is not a LUA function - %s",
                     func_name);

        /* Remove wrong parameter from stack. */
        lua_pop(L, 1);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
  
    /* Create a table for the coming function call. */
    lua_newtable(L);

    /* Push key. */
    lua_pushstring(L, "tx");

    /* Push value. */
    lua_pushlightuserdata(L, tx);

    /* Take key -2 and value -1 and assign it into -3. */
    lua_settable(L, -3);

    /* Pop the table and set it as the local env. */
    lua_setfenv(L, -1);
  
    /* Call the function on the stack with 1 input, 0 outputs, and errmsg=0. */
    lua_rc = lua_pcall(L, 1, 0, 0);
  
    /* Only check errors if ec is not 0 (LUA_OK). */
    if (lua_rc != 0) {
        switch(lua_rc) {
            case LUA_ERRRUN:
                ib_log_error(ib, 1,
                    "Error running Lua Rule %s - %s",
                    func_name,
                    lua_tostring(L, -1));

                /* Get error string off of the stack. */
                lua_pop(L, 1);
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            case LUA_ERRMEM:
                ib_log_error(ib, 1,
                    "Failed to allocate memory during Lua rule.");
                IB_FTRACE_RET_STATUS(IB_EINVAL);
            case LUA_ERRERR:
                ib_log_error(ib, 1,
                    "Error fetching error message during Lua rule.");
                IB_FTRACE_RET_STATUS(IB_EINVAL);
#if LUA_VERSION_NUM > 501
            /* If LUA_ERRGCMM is defined, include a custom error for it as
               well. This was introduced in Lua 5.2. */
            case LUA_ERRGCMM:
                ib_log_error(ib, 1,
                    "Garbage collection error during Lua rule.");
                IB_FTRACE_RET_STATUS(IB_EINVAL);
#endif
            default:
                ib_log_error(ib, 1,
                    "Unexpected error (%d) during Lua rule.", lua_rc);
                IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }
  
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
    sprintf(thread_name, "t_%p", (void*)L);
}

/**
 * @brief Spawn a new Lua thread and place a pointer to it in @a L.
 * @details This is intended to be called by
 * call_in_critical_section(ib_engine_t*, critical_section_fn_t, lua_State**)
 * only.
 * @param[out] ib The IronBee engine used to log errors.
 * @param[out] L The pointer to the newly created Lua state.
 * @returns IB_OK on success.
 */
static ib_status_t spawn_thread(ib_engine_t *ib, lua_State **L)
{
    IB_FTRACE_INIT(spawn_thread);
    char *Lname = (char*)malloc(THREAD_NAME_BUFFER_SZ);

    ib_log_debug(ib, 1, "Setting up new Lua thread.");

    *L = lua_newthread(g_ironbee_rules_lua);

    if (L == NULL) {
        ib_log_error(ib, 1, "Failed to allocate new Lua execution stack.");
        free(Lname);
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    sprint_threadname(Lname, *L);

    ib_log_debug(ib, 1, "Created Lua thread %s.", Lname);

    /* Store the thread at the global variable referenced. */
    lua_setglobal(g_ironbee_rules_lua, Lname);

    free(Lname);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @brief Destroy a new Lua thread pointed to by @a L.
 * @details This is intended to be called by
 * call_in_critical_section(ib_engine_t*, critical_section_fn_t, lua_State**)
 * only.
 * @param[out] ib The IronBee engine used to log errors.
 * @param[out] L The pointer to the Lua state to be destroyed.
 * @returns IB_OK on success.
 */
static ib_status_t join_thread(ib_engine_t *ib, lua_State **L)
{
    IB_FTRACE_INIT(join_thread);
    char *Lname = (char*)malloc(THREAD_NAME_BUFFER_SZ);
    sprint_threadname(Lname, *L);

    ib_log_debug(ib, 1, "Tearing down Lua thread %s.", Lname);

    /* Put nil on the stack. */
    lua_pushnil(g_ironbee_rules_lua);

    /* Erase the reference to the stack to allow GC. */
    lua_setglobal(g_ironbee_rules_lua, Lname);

    free(Lname);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @brief This will use @c g_lua_lock to atomically call @a fn.
 * @details The argument @fn will be either
 *          spawn_thread(ib_engine_t*, lua_State**) or
 *          join_thread(ib_engine_t*, lua_State**) which will be called
 *          only if @c g_lua_lock can be locked using @c semop.
 * @param[in] ib IronBee context. Used for logging.
 * @param[in] fn The function to execute. This is passed @a ib and @a fn.
 * @param[in,out] L The Lua State to create or destroy. Passed to @a fn.
 * @returns If any error locking or unlocking the
 *          semaphore is encountered, IB_EUNKNOWN is returned.
 *          Otherwise the result of \a fn is returned.
 */
static ib_status_t call_in_critical_section(ib_engine_t *ib,
                                            critical_section_fn_t fn,
                                            lua_State **L)
{
    IB_FTRACE_INIT(call_in_critical_section);

    /* Return code from system calls. */
    int sys_rc;

    /* Return code from IronBee calls. */
    ib_status_t ib_rc;

    struct sembuf lock_sops[2];
    struct sembuf unlock_sop;

    /* To unlock, decrement to 0. */
    unlock_sop.sem_num = 0;
    unlock_sop.sem_op = -1;
    unlock_sop.sem_flg = 0;

    /* Wait for 0. */
    lock_sops[0].sem_num = 0;
    lock_sops[0].sem_op = 0;
    lock_sops[0].sem_flg = 0;

    /* Increment, taking ownership of the semaphore. */
    lock_sops[1].sem_num = 0;
    lock_sops[1].sem_op = 1;
    lock_sops[1].sem_flg = 0;

    sys_rc = semop(g_lua_lock, lock_sops, 2);

    /* Report semop error and return. */
    if (sys_rc == -1) {
        ib_log_error(ib, 1, "Failed to lock Lua context - %s.",
                     strerror(errno));
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Execute lua call in critical section. */
    ib_rc = fn(ib, L);

    sys_rc = semop(g_lua_lock, &unlock_sop, 1);

    /* Report semop error and return. */
    if (sys_rc == -1) {
        ib_log_error(ib, 1, "Failed to unlock Lua context - %s.",
                     strerror(errno));
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    IB_FTRACE_RET_STATUS(ib_rc);
}

/**
 * @brief Call the rule named @a func_name on a new Lua stack.
 * @details This will atomically create and destroy a lua_State*
 *          allowing for concurrent execution of @a func_name
 *          by a call_lua_rule(ib_engine_t*, ib_txt_t*, const char*).
 * @param[in] ib IronBee context.
 * @param[in,out] tx The transaction. The Rule may color this with data.
 * @param[in] func_name the Lua function name to call.
 * @returns IB_OK on success, IB_EUNKNOWN on semaphore locking error, and
 *          IB_EALLOC is returned if a new execution stack cannot be created.
 *
 * FIXME - add static after this function is wired into the engine.
 *
 */
ib_status_t call_lua_rule_r(ib_engine_t*, ib_tx_t* , const char* );
ib_status_t call_lua_rule_r(ib_engine_t *ib,
                            ib_tx_t *tx,
                            const char *func_name)
{
    IB_FTRACE_INIT(call_lua_rule_r);

    ib_status_t ib_rc;

    lua_State *L;

    /* Atomically create a new Lua stack */
    ib_rc = call_in_critical_section(ib, &spawn_thread, &L);

    if (ib_rc != IB_OK) {
        IB_FTRACE_RET_STATUS(ib_rc);
    }
    
    /* Call the rule in isolation. */
    ib_rc = call_lua_rule(ib, tx, L, func_name);

    if (ib_rc != IB_OK) {
        IB_FTRACE_RET_STATUS(ib_rc);
    }

    /* Atomically destroy the Lua stack */
    ib_rc = call_in_critical_section(ib, &join_thread, &L);

    IB_FTRACE_RET_STATUS(ib_rc);
}

/**
 * @brief Parse a RuleExt directive.
 * @details Register lua function. RuleExt lua:/path/to/rule.lua phase:REQUEST
 * @param[in,out] cp Configuration parser that contains the engine being
 *                configured.
 * @param[in] name The directive name.
 * @param[in] vars The list of variables passed to @c name.
 * @param[in] cbdata User data. Unused.
 */
static ib_status_t rules_ruleext_params(ib_cfgparser_t *cp,
                                        const char *name,
                                        ib_list_t *vars,
                                        void *cbdata)
{
    IB_FTRACE_INIT(rules_ruleext_params);
  
    ib_list_node_t *var = ib_list_first(vars);
  
    char *file = NULL;
    char *phase = NULL;
    char *rule_name;

    ib_log_debug(cp->ib, 1, "Processing directive %s", name);

    if (cbdata!=NULL) {
        IB_FTRACE_MSG("Callback data is not null.");
    }
  
    if (var==NULL) {
        ib_log_error(cp->ib, 1, "RuleExt file.");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
  
    if (var->data==NULL) {
        ib_log_error(cp->ib, 1, "RuleExt file value missing.");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
  
    file = var->data;
  
    ib_log_debug(cp->ib, 1, "File %s", phase);

    var = ib_list_node_next(var);
  
    if (var==NULL) {
        ib_log_error(cp->ib, 1, "RuleExt rule phase.");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
  
    if (var->data==NULL) {
        ib_log_error(cp->ib, 1, "RuleExt rule phase value missing.");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
  
    phase = var->data;
  
    ib_log_debug(cp->ib, 1, "Phase %s", phase);

    if (strncasecmp(file, "lua:", 4)) {
        /* Lua rule. */
        ironbee_loaded_rule_count+=1;

        /* 4 (lua:) + 10 (%010d) + '\n' = 15 characters. */
        rule_name = (char*)malloc(15);
        sprintf(rule_name, "lua:%010d", ironbee_loaded_rule_count);
        add_lua_rule(cp->ib, g_ironbee_rules_lua, file+4, rule_name);

        /* FIXME - insert here registering with ib engine the lua rule. */

        free(rule_name);
        rule_name = NULL;
    }
    else {
        /* Some unidentified rule. */
        ib_log_error(cp->ib, 1, "RuleExt does not support rule type %s.", file);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
  
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @brief Parse a Rule directive.
 * @details Register a Rule directive to the engine.
 * @param[in,out] cp Configuration parser that contains the engine being
 *                configured.
 * @param[in] name The directive name.
 * @param[in] vars The list of variables passed to @c name.
 * @param[in] cbdata User data. Unused.
 */
static ib_status_t rules_rule_params(ib_cfgparser_t *cp,
                                     const char *name,
                                     ib_list_t *vars,
                                     void *cbdata)
{
    IB_FTRACE_INIT(rules_rule_params);
    ib_status_t     rc;
    ib_list_node_t *inputs;
    ib_list_node_t *op;
    ib_list_node_t *mod;
    ib_rule_t      *rule;

    ib_log_debug(cp->ib, 1, "Name: %s", name);

    if (cbdata != NULL) {
        IB_FTRACE_MSG("Callback data is not null.");
    }

    /* Get the inputs string */
    inputs = ib_list_first(vars);
    if ( (inputs == NULL) || (inputs->data == NULL) ) {
        ib_log_error(cp->ib, 1, "No inputs for rule");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Get the operator string */
    op = ib_list_node_next(inputs);
    if ( (op == NULL) || (op->data == NULL) ) {
        ib_log_error(cp->ib, 1, "No operator for rule");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Allocate a rule */
    rc = ib_rule_create(cp->ib, cp->cur_ctx, &rule);
    if (rc != IB_OK) {
        ib_log_error(cp->ib, 1, "Failed to allocate rule: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    
    /* Parse the inputs */
    rc = ib_rule_parse_inputs(cp, rule, inputs->data);
    if (rc != IB_OK) {
        ib_log_error(cp->ib, 1,
                     "Error parsing rule inputs: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }
    
    /* Parse the operator */
    rc = ib_rule_parse_operator(cp, rule, op->data);
    if (rc != IB_OK) {
        ib_log_error(cp->ib, 1,
                     "Error parsing rule inputs: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Parse all of the modifiers */
    mod = op;
    while( (mod = ib_list_node_next(mod)) != NULL) {
        rc = ib_rule_parse_modifier(cp, rule, mod->data);
        if (rc != IB_OK) {
        }
    }

    /* Finally, register the rule */
    rc = ib_rule_register(cp->ib, cp->cur_ctx, rule, PHASE_REQUEST_HEADER);
    if (rc != IB_OK) {
        ib_log_error(cp->ib, 1, "Error registering rule: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);
}


static IB_DIRMAP_INIT_STRUCTURE(rules_directive_map) = {

    /* Give the config parser a callback for the Rule and RuleExt directive */
    IB_DIRMAP_INIT_LIST(
        "Rule",
        rules_rule_params,
        NULL
    ),
    
    IB_DIRMAP_INIT_LIST(
        "RuleExt",
        rules_ruleext_params,
        NULL
    ),

    /* signal the end of the list */
    IB_DIRMAP_INIT_LAST
};

static ib_status_t rules_init(ib_engine_t *ib, ib_module_t *m)
{
    IB_FTRACE_INIT(rules_init);

    /* Return code from system calls. */
    int sys_rc;

    /* Error code from Iron Bee calls. */
    ib_status_t ib_rc;

    /* Snipped from the Linux man page semctl(2). */
    union semun {
        int              val;    /* Value for SETVAL */
        struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
        unsigned short  *array;  /* Array for GETALL, SETALL */
        struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
    } sem_val;

    /* Initialize semaphore */
    sem_val.val=1;
    g_lua_lock = semget(IPC_PRIVATE, 1, S_IRUSR|S_IWUSR);

    if (g_lua_lock == -1) {
        ib_log_error(ib, 1,
          "Failed to initialize Lua runtime lock - %s", strerror(errno));
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    sys_rc = semctl(g_lua_lock, 0, SETVAL, sem_val);

    if (sys_rc == -1) {
        ib_log_error(ib, 1,
          "Failed to initialize Lua runtime lock - %s", strerror(errno));
        semctl(g_lua_lock, IPC_RMID, 0);
        g_lua_lock = -1;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    ib_log_debug(ib, 1, "Initializing rules module.");

    if (m == NULL) {
        IB_FTRACE_MSG("Module is null.");
        semctl(g_lua_lock, IPC_RMID, 0);
        g_lua_lock = -1;
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
  
    ironbee_loaded_rule_count = 0;
    g_ironbee_rules_lua = luaL_newstate();
    luaL_openlibs(g_ironbee_rules_lua);

    ib_rc = load_ironbee_ffi(ib, g_ironbee_rules_lua);

    if (ib_rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to load FFI file for Lua rule execution.");
        semctl(g_lua_lock, IPC_RMID, 0);
        g_lua_lock = -1;
        IB_FTRACE_RET_STATUS(ib_rc);
    }
   
    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t rules_fini(ib_engine_t *ib, ib_module_t *m)
{
    IB_FTRACE_INIT(rules_fini);
    ib_log_debug(ib, 4, "Rules module unloading.");
    
    if (g_lua_lock >= 0) {
        semctl(g_lua_lock, IPC_RMID, 0);
    }

    if (m == NULL) {
        IB_FTRACE_MSG("Module is null.");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (g_ironbee_rules_lua != NULL) {
        lua_close(g_ironbee_rules_lua);
    }
    
    IB_FTRACE_RET_STATUS(IB_OK);
}

/* Initialize the module structure. */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /* Default metadata */
    MODULE_NAME_STR,                     /* Module name */
    IB_MODULE_CONFIG_NULL,               /* Global config data */
    NULL,                                /* Configuration field map */
    rules_directive_map,                 /* Config directive map */
    rules_init,                          /* Initialize function */
    rules_fini,                          /* Finish function */
    NULL,                                /* Context init function */
    NULL                                 /* Context fini function */
);

