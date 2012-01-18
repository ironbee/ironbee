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

#include <math.h>
#include <strings.h>

#include <ironbee/cfgmap.h>
#include <ironbee/debug.h>
#include <ironbee/engine.h>
#include <ironbee/module.h>
#include <ironbee/provider.h>
#include <ironbee/util.h>
#include <ironbee/config.h>

#include "lua/ironbee.h"
#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 *  * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <lua.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        rules
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/**
 * Ironbee's root rule state.
 */
static lua_State *ironbee_rules_lua;
static int ironbee_loaded_rule_count;

static ib_status_t add_lua_rule(ib_engine_t *ib,
                                lua_State* L,
                                const char* func_name,
                                const char* file)
{
  IB_FTRACE_INIT(add_lua_rule);
  ib_status_t ec;

  /* Load (compile) the lua module. */
  ec = luaL_loadfile(L, file);
  
  if (ec != 0) {
      ib_log_error(ib, 1, "Failed to load file module \"%s\" - %s (%d)",
                   file, lua_tostring(L, -1), ec);

      /* Get error string off the stack. */
      lua_pop(L, 1);
      IB_FTRACE_RET_STATUS(IB_EINVAL);
  }
  
  /* lua_isfunction(L, -1 ); */
  lua_setglobal(L, func_name);

  IB_FTRACE_RET_STATUS(IB_OK);
}

/* FIXME - add static after this is used. */
ib_status_t call_lua_rule(ib_engine_t *, lua_State*, const char*);
ib_status_t call_lua_rule(ib_engine_t *ib,
                                 lua_State* L,
                                 const char* func_name)
{
  IB_FTRACE_INIT(call_lua_rule);
  
  int ec;
  
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
  
  // FIXME - Push the arguments. 
  
  /* Call the function on the stack wit 1 input, 0 outputs, and errmsg=0. */
  ec = lua_pcall(L, 1, 0, 0);
  
  /* Only check errors if ec is not 0 (LUA_OK). */
  if (ec!=0) {
    switch(ec) {
      case LUA_ERRRUN:
        ib_log_error(ib, 1, "Error running Lua Rule %s - %s",
                     func_name, lua_tostring(L, -1));

        /* Get error string off of the stack. */
        lua_pop(L, 1);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
      case LUA_ERRMEM:
        ib_log_error(ib, 1, "Failed to allocate memory during Lua rule.");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
      case LUA_ERRERR:
        ib_log_error(ib, 1, "Error fetching error message during Lua rule.");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
#ifdef LUA_ERRGCMM
      /* If LUA_ERRGCMM is defined, include a custom error for it as well. 
         This was introduced in Lua 5.2. */
      case LUA_ERRGCMM:
        ib_log_error(ib, 1, "Garbage collection error during Lua rule.");
        IB_FTRACE_RET_STATUS(IB_EINVAL);
#endif
      default:
        ib_log_error(ib, 1, "Unexpected error(%d) during Lua rule.", ec);
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }
  }
  
  IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @brief Parse a RuleExt directive.
 * @details Register lua function. RuleExt file:/path/to/rule.lua phase:REQUEST
 * @param[in,out] cp Configuration parser that contains the engine being
 *                configured.
 * @param[in] name The directive name.
 * @param[in] vars The list of variables passed to @code name.
 * @param[in] cbdata User data. Unused.
 */
static ib_status_t rules_ruleext_params(ib_cfgparser_t *cp,
                                        const char *name,
                                        ib_list_t *vars,
                                        void *cbdata)
{
  IB_FTRACE_INIT(rules_ruleext_params);
  
  ib_list_node_t* var = ib_list_first(vars);
  
  char* file = NULL;
  char* phase = NULL;
  char rule_name[20];

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
    sprintf(rule_name, "lua:%010d", ironbee_loaded_rule_count);
    add_lua_rule(cp->ib, ironbee_rules_lua, file+4, rule_name);
    
  } else {
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
 * @param[in] vars The list of variables passed to @code name.
 * @param[in] cbdata User data. Unused.
 */
static ib_status_t rules_rule_params(ib_cfgparser_t *cp,
                                     const char *name,
                                     ib_list_t *vars,
                                     void *cbdata)
{
  IB_FTRACE_INIT(rules_rule_params);

  ib_log_debug(cp->ib, 1, "Name: %s", name);

  ib_list_node_t* var = ib_list_first(vars);

  var = ib_list_node_next(var);

  if (cbdata!=NULL) {
    IB_FTRACE_MSG("Callback data is not null.");
  }

  IB_FTRACE_RET_STATUS(IB_OK);
}


static IB_DIRMAP_INIT_STRUCTURE(rules_directive_map) = {

    /* Give the config parser a callback for the directive GeoIPDatabaseFile */
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

  ib_log_debug(ib, 1, "Initializing rules module.");

  if (m==NULL) {
    IB_FTRACE_MSG("Module is null.");
    IB_FTRACE_RET_STATUS(IB_EINVAL);
  }
  
  ironbee_loaded_rule_count = 0;
  ironbee_rules_lua = luaL_newstate();
  luaL_openlibs(ironbee_rules_lua);
   
  IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t rules_fini(ib_engine_t *ib, ib_module_t *m)
{
  IB_FTRACE_INIT(rules_fini);
  ib_log_debug(ib, 4, "Rules module loaded.");
    
  if (m==NULL) {
    IB_FTRACE_MSG("Module is null.");
    IB_FTRACE_RET_STATUS(IB_EINVAL);
  }

  if (ironbee_rules_lua != NULL) {
    lua_close(ironbee_rules_lua);
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

