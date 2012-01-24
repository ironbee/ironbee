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
#include <rule_parser.h>
#include <rules_lua.h>

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
 *          for logging, @c a lua_State* which is used to create the 
 *          new thread, and a @c lua_State** which will be assigned a
 *          new @c lua_State*.
 */
typedef ib_status_t(*critical_section_fn_t)(ib_engine_t*, lua_State*, lua_State**);

/**
 * @brief Counter used to generate internal rule IDs.
 */
static int ironbee_loaded_rule_count;

/**
 * @brief This will use @c g_lua_lock to atomically call @a fn.
 * @details The argument @fn will be either
 *          ib_lua_new_thread(ib_engine_t*, lua_State**) or
 *          ib_lua_join_thread(ib_engine_t*, lua_State**) which will be called
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
    ib_rc = fn(ib, g_ironbee_rules_lua, L);

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
 *          by a ib_lua_func_eval(ib_engine_t*, ib_txt_t*, const char*).
 * @param[in] ib IronBee context.
 * @param[in,out] tx The transaction. The Rule may color this with data.
 * @param[in] func_name the Lua function name to call.
 * @returns IB_OK on success, IB_EUNKNOWN on semaphore locking error, and
 *          IB_EALLOC is returned if a new execution stack cannot be created.
 *
 * FIXME - add static after this function is wired into the engine.
 *
 */
ib_status_t ib_lua_func_eval_r(ib_engine_t *ib,
                               ib_tx_t *tx,
                               const char *func_name);
ib_status_t ib_lua_func_eval_r(ib_engine_t *ib,
                               ib_tx_t *tx,
                               const char *func_name)
{
    IB_FTRACE_INIT(ib_lua_func_eval_r);

    ib_status_t ib_rc;

    lua_State *L;

    /* Atomically create a new Lua stack */
    ib_rc = call_in_critical_section(ib, &ib_lua_new_thread, &L);

    if (ib_rc != IB_OK) {
        IB_FTRACE_RET_STATUS(ib_rc);
    }
    
    /* Call the rule in isolation. */
    ib_rc = ib_lua_func_eval(ib, tx, L, func_name);

    if (ib_rc != IB_OK) {
        IB_FTRACE_RET_STATUS(ib_rc);
    }

    /* Atomically destroy the Lua stack */
    ib_rc = call_in_critical_section(ib, &ib_lua_join_thread, &L);

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

    //---
    ib_status_t rc;
    ib_list_node_t *inputs;
    ib_list_node_t *mod;
    ib_rule_t *rule;
    char* file_name;

    /* Get the inputs string */
    inputs = ib_list_first(vars);
    if ( (inputs == NULL) || (inputs->data == NULL) ) {
        ib_log_error(cp->ib, 1, "No inputs for rule");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Allocate a rule */
    rc = ib_rule_create(cp->ib, cp->cur_ctx, &rule);
    if (rc != IB_OK) {
        ib_log_error(cp->ib, 1, "Failed to allocate rule: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    file_name = inputs->data;

    /* Parse all of the modifiers */
    mod = inputs;
    while( (mod = ib_list_node_next(mod)) != NULL) {
        rc = ib_rule_parse_modifier(cp, rule, mod->data);
        if (rc != IB_OK) {
        }
    }

    /* Using the rule->meta and file_name, load and stage the ext rule. */
    /* FIXME
    if (strncasecmp(file_name, "lua:", 4)) {
        ib_lua_load_func(cp->ib,
                         g_ironbee_rules_lua,
                         file_name+4,
                         rule->meta.id);
    }
    else {
        ib_log_error(cp->ib, 1, "RuleExt does not support rule type %s.",
            file_name);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
    */

    /* Finally, register the rule */
    rc = ib_rule_register(cp->ib, cp->cur_ctx, rule, PHASE_REQUEST_HEADER);
    if (rc != IB_OK) {
        ib_log_error(cp->ib, 1, "Error registering rule: %d", rc);
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Done */
    IB_FTRACE_RET_STATUS(IB_OK);

    //---
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
        ib_lua_load_func(cp->ib, g_ironbee_rules_lua, file+4, rule_name);

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

    /* Load and evaluate the ffi file. */
    ib_rc = ib_lua_load_eval(ib, g_ironbee_rules_lua, c_ffi_file);
    if (ib_rc != IB_OK) {
        ib_log_error(ib, 1, 
            "Failed to eval \"%s\" for Lua rule execution.",
            c_ffi_file);
        semctl(g_lua_lock, IPC_RMID, 0);
        g_lua_lock = -1;
        IB_FTRACE_RET_STATUS(ib_rc);
    }

    /* Require the ironbee module we just evaled. */
    ib_rc = ib_lua_require(ib, g_ironbee_rules_lua, "ironbee", "ironbee-ffi");
    if (ib_rc != IB_OK) {
        ib_log_error(ib, 1,
            "Failed to require \"%s\" for Lua rule execution.",
            c_ffi_file);
        semctl(g_lua_lock, IPC_RMID, 0);
        g_lua_lock = -1;
        IB_FTRACE_RET_STATUS(ib_rc);
    }

    /* Require the ffi module. */
    ib_rc = ib_lua_require(ib, g_ironbee_rules_lua, "ffi", "ffi");
    if (ib_rc != IB_OK) {
        ib_log_error(ib, 1, "Failed to load FFI for Lua rule execution.");
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

